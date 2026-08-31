# Hardware Services Milestone

## Goal

Move existing and future TabOS hardware drivers behind one portable, discoverable,
process-owned service model. Applications use typed public APIs, generic wait sources,
and stable logical device names without declaring service access in advance. Platform code keeps
ESP-IDF, FreeRTOS, SDL, native handles, buses, and controller-specific behavior private.

This document is the durable task list for work performed across multiple sessions.
Complete phases in dependency order and mark an item only after its associated tests and
documentation pass.

## Status Rules

- `[x]` means implementation, relevant automated tests, and required documentation are complete.
- `[ ]` means pending work.
- Hardware validation remains unchecked until performed on physical Tab5 hardware.
- At the end of every work session, update this checklist and `agents/roadmap.md`.
- When a public command or API changes, update the relevant file under `docs/` in the same session.
- Do not mark an entire phase complete while any acceptance item in that phase remains open.

## Fixed Decisions

- [x] Build the device registry and generic wait foundation before adding new hardware services.
- [x] Migrate existing display, keyboard, storage, Wi-Fi, RTC, and battery drivers into the registry.
- [x] Allow every application to use any available driver or service without declaring capabilities.
- [x] Replace the socket-specific wait API with a generic wait-source API.
- [x] Treat the application ABI as pre-release and freely change it when the design improves.
- [x] Rebuild all bundled applications whenever the ABI changes; no compatibility shim is required.
- [x] Register present/detected devices only; do not publish speculative entries for absent hardware.
- [x] Keep device IDs stable and non-reusable for the duration of one boot.
- [x] Implement RTC, battery, and IMU after the foundation, followed by audio, touch, camera,
  expansion I/O, and low-power integration.
- [x] Put small user-facing hardware diagnostics in `apps/coreutils`.
- [x] Defer USB host HID and mass storage until after camera support.
- [x] Keep GUI, gestures, compositor, window manager, raw bus access, and security policy out of scope.
- [x] Report device feature flags for discovery only; they do not grant or deny application access.

## Current Baseline

- [x] RTC and UTC wall-clock APIs exist on host and Tab5.
- [x] Battery telemetry and charge-control APIs exist.
- [x] Reboot and orderly shutdown paths exist.
- [x] Display revision detection supports ILI9881C/GT911, ST7123, and ST7121.
- [x] Keyboard, microSD, graphics, networking, TLS, and socket services exist.
- [x] Socket-only zero, finite, and infinite wait behavior exists.
- [x] PPA and PIE graphics acceleration foundations exist.
- [x] Process-owned file, socket, TLS, graphics, heap, and execution resources have cleanup paths.
- [x] Fixed-capacity internal device registry model and deterministic registry tests exist.
- [ ] Record a fresh macOS Debug/Release, Linux Debug/Release, Tab5 Debug/Release, app-build,
  and maintained-tester baseline before beginning Phase 1.

## Phase 1: Device Registry

### Registry model

- [x] Add a fixed-capacity registry with an initial capacity of 32 devices.
- [x] Define a generation-tagged 32-bit `tabos_device_id_t` and invalid sentinel.
- [x] Never reuse an ID during one boot, including after removal.
- [x] Retain an internal tombstone after removal so stale IDs fail deterministically.
- [x] Reject duplicate logical names.
- [x] Define device classes for display, keyboard, storage, RTC, battery, IMU/sensor, network,
  audio, pointer/touch, camera, and expansion I/O.
- [x] Define ready, offline, and fault states.
- [x] Store ID, class, logical name, driver name, state, device feature flags, and last error.
- [x] Keep logical names deterministic: `display0`, `keyboard0`, `storage0`, `rtc0`, `battery0`,
  `imu0`, `wifi0`, `audio0`, `touch0`, and `camera0`.
- [x] Register a device only after physical or virtual presence is detected.
- [x] Permit a detected device to transition among ready, offline, and fault.

### Public device API

- [x] Add `<tabos/device.h>` with device count, indexed lookup, ID lookup, and logical-name lookup.
- [x] Return copied, fixed-size device information; never expose internal pointers or native handles.
- [x] Add lifecycle events for added, removed, ready, offline, and fault transitions.
- [x] Give each process-owned subscription a bounded 32-event queue.
- [x] Drop the oldest event on overflow and report an overflow flag with the next successful read.
- [x] Close subscriptions and discard queued events during process cleanup.
- [x] Add private ELF transport calls and host-interpreter forwarding.

