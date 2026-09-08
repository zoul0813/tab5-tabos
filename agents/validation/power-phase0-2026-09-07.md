# Power Phase 0 evidence — 2026-09-07

## Scope and status

Source audit and GPIO ownership fix on `feature/power`. No sleep entry, wake-source
registration, PM enablement, CPU-frequency change, or public API added. The software
baseline includes physical boot identity and live runtime/peripheral counters. Power/peripheral
setup is operator-confirmed. User accepts PCB revision `unknown`, defers exact physical
matching and circuit-specific wake features, and closes Phase 0 with those limitations.
All ten checklist items are complete under that accepted scope. Instrumented current
measurements and retained sleep/wake tests remain later-phase evidence.

See [power baseline and measurement procedure](../../docs/power-baseline.md) for the
participant inventory, SDK restrictions, wake-route evidence, and worksheet.

## Build identity

- Source base: `740ba1d3043c9f3db95aec7f1e3b881f53eca683`, plus this Phase 0 diff.
- SDK: ESP-IDF v5.4.4, `296b6eab9445fd720e71aecab961e2d3fbca9944`.
- Built and flashed Debug image version: `740ba1d-dirty`; final diagnostic image boot
  ELF digest prefix `2f6559d9a`, compiled September 7 2026 07:44:52.
- Target: ESP32-P4, Debug; CPU 360 MHz, PSRAM HEX 200 MHz, flash 16 MB.
- Configured minimum chip revision: `CONFIG_ESP32P4_REV_MIN_FULL=1`; actual silicon revision **v1.3**, observed at boot.
- PM disabled; peripheral power-down disabled; FreeRTOS 100 Hz; automatic light sleep not configured.
- Cursor half-period 600 ms; host presentation configured at 58 Hz.

| Artifact/input | SHA-256 |
|---|---|
| `targets/tab5/dependencies.lock` | `27496361ee0533fbf032c879cefae48b45da8c8d0a9d620f9f2df3b6b2f70681` |
| `targets/tab5/sdkconfig.defaults` | `ac6f74aa7e7be1d147c1073cc2706de3397d646550ec7d676f3e3700303b3b46` |
| `targets/tab5/sdkconfig` | `ac16a65223ce7ab2f4fe0037b917ad9badc400757c8f769d46772a10c09b9d2c` |
| `build/tab5-debug/TabOS.bin` | `203625e29a3cc30833a088cf4ebfbca0975580832dbc075e8c471bdc8a3ae523` |

## Resolved components

Exact versions from `targets/tab5/dependencies.lock`, not manifest version ranges.
Resolution does not imply runtime initialization (for example BMI270 and sensor_hub).
ESP-IDF built-in drivers are fixed by the SDK commit above.

| Component | Version |
|---|---|
| `espressif/bmi270` | `1.1.0` |
| `espressif/cmake_utilities` | `0.5.3` |
| `espressif/eppp_link` | `1.1.6` |
| `espressif/esp_cam_sensor` | `2.0.1` |
| `espressif/esp_codec_dev` | `1.5.11` |
| `espressif/esp_h264` | `1.0.4` |
| `espressif/esp_hosted` | `1.4.7` |
| `espressif/esp_io_expander` | `1.2.1` |
| `espressif/esp_io_expander_pi4ioe5v6408` | `1.0.1` |
| `espressif/esp_ipa` | `1.3.1` |
| `espressif/esp_lcd_ili9881c` | `1.1.0` |
| `espressif/esp_lcd_st7121` | `1.0.1` |
| `espressif/esp_lcd_st7123` | `1.0.2` |
| `espressif/esp_lcd_touch` | `1.2.1` |
| `espressif/esp_lcd_touch_gt911` | `1.2.0~3` |
| `espressif/esp_lcd_touch_st7123` | `1.0.2` |
| `espressif/esp_sccb_intf` | `0.0.9` |
| `espressif/esp_serial_slave_link` | `1.1.2` |
| `espressif/esp_tinyusb` | `2.2.1` |
| `espressif/esp_video` | `2.0.1` |
| `espressif/esp_wifi_remote` | `0.8.5` |
| `espressif/i2c_bus` | `1.5.2` |
| `espressif/m5stack_tab5_noglib` | `1.2.0~1` |
| `espressif/sensor_hub` | `0.1.5` |
| `espressif/tinyusb` | `0.21.0~1` |
| `espressif/usb` | `1.4.1` |
| `espressif/usb_host_uvc` | `2.4.2` |
| `idf` | `5.4.4` |

