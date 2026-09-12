# Power baseline and measurement

TabOS currently supports orderly reboot, shutdown, and automatic idle dimming. Transparent
suspend is not implemented. Blocking the runtime task does not put the board to sleep:
other tasks, peripheral clocks, DMA, and external devices continue operating.

This baseline began at `740ba1d` and now includes Phase 1 through Phase 3 implementation
status. Rebuild and record fresh identity whenever code, SDK, components, configuration,
board revision, or attached peripherals change.

## Suspend admission rule

The inventory below is a prerequisite for the future manager, not a runtime registration
API. **Blocking** means no tested reversible suspend/resume lifecycle exists, or work
cannot yet be drained. **Inactive** means the indicated operation is not started in the
normal idle-shell scenario; its initialized parent can still block suspend. **Safely
suspendable** requires a tested reversible lifecycle. No initialized hardware participant
currently meets that standard. Missing or unknown lifecycle support must reject suspend;
absence of application handles and an ESP-IDF PM lock are not sufficient evidence.

The current firmware has no system-suspend entry point. Future registration must cover
all blocking rows, including idle services, before enabling one.

## Participant inventory

Paths below are relative to the repository. Managed component paths are under
`targets/tab5/managed_components/`; SDK paths are under `.local/esp-idf/components/`.
The resolved component lock is `targets/tab5/dependencies.lock`.

