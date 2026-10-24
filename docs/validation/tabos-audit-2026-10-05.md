# TabOS memory performance and hardware audit

Audit date: October 5, 2026, America/New_York. Source baseline: `534f620`
(`docs: reconcile settled architecture context`), with a clean tracked working tree
at the start.

This focused review traced configuration and boot into runtime scheduling, application
memory and teardown, display DMA/cache ownership, camera buffers, input interrupts,
storage, power controls, and service integration. It combines selected source review
with fresh builds and tests; it is not an exhaustive audit of every source file.

TabOS passes fresh macOS functional and sanitizer checks and builds for the Tab5 in
Debug and Release. It fits its flash partition and has bounded application resources,
but a claim that every application and hardware feature runs safely is not established.
Native application faults can still crash the device. The two hardware error paths
identified at this baseline were subsequently repaired as recorded below; physical
acceptance remains incomplete for those repairs, several services, and panel revisions.
Treat the system as a development platform for trusted applications.

## Reconciliation on October 24, 2026

This report preserves the October 5 baseline and its recorded follow-up repairs.
Both fixes are present in main `4b53ae0`. The later runtime/input/graphics/tooling/compute
changes have separate evidence in [TabOS improvements validation](tabos-improvements.md).
The test counts, firmware sizes, hashes, and measurements below are historical; they
were not rerun or remeasured during this documentation reconciliation. Physical
acceptance remains pending where stated.

## Validation recorded on October 5

| Check | Result |
| --- | --- |
| macOS Debug, ASan and UBSan | All 94 CTest cases passed across the full run and one targeted environment-corrected rerun |
| macOS Release | All 94 CTest cases passed across the full run and one targeted environment-corrected rerun |
| Tab5 Debug with ESP-IDF 5.4.4 | Build and partition-size check passed |
| Tab5 Release with ESP-IDF 5.4.4 | Build and partition-size check passed |
| Default independently compiled RV32 applications | `./apps/build.sh build` completed successfully |
| Maintained SDK tester through actual host RV32 execution | Two full runs, camera child cleanup, and camera/audio/UDP/storage overlap each returned zero and restored the parent shell |

The initial `build.application_tracking` failure in both host configurations was
`ld: library 'System' not found`. Both targeted reruns passed with
`SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`. The parent CMake build
supplies an SDK path; the nested test compiler invocation needed that environment
setting. This was a build-environment failure, not a Tab5 runtime failure.

The RV32 probe reused the maintained Kilo integration harness's storage and input
helpers, loaded the freshly built shell and tester into an isolated temporary root,
and ran `tester one 'two words'`, `tester --camera-cleanup`,
`tester --camera-services`, and the full tester again. It linked the current Debug
core and host backend with ASan/UBSan. Local loopback access was required; the
sandboxed full tester returned nonzero before the unrestricted run passed.
The probe validates host runtime integration and virtual peripherals, not physical
Tab5 media or dual-core timing.

No flash or mass-storage command ran. No firmware or SDK production source was
changed. Linux validation, optional DOOM rebuilding, electrical measurements, and
fresh on-device testing are outside this run. Prior physical results below are
explicitly historical. Third-party dependencies were cross-built and inspected at
relevant interfaces, not independently audited in their entirety.

Commands and logs are retained locally under `build/audit-2026-10-05/`, including
the temporary RV32 probe source and binary. Firmware SHA-256 values are:

- Debug: `af51f329f0ab8de6f3438998822ff379f2124911dc4553789adc1e22c3f70890`.
- Release: `8779c7ce012d720f66fd6b7ba66d0507812bcdb444db5cdcad638a4f3699b607`.

## Findings at the audited baseline

These are source-confirmed error-path gaps. Neither triggering hardware failure was
injected during this audit. P2 denotes a normal defect requiring repair and regression
coverage; it does not imply an observed physical failure on this checkout.

### AUD 047 Display completion waits can block indefinitely