### Existing driver migration

- [x] Register the active display as `display0` after controller detection and initialization.
- [x] Register the built-in keyboard as `keyboard0` only when detected.
- [x] Register mounted microSD storage as `storage0`; transition/remove it on future live-removal support.
- [x] Register the RX8130CE as `rtc0` after successful detection.
- [x] Register INA226/charger support as `battery0` after successful initialization.
- [x] Register ESP32-C6 networking as `wifi0` after hosted transport initialization.
- [x] Preserve each existing platform driver and typed public API; registry integration wraps driver
  lifecycle and discovery rather than moving ESP-IDF code above the platform boundary.
- [x] Route detected driver initialization failures and ESP32-C6 connection recovery into device
  fault/ready state reporting.
- [x] Replace ad-hoc boot-report device state/presence decisions with registry data where practical.
- [x] Keep boot-critical platform diagnostics available before registry-backed report generation.

### User tooling and validation

- [x] Add `devices` to `apps/coreutils`.
- [x] Print ID, logical name, class, state, driver, features, and last error without native details.
- [x] Unit-test capacity, duplicate names, deterministic lookup, state transitions,
  tombstones, stale IDs, removal, and registry reinitialization.
- [x] Unit-test lifecycle-event order, overflow, subscriptions, and owner cleanup.
- [x] Add deterministic host virtual devices.
- [x] Add maintained tester coverage for enumeration, copied lookup, lifecycle-subscription behavior,
  stale handles, and process-owned subscription cleanup.
- [x] Validate `devices` output and maintained device-registry tester coverage on physical Tab5.
- [x] Update boot diagnostics, device API, and utility documentation.

## Phase 2: Generic Wait Sources

### API and ownership

- [x] Replace socket-specific wait items with opaque generation-tagged `tabos_wait_source_t` values.
- [x] Replace `tabos_wait_set()` with `tabos_wait(items, count, timeout_ms)`.
- [x] Define readable, writable, state-changed, error, and hangup event bits.
- [x] Preserve zero-time polling, finite monotonic timeout, and infinite wait.
- [x] Associate every wait source with its owning process and parent service handle.
- [x] Invalidate a source when its parent socket, subscription, stream, or device handle closes.
- [x] Reject foreign-process and stale sources with `EBADF`.
- [x] Cancel process waits before closing owned resources during cleanup.
- [x] Ensure cancellation wakes native platform workers and does not leave stale responses.

### Source adapters and migration

- [x] Add socket-to-wait-source conversion.
- [x] Add device-subscription wait sources.
- [x] Add reusable internal hooks for future sensor, audio, pointer, camera, and expansion sources.
- [x] Replace old private transport slots and update the host interpreter.
- [x] Migrate networking service tests and SDK tests.
- [x] Migrate `nettest`, `ntpdate`, IRC, tester, and every other application using socket waits.
- [x] Remove the old public socket-only wait API and compatibility code.

### Phase 2 validation

- [x] Test zero, finite, and infinite waits on each available source type.
- [x] Test mixed socket and device-event waits.
- [x] Test readiness ordering, timeout, spurious wake handling, cancellation, stale sources,
  closed parents, foreign ownership, and process teardown.
- [x] Test disconnect and reconnect around retained parent wait sources.
- [ ] Run wait and networking tests on every supported target.
  - [x] macOS full `tester` run.
  - [ ] Linux full `tester` run.
  - [x] Physical Tab5 full `tester` run.
- [x] Update wait, networking, lifecycle, and SDK documentation.

## Phase 3: RTC, Battery, and BMI270 IMU

### RTC integration

- [ ] Complete physical RX8130CE restart-retention check.
- [x] Validate physical RX8130CE detection, calendar read, and unchanged-value write through maintained tester.
- [x] Preserve UTC storage and separation between wall clock and monotonic time.
- [x] Expose RTC presence and live read/write fault state through `rtc0`.

### Battery integration

- [x] Expose battery presence and live telemetry/control fault state through `battery0`.
- [x] Audit INA226 initialization, validity flags, signed current direction, voltage, power,
  charge state, percentage approximation, and charger-control error propagation.
- [x] Preserve orderly reboot and shutdown paths.
- [ ] Validate telemetry plausibility, charger toggling, external-power behavior, and shutdown on Tab5.

### IMU service

- [ ] Detect and initialize BMI270, then register it as `imu0`.
- [ ] Add `<tabos/sensor.h>` with process-owned sensor stream handles.
- [ ] Define monotonic timestamp, three-axis acceleration in milli-g, and three-axis angular
  velocity in milli-degrees per second.