| Participant / source | Initialized or ongoing work; dependencies | Classification and missing lifecycle |
|---|---|---|
| Runtime, process manager, ELF tasks (`kernel/runtime.c`, `process/process.c`, `platform/esp32p4/executable.c`) | Runtime waits on task notifications; each native process has its own task and may execute or block in an API. Parent stacks, executable PSRAM aliases, heaps, descriptors and foreground identity remain live. | Blocking: admission freeze, safe-point parking and in-flight accounting absent. A task blocked on stdin is not a proven suspend safe point. |
| Console, repeat, finite waits, network retry (`console/console.c`, `input/input.c`, `time/time.c`, `net/network.c`) | Absolute monotonic deadlines dispatched centrally; current cursor half-period 600 ms. No separate power tick. | Blocking: suspend deadline suppression and retained-input ordering absent. Pure retained timer state needs no hardware teardown. |
| Device registry / health (`kernel/hardware_devices.c`) | 60-second audit of keyboard, RTC, battery and storage; event-driven state copies for other services. Audit includes shared-bus and storage access despite no app handles. | Blocking: suppress during suspend and run one overdue audit after resume. |
| Shared system I2C (`bsp_i2c_init`, `esp_driver_i2c`) | BSP I2C1, SDA31/SCL32, transaction IRQ/driver synchronization. Used by expanders, touch, RTC, battery, codecs, camera control and future IMU. | Blocking: drain every dependent operation before bus preparation; preserve devices, speed, pulls and timeout state. |
| Keyboard (`platform/esp32p4/keyboard.c`, `keyboard_interrupt.c`) | Separate I2C0 SDA0/SCL1, STM32 at 0x6D, active-low GPIO50. ISR records INPUT readiness; task drains event/status registers with bounded recheck. Boot Delete sampling is a separate finite 750 ms window. | Blocking: held keys, pending reports and unvalidated sleep arming. Preserve keyboard supply and controller mode. Idle I2C reads are absent by source design. |
| GPIO ISR service (`platform/esp32p4/gpio_interrupt.c`) | One boot-lifetime service, flags 0; keyboard and touch register individual handlers in serialized initialization. | Blocking as wake infrastructure until arm/disarm contract tested. Each consumer removes only its handler; never uninstall shared service. |
| Touch (`platform/esp32p4/pointer.c`, `touch_interrupt.c`) | GT911 0x14/0x5D or ST712x 0x55, I2C1, active-low GPIO23. ISR records POINTER readiness; bounded task-context drain, contact matching and cancellation. | Blocking: active contacts or unvalidated controller sleep/wake. Preserve first report, touch supply and reset levels. No idle report polling. |
| LCD scanout / backlight (`platform/esp32p4/display.c`, BSP, `esp_lcd`) | Detected ILI9881C/ST7123/ST7121; MIPI DSI/DPI scanout, PSRAM framebuffers, display LDO, LEDC GPIO22, VSYNC ISR gives semaphore even at idle. | Blocking: no reversible blank/quiesce/restore preserving scanout buffers. Fullscreen ownership blocks all suspend requests. |
| PPA / PIE (`platform/esp32p4/display.c`, `pie.c`) | PPA SRM/fill/blend clients initialized at display startup; completion IRQ, DMA/cache synchronization and semaphore. PIE is synchronous CPU work. | PPA operations inactive at unchanged idle frame, but display parent blocks; unfinished accelerator work always blocks. |
| Audio codecs / I2S (`platform/esp32p4/audio.c`, BSP, `esp_codec_dev`) | ES8388 0x10 and ES7210 0x40 on I2C1. I2S clocks GPIO30/27/29, output26/input28; TX/RX DMA. Codec device handles remain discovered but closed with zero streams. First open configures both codecs and starts `tabos_audio`; last close joins worker exit and closes both codecs. | Inactive and demand-driven with zero streams; any open stream remains a suspend blocker. Physical codec-clock/current shutdown requires validation. |
| Headphone / speaker route (`platform/esp32p4/audio.c`) | Speaker-routed first open samples expander 0x43 P7 before enabling output, then `tabos_headphones` retains 50 ms two-sample debounce. Headphone/microphone routing or last-stream close stops monitoring and disables speaker. | Inactive with zero streams or non-speaker route. Known shared-I2C read errors during active monitoring remain separate work. |
| PI4IO expanders / power rails (`platform/esp32p4/power.c`, BSP) | 0x43 controls speaker, extension 5V and LCD/touch/camera resets; 0x44 controls C6, USB-A 5V, board power-off and charging. I2C1 shared by every control operation. | Blocking: preserve output levels; never use board power-off pulse as suspend. USB-A remains safe-off in normal boot. |
| INA226 / charger (`platform/esp32p4/power.c`) | INA226 0x41 configured for continuous conversion (0x4527), rail telemetry read on demand; charger enabled and fast charge disabled at boot. No INA226 IRQ handler. | Blocking: no reversible monitor/charger policy yet. Conversion and charging consume power independently of runtime wakes. |
| RTC (`platform/esp32p4/rtc.c`) | RX8130 at 0x32, calendar reads/writes and health access on I2C1. No RTC alarm programming or interrupt handler. | Blocking for shared-bus drain; alarm operation inactive/unsupported. Keep wall clock independent from monotonic time. |
| Camera control / CSI / ISP (`platform/esp32p4/camera.c`, BSP, `esp_video`, `esp_cam_sensor`, `esp_ipa`) | SC2356 through SC202CS-compatible driver initialized at boot; I2C1 control, MCLK36, MIPI CSI, ISP/video resources. `tabos-camera` created on CPU0, notification-blocked without stream. ISP pipeline owns additional worker/queue/statistics IRQ work when enabled by video lifecycle. | Blocking even when capture inactive: sensor/controller power and reversible restore not validated. Streams, DMA buffers, frame leases and outstanding work block. |
| Camera stream / encoders (`platform/esp32p4/camera.c`, `esp_h264`, `esp_driver_jpeg`) | On-demand V4L2 buffers and CSI DMA; worker dequeue with 2-second stall watchdog; RAW conversion/preview, JPEG hardware or H.264 encoder, completion IRQs and cache operations. H.264 capacity wait resumes after lease release. | Inactive before first stream; blocking whenever opened or draining. Joined stop exists but full retained-service suspend does not. |
| Storage (`fs/filesystem.c`, `platform/posix/storage.c`, `platform/esp32p4/storage_backend.c`, FatFs/SDMMC) | Mounted T: microSD, SDMMC GPIO39–44, transaction IRQ/DMA, filesystem state/handles; I/O and health can occur outside runtime. | Blocking: spinlock spans I/O; no admission freeze, full drain or storage-sync contract preserving descriptors. Unmount/shutdown is not reversible suspend. |
| C6 power / hosted transport (`platform/esp32p4/runtime.c`, `esp_hosted`) | C6 powered at platform boot. SDIO 4-bit 40 MHz GPIO8–13, reset15; driver IRQ/DMA. `tabos_wifi_start` does asynchronous reset/firmware negotiation and Wi-Fi init. | Blocking: disconnect alone does not quiesce SDIO/C6; hosted deinit/reinit must be validated with preserved config and reconnect intent. |
| Hosted workers / RPC (`esp_hosted/host/drivers`) | `sdio_rx_buf`, `sdio_read`, `sdio_process_rx`, `sdio_write`, `rpc_rx`, `rpc_tx`; queues/semaphores/SDIO interrupt waits, bounded flow-control retries and RPC timeout timers. Packet statistics disabled in resolved config; heartbeat API exists but no call found in current TabOS startup. | Blocking even offline or without sockets. Do not infer no bus traffic from no runtime NETWORK events; capture actual worker activity and C6 firmware behavior. |
| TCP/IP, Wi-Fi and application network operations (`platform/posix/network.c`, `socket.c`, `tls.c`) | ESP event loop and lwIP TCP/IP task/timers; connection/retry events; on-demand `tabos_net_ops`, `tabos_sockets`, `tabos_tls` workers, socket readiness and DNS/echo/TLS operations. | Blocking for open connections, pending operations, startup/reconnect or unsupported worker lifecycle. No API admission freeze exists. |
| USB MSC (`platform/esp32p4/usb_storage.c`, TinyUSB) | Inactive in normal shell boot. Delete selects exclusive pre-mount SD export, USB OTG interrupts, TinyUSB worker and transfer buffers; fallback/eject restart logic. | Inactive normally; blocking whenever exported. USB export has a separate boot loop, not a suspended shell. |
| Flash/NVS, PSRAM, clocks, FreeRTOS/SDK (`esp_hw_support`, `esp_system`, `esp_timer`) | Boot flash/cache and executable PSRAM, both cores, scheduler tick/idle/IPC tasks, esp_timer dispatch, watchdogs, dynamic SDK timers. Flash/NVS writes possible in driver lifecycle. | Blocking pending pinned-SDK sleep entry, both-core coordination, watchdog and retention validation; no clock/domain shutdown allowed in Phase 0. |
| BMI270 / sensor_hub, expansion, RS485, general USB host | Components may be resolved, but no TabOS IMU stream/motion service, general USB-host service or RS485 initialization exists. Physical external devices can still draw power. | Inactive software; unavailable as wake sources. Any future initialized driver without lifecycle support becomes blocking. |
| Host SDL/POSIX backend | SDL event wait, display/audio callbacks, host socket/TLS/DNS workers and camera completion worker; RV32 instruction slices while guest runnable. | Blocking for simulated suspend until deterministic manager model exists. Never use host process CPU or host power as Tab5 electrical evidence. |