## Physical identity and observations

| Field | Evidence |
|---|---|
| Board serial / PCB revision | Serial not recorded; PCB revision `unknown`, exact matching explicitly deferred by user. |
| Actual P4 silicon revision | P4 v1.3, eFuse v0.3; ROM esp32p4-eco2-20240710 |
| Display / touch | ST7121 / ST712x firmware 1(1.80.1.16), native 720x1280 |
| Keyboard firmware | 1, Normal mode |
| C6 firmware | 1.4.1 |
| Power source / battery voltage / charger state | Operator: battery and USB-C connected. Boot enables charger; actual charging current/state and battery voltage were not measured. |
| Attached SD, keyboard, USB, headphones, expansion | Operator: microSD and Tab5 keyboard attached; no headphones; USB-A connected to host for MSC. |
| Runtime wake counters / interval / worker CPU time | See chronological captures below; worker CPU time remains unmeasured |
| Current / energy / input latency | Pending instruments and physical run |

## Validation in this worktree

- `./tools/tabos macos debug test`: 53/53 pass, including host smoke, ASan/UBSan
  configured host tests, keyboard/touch drain tests, and new GPIO ownership regression.
- `./tools/tabos tab5 debug build`: pass with pinned SDK; final diagnostic image size `0x17ec90`,
  25% free in smallest application partition.
- New GPIO regression: allocation failure, unexpected external service owner, retry,
  handler-add failure with existing consumer preserved, simultaneous delivery, removing
  one consumer while another survives, and reinitialization without global reinstall.
- `./tools/tabos tab5 release build`: pass; image size `0x165bd0`, 30% partition free.
- Debug activity hook: existing health audit emits once; ordinary dispatch emits no
  activity reports. Updated macOS Debug suite passes 53/53.
- Final Debug firmware flashed and boot captured; duplicate global GPIO ISR-install
  error absent. Operator subsequently confirmed keyboard/touch with `hello` and `touchtest`; see
  board-routing continuation below.

## ISR handoff retained

Completed conversion stays completed: GPIO50 keyboard, GPIO23 touch, deadline-driven
repeat/cursor/retry/waits, event-driven network and camera completion, process readiness,
and bounded central dispatch. Prior keyboard and process board evidence is retained in
[ISR milestone](../milestone-isr.md), including 2026-09-05 keyboard testing and
2026-09-06 repeated tester/child-unwind validation. No interrupt conversion repeated.

Outstanding evidence carried forward:

- GT911 and ST7123 board touch coverage; complete per-revision touch gesture matrix.
- Keyboard fault/unplug recovery, ISR setup/teardown under physical faults.
- No-idle-I2C, worker/interrupt rates, latency, watchdog and mixed-load board traces.
- Linux Debug/Release, macOS Release, Tab5 Release, standalone app matrix, and full
  maintained RV32 tester on host and board for the final milestone revision.
- Camera pool-exhaustion timing assertion remains the pre-existing frame-rate-sensitive
  follow-up; this power change does not claim to resolve it.

## Concurrent audit work

The separate `../tabos/agents/audit.md` is authoritative for audit repairs; no copy
exists on this power branch. Status below is a 2026-09-07 snapshot, not a second repair
tracker. Integrate the audit fixes before implementing dependent power phases and repeat
the inventory when worker/lifecycle behavior changes.

| Audit dependency | Power implication |
|---|---|
| AUD-023 | Direct mismatch with `docs/power.md` live battery fault/recovery promise; baseline still has delayed audit refresh. |
| AUD-006 | Native teardown is not a proven join; Phase 4 parking/Phase 6 rollback must consume safe task lifecycle. Historical ISR completion checkboxes do not override this finding. |
| AUD-042, AUD-015 | Wi-Fi partial-init cleanup and portable network synchronization are prerequisites to reversible network suspend. |
| AUD-024 | Headless audio progress must be fixed before using host audio waits as power-manager evidence. |
| AUD-009, AUD-041 | Panic visibility and nested fullscreen ownership affect failure handling and retained foreground state. |
| AUD-038 | Bound RTC epoch conversion before implementing absolute wake alarms. |
| AUD-045 | Establish recoverable FAT replacement before implementing `powerctl save`; temp/sync/rename alone is insufficient on current Tab5 FAT. |
| AUD-002, AUD-003, AUD-005 | Marked resolved in audit worktree; absent from this branch baseline. Integrate wait/graphics/host-I/O fixes rather than duplicate them. |
| AUD-001 | Boot-report display string is unreliable; capture platform driver's own display initialization log. |
| AUD-028, AUD-043 | Potential file overlap in pointer health hook and agent context; Phase 0 does not repair these findings. |