**P2, source repair complete; physical acceptance pending.** Baseline behavior: `platform/esp32p4/display.c:81` waits for VSYNC with `portMAX_DELAY`.
`present_with_ppa()` and `wait_for_ppa()` likewise wait indefinitely for accelerator
completion after successful submission. If the driver accepts work but a completion
interrupt stops arriving, presentation never returns. Software fallback handles an
immediate submission error but cannot handle a missing completion. Boot presentation,
console writes, and graphics calls can therefore stop progressing; a blocked task
allows idle tasks to run, so idle-task watchdog monitoring alone does not guarantee
recovery.

Original repair recommendation (implemented as described below): add bounded
completion waits and an explicit driver fault/quiescence path. A timeout
must not immediately reuse or free buffers that DMA may still own. Publish the display
fault, preserve buffer ownership until the peripheral is stopped, and recover or enter
a controlled failure state. Validate withheld VSYNC/PPA completion, delayed completion,
shutdown after failure, and repeated recovery on Tab5. Normal presentation must remain
free of tearing and borrowed-buffer lifetime violations.

### AUD 048 Camera processing does not validate source buffer lengths

**P2, source repair complete; physical acceptance pending.** Baseline behavior: `platform/esp32p4/camera.c:214` records each mapped buffer's length.
`capture_one_frame():404` checks the dequeued index and flags but does not compare
the payload length or required image footprint with that mapping before processing.
The RAW8 conversion reads `width * height` RGB565 pixels regardless of `bytesused`;
JPEG/H.264 paths forward driver-reported lengths to encoders. Format setup checks
dimensions and pixel format but does not establish a packed stride/size invariant.

A short mapping paired with a completed frame can make RAW8 conversion read past the
mapping; a short payload can expose stale pixels even when the mapping is large enough.
The portable frame pool's destination-capacity check occurs after conversion and
cannot protect those source reads. Normal pinned-driver frames passed previous physical
checks; malformed or partial buffer behavior remains unchecked here.

Original repair recommendation (implemented as described below): validate negotiated
stride and image size, mapped length, dequeued `bytesused`, and
encoder output length before conversion, encoding, or submission. Reject inconsistent
frames with a service error while retaining orderly stop/cleanup. Add production-backend
fakes for undersized mappings, short/oversized payloads, padded stride, and invalid
encoder lengths; then rerun normal formats and cleanup on hardware.

## Follow-up repairs

The findings above describe baseline `534f620`. Both source repairs were subsequently
committed separately at the user's request:

- `f1e2c7e` — `fix(display): bound DMA completion waits`. The production wait helper
  limits VSYNC/PPA completion waits to two seconds. Missing completion logs and enters
  ESP-IDF's fatal/panic path, which defaults to reboot. It never returns to guest code,
  software fallback, or buffer reclamation while DMA ownership is uncertain. The pinned
  PPA API has no supported cancellation; immediate submission errors retain fallback.
- `aaa4d72` — `fix(camera): validate capture buffer bounds`. Format readback establishes
  packed RGB565/YUV420 input. Mapping and completed-payload bounds are checked before
  conversion/encoding, and encoded results must fit their output allocations. Invalid
  frames fault without submission/requeue; ordinary stop retains worker-join cleanup.

Both new regression targets pass in macOS Debug ASan/UBSan and Release. The camera
fix also passes 13 related camera, registry, lifecycle, and architecture checks in each
host configuration; the display fix passed 12 related Debug checks. Both Tab5 Debug
and Release firmware builds pass after each fix, including partition-size checks.
Focused pre-fix probes reproduced the infinite display timeout and an ASan camera
source-buffer overread. Follow-up logs are under `/private/tmp/tabos-display-*` and
`/private/tmp/tabos-camera-*`; these are temporary local evidence.

No flash or mass-storage command ran. Physical withheld/late completion interrupts,
restart behavior, normal DMA presentation, and all-format camera cleanup remain pending.
The baseline sizes and firmware hashes below/above describe the original audit builds,
not the repaired builds. These repairs do not establish hardware acceptance or native
application isolation.

## Memory assessment

Fresh ESP-IDF `size` output measures static placement; remaining linker-region space
is not the free runtime heap after drivers, tasks, and applications start.