- [ ] Support rates of 25, 50, 100, and 200 Hz with 50 Hz default.
- [ ] Start hardware sampling only while at least one stream is open.
- [ ] Use bounded per-stream queues that retain newest samples and count overruns.
- [ ] Make reads nonblocking and expose readable generic wait sources.
- [ ] Add deterministic synthetic host IMU data.
- [ ] Close streams and stop unneeded sampling during process teardown.
- [ ] Defer motion wake and threshold events to low-power phase.

### Utilities and validation

- [x] Retain `battery`, `date`, `reboot`, and `shutdown` in `apps/coreutils`.
- [x] Improve `battery status` into the single power diagnostic: report availability,
  battery/external-power source, charge/discharge/full state, charger state, estimated level,
  voltage, signed current, and power with clear labels for unavailable or unknown values.
- [x] Keep normal and fast charger controls under `battery`; report backend and control failures clearly.
- [ ] Add tester coverage for sensor open/read/wait/close, rates, overflow, stale handles,
  unavailable hardware, and cleanup.
- [ ] Validate axis orientation, units, sample rates, and idle behavior on physical Tab5.
- [x] Update RTC, battery, and battery-utility documentation.
- [ ] Update sensor and sensor-utility documentation.

## Phase 4: Audio Service

- [ ] Detect and initialize ES8388/ES7210, then register `audio0`.
- [ ] Add `<tabos/audio.h>` with process-owned playback and capture stream handles.
- [ ] Require signed 16-bit little-endian PCM at 48 kHz as the mandatory format.
- [ ] Support mono and stereo playback and backend-reported capture channel counts.
- [ ] Use bounded ring buffers with nonblocking reads/writes and `EAGAIN`.
- [ ] Expose readable/writable/error/hangup through generic waits.
- [ ] Mix at least four playback streams with saturation and per-stream volume.
- [ ] Support speaker, headphone, and microphone routes.
- [ ] Expose AEC only when the backend reports working support.
- [ ] Implement SDL3 host playback/capture and deterministic fake-buffer testing.
- [ ] Implement Tab5 playback through ES8388 and capture through ES7210.
- [ ] Stop streams, wake waiters, and free buffers on close, exit, failed launch, and process fault.
- [ ] Add `audiotest` to `apps/coreutils` for tone, microphone level, loopback, route,
  and underrun/overrun tests.
- [ ] Validate speaker, headphone, microphones, simultaneous playback/capture, four-stream mixing,
  route changes, latency, underrun, overrun, cleanup, and repeat launch on physical Tab5.
- [ ] Update audio API and utility documentation.

## Phase 5: Touch and Pointer

- [ ] Register detected GT911, ST7123, or ST7121 touch hardware as `touch0`.
- [ ] Add `<tabos/pointer.h>` with down, move, up, and cancel events.
- [ ] Include device ID, contact ID, logical 1280x720 coordinates, buttons, and optional normalized pressure.
- [ ] Normalize rotation and controller-specific coordinates inside the Tab5 backend.
- [ ] Support bounded multi-contact state and queues.
- [ ] Map SDL mouse to contact 0 and SDL touch contacts to stable contact IDs.
- [ ] Restrict pointer consumption to the foreground process through existing focus ownership.
- [ ] Expose pointer readiness through generic waits.
- [ ] Cancel active contacts when focus changes, device disappears, or queue state must reset.
- [ ] Add `touchtest` to `apps/coreutils`.
- [ ] Test synthetic host mouse/touch input, focus ownership, multi-contact behavior,
  overflow, cancellation, waits, stale sources, and cleanup.
- [ ] Physically validate coordinate orientation and input on ILI9881C/GT911, ST7123, and ST7121 revisions.
- [ ] Update pointer/input and utility documentation.

## Phase 6: Camera and Media Pipeline

### Capture foundation

- [ ] Detect SC2356 and register `camera0`.
- [ ] Add `<tabos/camera.h>` for formats, configuration, stream lifecycle, frame acquisition,
  metadata, copying, and release.
- [ ] Use kernel-owned bounded frame pools and opaque generation-tagged leases.
- [ ] Never expose native DMA, ISP, or frame-buffer pointers to applications.
- [ ] Require explicit frame release and reclaim leaked leases on process exit.
- [ ] Drop the oldest unleased frame for slow consumers and increment a drop counter.
- [ ] Expose frame readiness, error, and hangup through generic waits.
- [ ] Add deterministic host frame fixtures.