No audit finding covers the shared GPIO ISR-service install fixed here. Merge-sensitive
files include `cmake/TabOSSources.cmake`, `tests/CMakeLists.txt`, agent documentation and
`platform/esp32p4/pointer.c`. Audit worktree files were read only.

## Continuation: attached-board identity and SDK gate

Opened `/dev/cu.usbmodem211201` at 115200 baud with DTR/RTS set false and no reset
command or serial input. A boot sequence with `CHIP_USB_UART_RESET` nevertheless
appeared. Treat opening this USB-UART connection as potentially resetting the board;
this was not a reset-free observation. No firmware was flashed in this capture.

Observed boot identity (separate from the local Phase 0 image above):

- P4 silicon v1.3; eFuse block v0.3; ROM `esp32p4-eco2-20240710`.
- Installed app `dd9031b-dirty`, compiled September 6 2026 11:59:53,
  ELF digest prefix `260f8a0d3`; SDK v5.4.4. Dirty suffix prevents attributing exact
  running source/configuration from the commit alone.
- Flash 16 MB, DIO 80 MHz; PSRAM 32 MB, 200 MHz, startup memory test passed.
- CPU 360 MHz, two cores; platform detection ST7121; ST712x touch firmware
  `1(1.80.1.16)` with native 720x1280 bounds.
- Tab5 keyboard firmware 1, Normal mode; C6 hosted firmware 1.4.1;
  SD mounted and Wi-Fi connected; LCD backlight 75%.
- Duplicate global GPIO ISR-service installation error reproduced at boot on this
  older image. This does not validate the new fix until its image is flashed.

The base `dd9031b` source has no `Runtime wakes:` accounting, but the installed dirty
image does emit it. Its local modifications therefore include later runtime work; do
not identify it with clean `dd9031b`. Capture current Phase 0 firmware after board
availability is confirmed; do not overwrite firmware while parallel audit work may
be using the device.

Pinned SDK audit is now closed as a source-evidence item. Added explicit GPIO
edge-restoration requirement and traced PSRAM/flash CS pullups, silicon workarounds,
and monotonic compensation. Physical sleep/PSRAM-retention testing remains Phase 7.
PCB revision, power source and complete peripheral list still need operator input.

### Captured runtime wake interval

Passive capture lasted 130 seconds and completed with port closed. No serial commands
were sent. Full local log: `/tmp/tabos-power-phase0-passive-20260907.log`.
Log SHA-256: `eb67305c7fc77811b263149b5910bbd143b744634faaa4aefcc070711bd1c47e`.

```text
I (63674) tabos: Runtime wakes: total=104 input=0 pointer=0 network=3 camera=0 audio=0 application=1 device=0 deadline=102 [input=0 console=98 network=0 health=1 app=1]
I (123674) tabos: Runtime wakes: total=205 input=0 pointer=0 network=3 camera=0 audio=0 application=1 device=0 deadline=203 [input=0 console=198 network=0 health=2 app=1]
```

Endpoints are boot-relative 63,674 ms and 123,674 ms: 60,000 ms elapsed.

| Counter | Delta | Rate |
|---|---|---|
| Total dispatcher wakes | 101 | 1.6833 / second |
| Deadline readiness | 101 | 1.6833 / second |
| Console deadline | 100 | 1.6667 / second |
| Health audit deadline | 1 | 0.0167 / second |
| Input, pointer, network, camera, audio, application, device readiness | 0 each | 0 in this interval |
| Input/network deadline, application slice | 0 each | 0 in this interval |

These are observed dispatcher counts, not worker/IRQ measurements. They are consistent
with the 600 ms cursor half-period and 60-second health audit. No current-saving claim
is made. Continuous audio I/O, 50 ms headphone monitoring, LCD VSYNC, C6/ISP/SDK tasks
still require an independent activity trace or bounded instrumentation. The full runtime
counter/worker checkbox remains open for that reason and for a repeat on identified
Phase 0 firmware.