Known audit prerequisites remain separate repairs: native task quiescence, Wi-Fi
partial-init unwind, headless audio progress and recoverable FAT replacement. Historical
shutdown success or a passing host suite does not prove these paths safe. Refresh this
inventory after integrating audit fixes, especially host worker and process lifetimes.

## Existing PM locks

No direct `esp_pm_lock_*` calls exist in TabOS platform sources. `CONFIG_PM_ENABLE`
is off in the Phase 0 resolved build, so conditional SDK locks are not active evidence
of suspend safety. The relevant pinned driver implementations contain:

| Driver source | Lock when PM enabled / lifetime |
|---|---|
| `esp_driver_i2c/i2c_common.c`, `i2c_master.c` | Clock-dependent lock selected by bus clock; held during transactions. |
| `esp_driver_i2s/i2s_std.c`, `i2s_common.c` | Clock-dependent APB/no-light-sleep lock, acquired on channel enable and released on disable. Check selected clock before enabling PM. |
| `esp_driver_sdmmc/src/sdmmc_transaction.c` | `ESP_PM_APB_FREQ_MAX` across SDMMC transactions; shared host serves SD and C6. |
| `esp_lcd/dsi/esp_lcd_panel_dpi.c` | `ESP_PM_CPU_FREQ_MAX` acquired when DPI panel is created, released on deletion. |
| `esp_driver_ppa/src/ppa_core.c` | `ESP_PM_CPU_FREQ_MAX` around accelerator work. |
| `esp_driver_jpeg/jpeg_common.c`, `jpeg_encode.c` | `ESP_PM_CPU_FREQ_MAX` around codec operation. |

CSI/ISP and H.264 still require explicit quiescence even where no PM lock is visible.
These locks constrain automatic PM/DFS; they do not implement TabOS admission checks or
make a direct `esp_light_sleep_start()` safe. Re-audit conditional drivers when PM is enabled.

## Wake routes and availability