| Resource | Debug | Release |
| --- | ---: | ---: |
| Firmware binary | 1,591,040 bytes | 1,488,240 bytes |
| Free space in 2 MiB application partition | 506,112 bytes, 24.1% | 608,912 bytes, 29.0% |
| Static DIRAM use | 265,663 bytes, 46.08% | 252,397 bytes, 43.78% |
| Remaining reported DIRAM region | 310,801 bytes | 324,067 bytes |

The configured device has 16 MiB flash; its application partition is 2 MiB.
Firmware partition fit passed in both builds. Much of the physical flash remains
unpartitioned, and internal-flash `A:` storage is not implemented on Tab5.

At the default 1280×720 RGB565 size, one framebuffer is 1,843,200 bytes. The logical
framebuffer, two native scanout buffers, and PPA scratch buffer total 7,372,800 bytes.
Two overlay backups add 20,480 bytes: approximately **7.05 MiB of display PSRAM** when
acceleration initializes. Default 80×24 terminal state plus 256 history lines needs
approximately 182,520 bytes on RV32, excluding allocator overhead and other console
state.

For 1280×720 RGB565 capture, the three service frame slots add 5,529,600 bytes, and
two packed backend buffers add approximately 3,686,400 bytes. Thus display plus these
camera buffers already account for roughly 15.84 MiB before application heaps,
encoders, driver state, task stacks, and transient ELF loading. RAW8 service slots
currently reserve the same two-byte-per-pixel capacity. This is a bounded allocation
choice, but leaves an opportunity to reduce memory after profiling.

Application defaults are a 256 KiB heap and 16 KiB stack. Metadata permits heaps up
to 16 MiB and stacks up to 256 KiB; loaded images are limited to 1 MiB and ELF files
to 2 MiB. Executable mappings round to 64 KiB pages. Native task stacks use PSRAM;
their task-control blocks remain internal RAM. The pinned RISC-V `StackType_t` is
one byte, so the current stack-depth calculation provides the declared byte budget.
Parents retain their heaps, images, and stacks during nested execution. Individual
limits therefore do not guarantee that every permitted nesting combination fits
32 MiB PSRAM or that a sufficiently large contiguous block remains available.

Heap arenas and camera allocations check allocation failure and roll back owned
resources. Heap guards are checked during process cleanup, not on every write.
FreeRTOS stack canaries, hardware stack guard, PSRAM boot memory testing, brownout
detection, and interrupt/idle-task watchdog monitoring are enabled. Heap poisoning
and task-watchdog panic are disabled in the effective configuration. These settings
detect some faults; they do not establish application isolation or peak-memory safety.

Before hardware acceptance, record free heap, minimum-ever free heap, largest free
block separately for internal RAM and PSRAM, and task stack high-water marks. Measure
boot, full tester, nested heavy applications, repeated media open/close, and simultaneous
camera/audio/network/storage. Require no downward baseline drift after cleanup,
adequate contiguous allocation headroom, and no canary/watchdog failure. No fresh
hardware watermark or long-duration fragmentation result is available from this run.

## Performance assessment

The current runtime blocks on events and exact deadlines rather than a permanent
10 ms tick. Keyboard and touch interrupts notify task-context processing; callbacks
avoid bus operations. Camera capture uses a joined worker, with CPU0 affinity justified
by previous physical corruption when capture migrated between cores. Audio transport
starts on demand. Fresh runtime, interrupt, deadline, concurrency, and cancellation
tests pass.

Console writes still present immediately, and ordinary terminal presentation rotates
the full logical framebuffer. Dirty glyph rendering therefore does not make scanout
submission proportional to changed cells. Repeated short writes and cursor updates
can consume memory bandwidth and VSYNC latency. Fullscreen direct drawing and lower
resolution SDK canvases reduce some work; any further batching should follow on-device
measurement and preserve visible output and panic behavior.

Historical September 5 camera measurements in
[Phase 6 evidence](../../agents/validation/phase6-2026-09-05.md) show:

| Workload | Observed result |
| --- | --- |
| Corrected RGB565 preview | 241 backend frames in 39.67 seconds, about 6.07 frames/s |
| H.264 recording | 30 written pictures in about 17.53 seconds, about 1.71 pictures/s |
| JPEG encoding stage | About 78.95 ms per backend frame |
| Six camera/audio/UDP/storage overlap rounds | 632 ms total, longest round 155 ms |