Wake classification is established conservatively for Phase 0: keyboard/touch are
unarmed candidates; RTC/IMU/power-button transparent wake is unavailable until physical
routing/retained-resume proof exists. Hardware power-on/reset is explicitly excluded.
This closes the distinction item without claiming unsupported paths work.


## Diagnostic firmware and SD application recovery

Debug-only boot-lifetime counters now measure completed codec I/O, headphone worker
reads/errors, display VSYNC and PPA completions. They share the existing health-audit
report; no new timer or periodic task was added. Release compiles out counter updates.

The first final-image boot (`/tmp/tabos-power-phase0-activity-boot.log`) loaded the SD
shell, which returned 127 and panicked process 0. The shell CRT uses 127 for incompatible
ELF API; this branch exports API 20 while the SD held September 6 apps from another
firmware. That capture and the earlier `/tmp/tabos-power-phase0-flashed-boot.log` must
not be used as normal idle-shell baselines. The diagnostic panic interval nevertheless
confirmed counters work; it does not establish healthy application behavior.

Operator entered boot MSC on September 7. Preserved all 32 existing default-app binaries
and their SHA-256 manifest in `.local/power-phase0/sd-app-backup-20260907-081603/`.
Forced `./apps/build.sh -B --msc` rebuilds against this worktree's SDK headers before
installing matching apps and requesting full eject. Existing custom files and DOOM are
outside that install set. DOOM remains from the previous firmware and requires its own
matching rebuild before use.

### Healthy shell baseline after matching-app install

`./apps/build.sh -B --msc` completed successfully, rebuilt all default applications,
installed them through MSC and requested full eject. Shell binary SHA-256:
`006b98c99f812b4723e7a367e5646be2376beac0ed85861dfdfe3bb6a2f58b87`.

A 190-second capture explicitly reset the board using USB-UART reset, then sent no
serial input. Shell loaded 66,617 memory bytes, platform run loop started, and Wi-Fi
connected. No process-0 panic, status-127 return or duplicate GPIO ISR-service install
error appeared. The serial connection was closed after capture. Reset is expected on
this setup and is not transparent wake evidence.

Capture: `/tmp/tabos-power-phase0-matching-apps-boot.log`. SHA-256:
`59967d7d5c008f75992e079a612acd8a6105553f6a254a1a9dd27618ca923df1`. Local durable copy, source/build
SHA-256 manifest, tracked diff, firmware, resolved config and dependency lock are under
`.local/power-phase0/final-diagnostic/`.

```text
I (63689) tabos: Runtime wakes: total=102 input=0 pointer=0 network=3 camera=0 audio=0 application=1 device=0 deadline=101 [input=0 console=98 network=0 health=1 app=1]
I (63689) tabos: Platform activity: ms=62086 audio_chunks=6080 audio_frames=2681280 audio_errors=0 headphone_reads=1216 headphone_errors=0 vsync=3945 ppa=103
I (123689) tabos: Runtime wakes: total=203 input=0 pointer=0 network=3 camera=0 audio=0 application=1 device=0 deadline=202 [input=0 console=198 network=0 health=2 app=1]
I (123689) tabos: Platform activity: ms=122086 audio_chunks=12080 audio_frames=5327280 audio_errors=0 headphone_reads=2416 headphone_errors=0 vsync=7873 ppa=203
I (183689) tabos: Runtime wakes: total=304 input=0 pointer=0 network=3 camera=0 audio=0 application=1 device=0 deadline=303 [input=0 console=298 network=0 health=3 app=1]
I (183689) tabos: Platform activity: ms=182086 audio_chunks=18080 audio_frames=7973280 audio_errors=0 headphone_reads=3616 headphone_errors=0 vsync=11800 ppa=303
```

Both adjacent intervals are 60,000 ms. The second interval starts after 120 seconds
of boot settling; both show the same rates except a one-count VSYNC variation.