The [M5Stack pin map](https://docs.m5stack.com/en/core/Tab5) includes ST7121 touch
on GPIO23. Published schematic nets have now been visually traced; see
[board-routing evidence](power-routing.md) for connector pins, interrupt conditioning,
power-controller connections and revision limits. Keyboard and touch worked on the
attached ST7121 unit after the Phase 0 GPIO fix, confirmed by the operator with `hello`
and `touchtest`. This confirms normal input operation, not sleep/wake retention.

RTC/BMI270 use conditioned `E_TRG` / `nINT_STAT_TRIG` into PMS150G PA6. No dedicated
P4 runtime interrupt connection is shown for RTC, IMU or the power button. The
controller's `BOOT_GPIO35` connection is a boot-strap connection, not evidence of a
transparent wake protocol. PCB revision is recorded as `unknown`; exact matching and
circuit-specific RTC/IMU/power-button wake are explicitly deferred. See the
[routing scope](power-routing.md#deferred-circuit-specific-features).

| Source | Proven current path | Transparent light-sleep status |
|---|---|---|
| Keyboard | STM32 Normal mode → active-low GPIO50 → INPUT event → bounded I2C drain/clear/recheck. See [keyboard protocol](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1241/Tab5_Keyboard_User_Manual_EN.pdf) and platform source. | Candidate only. SDK GPIO wake is separate from edge ISR delivery. Retain keyboard supply, arm low-level wake, reject already-low line, test first press/release. |
| Touch | GT911/ST712x → GPIO23 → POINTER event → controller report drain. | Candidate only, per controller/board revision. Keep touch powered; verify report survives sleep and input ordering after wake. |
| RTC alarm | RX8130 shared I2C; vendor documents power-controller E_TRG route. | Unavailable: no alarm driver or proof of light-sleep resume; power-on/reset does not qualify. |
| BMI270 motion | Vendor documents shared E_TRG route; normal TabOS IMU service absent. | Unavailable: no sensor lifecycle, motion arming, attribution or retained resume proof. Remains opt-in in future policy. |
| Physical power button | Vendor describes single press for power-on and double press for power-off. | Unavailable as transparent wake until actual routing/behavior verified. Preserve hardware power/reset semantics; invent no GPIO mapping. |
| SDK timer | Pinned SDK provides timer wake independent from the external RX8130. | Diagnostic candidate for first controlled sleep test; no sleep entry implemented. Application timeout is not a future system-wake alarm. |

All sources remain unarmed. No blanket wake support is advertised. Functional board test
must distinguish continuing after sleep (same process/stack/PSRAM markers) from running
boot code again. Record reset reason, sleep return code, wake cause and retained markers.

## Pinned ESP-IDF restrictions

Use local v5.4.4 source, not latest documentation for another chip. Primary references:
[sleep guide source](https://github.com/espressif/esp-idf/blob/v5.4.4/docs/en/api-reference/system/sleep_modes.rst),
[sleep implementation](https://github.com/espressif/esp-idf/blob/v5.4.4/components/esp_hw_support/sleep_modes.c),
[P4 capabilities](https://github.com/espressif/esp-idf/blob/v5.4.4/components/soc/esp32p4/include/soc/soc_caps.h),
and the local resolved `targets/tab5/sdkconfig`.

- Light sleep preserves internal state; deep sleep discards the normal process runtime.
  Keep flash, PSRAM and digital peripheral power retained in the initial profile. Do not
  infer that retained power allows running DMA during sleep.
- GPIO wake uses level-sensitive `gpio_wakeup_enable()` and
  `esp_sleep_enable_gpio_wakeup()`. Ordinary GPIO edge handlers do not arm wake. Retaining
  the digital domain is essential to the proposed GPIO23/GPIO50 profile; deeper domain
  shutdown has different pin restrictions and remains disabled.
- The P4 code forces flash domain ON in AUTO for revisions below v1.0, and reports an
  error for a forced flash-off request there. ECO0 handling keeps LP peripheral power
  on to avoid EFUSE_CRC reset. Actual eFuse revision must be recorded; configured minimum
  revision is not the actual silicon revision.
- Retain the pinned flash/PSRAM leakage and GPIO-reset workarounds. P4 capability flags
  also select systimer-stall and timer-group-watchdog workarounds. Do not bypass SDK sleep
  entry or copy workarounds from another ESP32 model.
- PSRAM retention must include data, application stacks and executable aliases, with
  cache coherency and both cores verified after repeated sleep. No such physical proof
  exists for this revision yet.
- Current PM is disabled, CPU is 360 MHz, FreeRTOS tick is 100 Hz, PSRAM is 200 MHz.
  Future PM/tickless configuration is separate work. Leave automatic light sleep disabled
  so normal task blocking cannot bypass service admission. DFS is a separate optimization.
- SDK light-sleep code compensates monotonic time; future TabOS must not add sleep time
  twice. RX8130 UTC remains independent. Test overdue timers once after another wake.

### Phase 0 SDK audit conclusion

The pinned SDK contract audit is complete. Actual retained-stack/PSRAM and repeated
sleep/wake tests remain the Phase 7 hardware gate; they are not prerequisites for
recording the Phase 0 platform limits. No experimental sleep is needed to close this
source-audit item.

`esp_hw_support/Kconfig` keeps flash power-down disabled by default and disallows its
selection for a configured P4 minimum below v1.0. Flash and PSRAM CS pull-up workarounds
protect retained memory while other pins can float. The attached board's boot reports
P4 v1.3; the current build still selects minimum revision 0.1, so do not change revision
selection based on one unit or remove compatibility workarounds.

`esp_driver_gpio/src/gpio.c:gpio_wakeup_enable()` accepts only low/high levels and
writes the pin interrupt type. It disables sleep-pad selection for an armed wake pin
when the GPIO-reset workaround applies. `gpio_wakeup_disable()` does not restore the
original edge type. Future suspend preparation must retain GPIO50/GPIO23 runtime edge
configuration and restore it on both resume and aborted entry, with pending-level
recheck. This is distinct from installing their ordinary ISR handlers.

`sleep_modes.c` corrects `esp_timer` from RTC elapsed ticks after successful light sleep;
a rejected/too-short sleep does not count as a successful wake. P4 ECO0 LP-peripheral
retention and pre-v1.0 flash restrictions remain SDK-owned. The separately selected
revision-3 MSPI workaround is not applicable to the observed v1.3 unit or this minimum
revision configuration. Do not select another revision's workaround manually.

## Shared GPIO service ownership

`tab5_gpio_interrupt_add()` owns the first global install with flags 0. Boot/service
initialization is serialized; this helper is not callable concurrently or from an ISR.
Successful installation lasts for the boot, including a consumer failure or teardown.
A failed installation can be retried by the next consumer; an unexpected preinstalled
external service is an ownership error, not silently adopted with unknown flags.

Touch controller constructors receive a null component callback. TabOS attaches GPIO23
itself after successful construction and checks the result. This avoids the pinned touch
component's repeated global install and ignored registration result. Constructors still
configure controller-specific GPIO edges. Task drain and post-attach level recheck retain
pending reports. Shutdown disables/removes each handler before releasing its owner.

The service is deliberately not registered as IRAM-safe: an `IRAM_ATTR` entry alone does
not prove its entire notification call graph and data are cache-independent. Future
interrupt consumers must use this owner and audit any change to allocation flags.

## Debug peripheral activity counters

Debug firmware prints `Platform activity:` immediately after the existing 60-second
`Runtime wakes:` report. It adds no task, timer or deadline. Release builds compile out
counter updates. The portable platform hook is diagnostic only; host reports no physical
Tab5 values.

| Field | Meaning |
|---|---|
| `ms` | Monotonic timestamp of snapshot |
| `audio_chunks` | Completed codec write/read pairs |
| `audio_frames` | Frames in completed pairs at the current sample rate |
| `audio_errors` | Failed codec write/read pairs |
| `headphone_reads` | Worker headphone-read attempts; boot's initial sample excluded |
| `headphone_errors` | Failed worker reads |
| `vsync` | Display refresh completion ISR calls |
| `ppa` | PPA transaction completion ISR calls |

Counters are boot-lifetime unsigned 32-bit values. Subtract successive values modulo
2^32 and divide by timestamp difference; keep intervals short enough to avoid multiple
wraps. Counter loads form an approximate snapshot, not a coherent driver-state transaction.
ISR updates use compile-time-verified lock-free atomics in internal RAM. These counters
measure completed work, not CPU load, energy, open stream counts or proof of suspend
safety. Zero runtime AUDIO readiness can coexist with continuous codec transfers.
Diagnostics add small overhead; use the same build when comparing workloads and measure
Release power separately once the behavior is established.

## Reproducible measurement worksheet

Copy this worksheet for every run. Never fill an unmeasured value with zero.

| Identity / setup | Record |
|---|---|
| UTC date, operator, run ID | Pending |
| `git rev-parse HEAD`, dirty diff, target/config | Pending |
| SDK tag + commit, dependencies.lock, resolved sdkconfig | Pending |
| Firmware SHA-256 and boot-reported version | Pending |
| Actual P4 revision, PCB revision, display/touch, keyboard FW, C6 FW | Pending |
| Battery/external source, voltage, charge/fast-charge state, temperature | Pending |
| Brightness, shell/app, cursor interval, Wi-Fi state/AP/RSSI | Pending |
| Attached keyboard, SD, USB, headphone, expansion peripherals | Pending |
| Meter/model, supply insertion point, calibration, sample rate | Pending |
| Workload, settling time, start/end monotonic timestamps, duration | Pending |

Build and retain identity from the worktree being tested:

```sh
./tools/tabos macos debug test
./tools/tabos tab5 debug build
git rev-parse HEAD
git diff --binary
git -C .local/esp-idf describe --tags --always
git -C .local/esp-idf rev-parse HEAD
shasum -a 256 targets/tab5/dependencies.lock targets/tab5/sdkconfig build/tab5-debug/TabOS.bin
```

Save the lock, resolved configuration and build log with the run. Boot serial output
supplies chip/display/keyboard/C6 identity; use the platform display initialization
line, since the current boot-report display field has a known lifetime defect. Do not
substitute a previous run. Use
`./tools/tabos tab5 debug monitor` with `ESPPORT` set when needed. Record whether the
monitor reset the board. Run `devices` and `battery` before the quiet interval, then let
the shell settle for 60 seconds. Capture at least two consecutive 60-second
`Runtime wakes:` records with no input, then a separate typing/touch workload.

Counters are cumulative since runtime initialization: subtract the two endpoints and
divide by their elapsed seconds. INPUT/POINTER/etc. count coalesced dispatcher readiness,
not physical edges or bus transactions; multiple bits can share one wake. Bracketed
fields count expired owners and runnable application slices. Cursor at 600 ms predicts
about 100 expirations per minute only while visible/owned; measure the actual result.
Host RV32 slicing is not native P4 task activity.

| Observable | Baseline | Later isolated change | Method / interpretation |
|---|---|---|---|
| Runtime total and each source; deadline owners | Pending | Pending | Endpoint delta / elapsed seconds |
| Audio codec transfers / worker CPU time | 6,000 chunks / 60 s quiet baseline | 199 chunks after one two-second speaker tone; consistent with no continuous idle transfer | `audio_chunks` / `audio_frames` deltas; separate task trace for CPU time |
| Headphone reads / errors | 1,200 reads / 60 s quiet baseline | 41 reads after one two-second speaker tone; consistent with active-route-only monitoring | `headphone_reads` / `headphone_errors` deltas; trace I2C1 for transaction timing |
| LCD VSYNC / PPA completions | Pending | Pending | `vsync` / `ppa` deltas; runtime wake counter does not count scanout interrupts |
| Keyboard/touch I2C transfers at idle and activity | Pending | Pending | Bus trace; isolate shared-bus headphone and health traffic |
| C6 SDIO, RPC, network/ISP/SDK worker CPU and timer activity | Pending | Pending | Task/bus trace; identify every unexplained wake |
| Health audit / cursor expiration | Pending | Pending | Runtime deadline-owner deltas; correlate audit hardware reads |
| Current min/mean/max, average watts, energy | Pending | Pending | External meter at recorded supply point; keep charger/source comparable |
| Input-to-delivery and wake-to-visible latency | Pending | Pending | Correlated input edge/display or application marker |
| Retained processes, descriptors, buffers, elapsed/UTC clocks | Not yet applicable | Pending sleep implementation | Before/after identity and data checks; no reboot allowed |

Continuous INA226 battery-rail readings are useful diagnostics, not whole-system metered
power evidence, especially with external power or charging. Measure dimming, idle audio
stop, headphone monitoring changes, display quiescence and full sleep separately with
identical setup. Do not claim current savings from runtime wake reductions alone.

Phase 3 measurement uses a generic inline USB meter at the Tab5 USB-C input, reading 5.12 V.
Battery is absent and charging disabled; keyboard and SD are attached, USB-A is connected to
an unpowered host, and Wi-Fi is connected. After two seconds stable, idle shell at 75%
brightness varies from 0.07 A to 0.09 A and 20% dimmed idle reads 0.04 A. This is coarse
whole-system evidence. Unknown meter accuracy/resolution and the short sampling window still
prevent precise energy or isolated Phase 3 savings claims. Available equipment cannot
intercept the battery-only path, so current validation is explicitly limited to USB-C input.
Battery telemetry may diagnose rail behavior but cannot replace that external measurement.

For the initial physical GPIO fix, boot with keyboard and touch, verify no duplicate
service-install error, rapid input/chords/repeat, touch down/move/up and retouch, and
orderly reboot. Repeat on each supported display revision. Do not enter experimental
sleep until participant lifecycle and rollback gates have been implemented and tested.