### Formats and tools

- [ ] Deliver bounded raw capture first.
- [ ] Add RGB preview conversion and fullscreen preview validation.
- [ ] Add JPEG encoding after raw/preview cleanup and throughput tests pass.
- [ ] Add H.264 only after raw and JPEG paths meet memory and service-responsiveness requirements.
- [ ] Add `cameratest` to `apps/coreutils` for detection, still capture, preview,
  file output, and dropped-frame reporting.

### Phase 6 validation

- [ ] Test pool exhaustion, lease reuse, stale handles, slow consumers, drops, cancellation,
  unavailable hardware, process cleanup, and repeat launch.
- [ ] Validate physical capture, preview, JPEG, and optional H.264 without starving input,
  audio, networking, storage, or display service work.
- [ ] Record memory use, frame rate, conversion time, encoding time, and dropped frames.
- [ ] Update camera/media API and utility documentation.

## Phase 7: Expansion and Industrial I/O

- [ ] Detect and register supported RS-485, M5-Bus, HY2.0, and expansion devices.
- [ ] Add typed serial/RS-485 service with baud, framing, direction control, and termination control.
- [ ] Use bounded queues, nonblocking I/O, generic waits, and process-owned handles.
- [ ] Add typed scalar/vector sensor descriptors for supported modules.
- [ ] Enforce exclusive bus/address claims and deterministic conflict errors.
- [ ] Keep arbitrary raw GPIO, I2C, SPI, and UART unavailable to external applications.
- [ ] Add `serialctl` to `apps/coreutils`.
- [ ] Test host fake-device enumeration, hotplug, contention, disconnect, overflow,
  stale handles, waits, and cleanup.
- [ ] Validate physical RS-485 loopback and at least one supported expansion sensor.
- [ ] Update expansion-device and utility documentation.

## Phase 8: Low-Power Integration

- [ ] Define active, idle, suspending, suspended, resuming, and shutting-down states.
- [ ] Add idle display dimming and reduced driver polling before attempting suspend.
- [ ] Add wake-source registration for RTC alarm, power button, keyboard, and BMI270 motion.
- [ ] Define suspend blockers for active filesystem writes, media streams, network operations,
  and unreleased camera frames.
- [ ] Add service suspend/resume callbacks and explicit dependency ordering.
- [ ] Suspend services in dependency order and resume them in reverse order.
- [ ] Reject suspend while an unsafe operation or unsupported active driver remains.
- [ ] Preserve RTC wall time and account for sleep in the monotonic-time contract.
- [ ] Add deep sleep only after all mounted storage and active drivers support suspend/resume.
- [ ] Test simulated idle, blockers, wake events, ordering, rollback after suspend failure,
  and repeated cycles on host.
- [ ] Validate repeated physical suspend/resume without filesystem corruption, lost input,
  stale registry state, broken networking, or foreground-process failure.
- [ ] Record power before and after each enabled optimization.
- [ ] Update power-state and application-behavior documentation.

## Explicitly Deferred

- [ ] USB host initialization, HID keyboard/mouse, and USB mass-storage mounting.
- [ ] Wi-Fi scan, forget, prompted connection, and richer network status fields.
- [ ] Motion-triggered wake before Phase 8.
- [ ] GUI toolkit, gestures, focus hierarchy, compositor, and window manager.
- [ ] External-application raw GPIO, I2C, SPI, or UART access.
- [ ] Signed applications, secure boot policy, and flash encryption.
- [ ] Application memory isolation and recoverable native Tab5 faults.

## Completion Criteria

- [ ] Every present built-in device appears in the registry with stable logical naming.
- [ ] Applications can use every available registered service without a declaration or permission mask.
- [ ] All asynchronous services use generic process-owned wait sources.
- [ ] Existing display, keyboard, storage, RTC, battery, and Wi-Fi behavior remains functional after migration.
- [ ] RTC, battery, IMU, audio, touch, camera, expansion I/O, and low-power phases meet their acceptance checks.
- [ ] Host behavior matches portable semantics for every implemented service.
- [ ] macOS Debug/Release tests, Linux Debug/Release tests, Tab5 Debug/Release builds,
  all application builds, and maintained tester pass.
- [ ] Physical hardware checklists are complete for every implemented Tab5 service.
- [ ] Agent architecture/testing/roadmap documents and user-facing documentation agree with implementation.