| Observable | First / second interval delta | Rate |
|---|---|---|
| Runtime total / deadline readiness | 101 / 101 each | 1.6833/s |
| Cursor deadline | 100 / 100 | 1.6667/s |
| Health deadline | 1 / 1 | 1/60 s |
| All other readiness/deadline counters | 0 / 0 each | 0 in these intervals |
| Codec completed write/read pairs | 6,000 / 6,000 | 100/s |
| Codec frames | 2,646,000 / 2,646,000 | 44,100/s |
| Headphone worker reads | 1,200 / 1,200 | 20/s |
| VSYNC callbacks | 3,928 / 3,927 | 65.4667 / 65.45 per second |
| PPA completions | 100 / 100 | 1.6667/s |
| Codec and headphone errors | 0 / 0 each | No errors observed |

These observations close the named Phase 0 runtime/worker baseline. They are not
whole-system task CPU time, complete IRQ/bus traces or electrical power measurements.
C6/SDK/ISP activity, idle keyboard/touch bus traffic and input latency remain separately
tracked in the measurement worksheet and ISR handoff. Physical setup details and
matching-PCB routing remain open; keyboard/touch interaction requested after capture.

Audit merge-sensitive files additionally include `kernel/runtime.c`, platform header,
host runtime, `audio.c`, `display.c`, fake platform and `core_smoke.c` because of the
small Debug diagnostic hook. No parallel audit repairs were implemented here.

## Board-routing continuation and physical input confirmation

Operator confirmed keyboard/touch worked while running `hello` and `touchtest` after
the matching-app install. Record this as successful ordinary input on the tested
ST7121 unit. It does not establish fault recovery, exhaustive gestures, other revisions
or sleep/wake retention. No further board reset, flash, alarm/motion programming or
power-button experiment was needed for this continuation.

[Board-routing evidence](../../docs/power-routing.md) now records visual inspection of
published schematic pages 1, 2, 4 and 5, connector/package pins, conditioned E_TRG path,
PMS150G endpoints, source hashes and software cross-checks. Local source PDFs and
rendered pages are preserved in `.local/power-phase0/routing/`. Browser PDF rendering
failed; local PDFKit/PyMuPDF rendering resolved the actual line connections instead
of inferring them from text order.

The two published exports differ in the IMU supply/interface circuit. Neither supplies
a physical revision match for this unit; silicon/display identity is insufficient.
No dedicated P4 runtime interrupt is shown for RTC/IMU/button. GPIO35's boot-strap
connection is not registered as a speculative wake source. Transparent support remains
unavailable pending hardware/controller evidence and later retained-resume validation.

The routing checklist stays open only for matching the physical PCB to the schematic.
Published tracing and normal physical keyboard/touch evidence are complete. Power
source and full attached-peripheral inventory remain the other Phase 0 open item.
This continuation changes documentation only; `git diff --check` passes. Existing
53/53 macOS Debug tests and Tab5 Debug/Release builds remain the code validation.

## Operator-confirmed power and peripheral setup

Operator identified the testing setup as battery plus USB-C connected, no headphones,
Tab5 keyboard as the expansion accessory, and microSD attached. USB-A was connected
to the host for MSC. This records the operator's connector description; it does not
infer USB electrical role or continued MSC operation during the later shell baseline.
MSC had been ejected before that capture. Battery voltage, actual charging current,
charge level and exact USB-A cable state throughout the quiet interval were not logged.

This closes the Phase 0 identity/setup checklist item. Nine of ten items are complete.
Exact physical PCB matching is the remaining gate. Deferring that match while keeping
circuit-specific wake features unavailable has been proposed; the operator's setup
answer does not itself accept that change to the gate.

## Accepted Phase 0 closure

User explicitly accepted recording PCB revision as `unknown` and deferring
circuit-specific wake features. This supersedes the proposed/open PCB gate in the
chronological entries above. All ten Phase 0 checklist items are now complete under
the accepted scope; no previously missing electrical evidence is claimed as measured.

Deferred features are transparent light-sleep wake from RX8130 RTC alarm, BMI270
motion, and the physical power button/PMS150G controller. Exact PCB matching and
controller/supply behavior must be established before implementing those paths.
Revision-dependent IMU supply or isolation optimizations also remain unverified.

Keyboard GPIO50 and touch GPIO23 remain candidates for later coordinated light sleep,
subject to driver lifecycle and per-variant resume tests. Portable policy, dimming and
service quiescence can proceed. Existing reboot/shutdown and ordinary RTC/input
behavior are unchanged. No sleep path or new default was enabled by this closure.

Documentation-only closure validated with `git diff --check`; no code or firmware
changed after the recorded successful builds and tests.