Configured 30 FPS is a requested sensor rate, not a measured application throughput.
These historical numbers do not establish current firmware speed, sustained audio
quality, or a latency guarantee. Run `gbench` and media stage counters on matching
firmware/applications to establish current FPS, frame latency, input response, and
resource contention. Host interpreter throughput is not a Tab5 CPU benchmark.

## Hardware and functionality assessment

| Area | Current assessment |
| --- | --- |
| Processor and memory configuration | Pinned ESP-IDF 5.4.4, ESP32-P4, 360 MHz CPU, 200 MHz PSRAM, 64-byte cache lines, and 16 MiB flash cross-build successfully |
| Display revisions | Runtime detection supports ILI9881C, ST7123, and ST7121; 1000 Mbps ILI9881C and 965 Mbps ST712x DSI settings match the repository's vendor baseline. Current physical ILI9881C/ST7123 acceptance remains open |
| DMA and interrupts | Aligned PSRAM display buffers, explicit cache synchronization, ISR-only readiness notification, and stopped-task/resource cleanup are present. AUD-047 and AUD-048 source repairs are complete; physical failure-path acceptance remains pending |
| Keyboard and touch | Keyboard GPIO0/1 I2C and IRQ50; touch shared I2C GPIO31/32 and IRQ23 agree with the documented vendor routing. Full touch acceptance across all revisions remains open |
| Storage | Mounted FAT `T:`, no automatic formatting on mount failure, process-owned descriptors, and separate boot MSC ownership. Card-removal recovery and power-loss-atomic replacement are not provided |
| USB power | Boot attempts to disable USB-A host power. Normal boot logs and continues if that operation fails; MSC startup refuses that failure. Safe VBUS state under expander failure is therefore unverified; no physical failure injection or voltage check was performed |
| Charging and battery | Boot enables normal charging and disables fast charging through the documented expander controls. Failed controls are reported, not proven physically safe by telemetry; percentage is voltage-derived rather than a calibrated fuel gauge |
| Audio and networking | Typed services, ownership cleanup, bounded buffers/waits, and host integration pass. Sustained physical audio quality, contention, and native DNS/TLS forced-stop checks remain acceptance work |
| Camera | Preview then operator-triggered snapshot has historical acceptance. Automatic first-frame still capture remains faulty, with prior nearly gray images and recurring expander read errors; full camera acceptance is incomplete |
| Sleep and power | Real CPU light sleep is disabled. Dimming/backlight-off/panel-off retain CPU/services; repeated combined-stage recovery across all panels is unvalidated, and panel-off still leaves DMA/VSYNC running |
| Application faults | Native apps share device privilege/address space; invalid accesses may corrupt the OS or reset the device. The PMP IDRAM split is not per-process isolation |
| Unimplemented hardware features | BMI270 service, runtime USB host HID/storage, expansion/RS-485 APIs, and transparent RTC/IMU/button sleep wake are not complete |

Unknown ST712x firmware versions fall back to an assumed ST7123 driver. That fallback
is visibly logged, but it is not evidence that an unknown panel is supported. Published
board schematics have revision differences, and the recorded physical PCB revision
remains unknown. [Board-routing evidence](../power-routing.md) describes those limits;
source configuration cannot certify electrical compliance for every board population.

## Acceptance priorities

1. Complete physical fault-injection, recovery, and cleanup acceptance for the committed
   AUD-047 and AUD-048 repairs.
2. Keep native applications trusted until a user-mode/PMP boundary and recoverable
   fault path exist. Host fault containment must not be advertised as native containment.
3. Obtain memory/stack watermarks and repeated lifecycle/media soak results on matching
   installed firmware and applications, without treating historical snapshots as peaks.
4. Complete panel/touch and combined idle-power acceptance on each supported revision;
   verify USB power and charger-control failure behavior before claiming fail-safe boot.
5. Retain the camera automatic-still and I2C limitations in user-facing guidance until
   physical reruns establish their resolution.

The current automated evidence supports development use and the tested portable
contracts. Unconditional hardware safety and complete Tab5 functionality remain
unproven for the reasons above.
