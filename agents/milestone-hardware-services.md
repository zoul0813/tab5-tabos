# TabOS Hardware Services Milestone Roadmap

## Summary

Build hardware support as staged OS services. Preserve Application ABI v1,
platform boundaries, foreground-process rules, and host parity. Hardware remains
OS-owned; applications receive capability-gated, portable APIs.

Touch deferral changes: raw normalized touch/pointer support moves before GUI
work. GUI, gestures, and window management remain deferred.

## Core Architecture

- Add a fixed-capacity device registry with stable-per-boot IDs, logical names
  such as `touch0`, device class, capabilities, presence, readiness, error state,
  and driver description.
- Never reuse device IDs during one boot. Publish add, remove, ready, offline,
  and fault events.
- Add `<tabos/wait.h>` wait sets. Each item contains a process-owned source token,
  requested readiness flags, and returned flags. Support zero-time poll, finite
  monotonic timeout, and infinite wait.
- Keep subsystem reads typed. Wait sets signal readiness; callers then use input,
  device, audio, camera, sensor, or socket APIs.
- Use bounded queues with documented overflow behavior. Input keeps newest events;
  loss-sensitive streams report overrun.
- Foreground ownership continues governing keyboard, pointer, touch, and fullscreen
  graphics. Background processes may use explicitly granted noninteractive services.
- Process exit closes handles, cancels waits, releases media buffers, stops streams,
  closes sockets, and removes subscriptions.
- Drivers run through portable service boundaries. ESP-IDF, FreeRTOS, SDL,
  controller protocols, and native handle types remain private.
- Do not add permanent core affinity. Add affinity only after measurements.
- Extend the private ELF API table by appending entries while retaining its existing
  prefix and version so old ABI-v1 binaries continue running.
- Add a feature-query entry for newer SDK calls. Unsupported features return `ENOSYS`.
- Add bounded `.note.tabos` ELF metadata containing metadata version, required
  Application ABI, requested hardware capabilities, and optional stack/heap limits.
- Legacy executables without metadata retain current ABI-v1 baseline services. New
  hardware services require a declared capability.
- External applications cannot receive raw bus access yet. Raw GPIO, I2C, SPI, and
  UART access remains built-in/system-only until a future trust model exists.

## Milestone 0: Baseline and Documentation

- Update architecture, context, and testing documents.
- Replace the touch-deferred requirement with the decision to expose raw normalized
  touch before GUI work.
- Update `agents/roadmap.md` and keep hardware services ordered as below.
- Finish deterministic clipping and framebuffer tests for the current graphics API
  before adding accelerated paths.
- Record the current host and Tab5 Debug/Release build and test baseline.

Acceptance: documents agree, current tests pass, and old sample binaries still run.

## Milestone 1: Device Registry, Capabilities, and Wait Sets

- Add public device enumeration and lookup by ID or name.
- Add a device lifecycle event source.
- Add a process-owned wait-source table integrated with process cleanup.
- Add ELF metadata generation to SDK build rules and validation to the loader.
- Add capability grants for device enumeration, pointer input, RTC, power read and
  control, sensor read, audio playback and capture, camera, networking, and future
  raw I/O.
- Extend the boot report from registry state instead of separate ad hoc platform
  diagnostics where practical.
- Add a shell `devices` command showing name, class, state, driver, and capabilities.

Acceptance: the host registers deterministic virtual devices, capability denial
returns `EACCES`, stale handles fail safely, multiple wait sources wake correctly,
and legacy ABI-v1 applications remain compatible.

## Milestone 2: Touch, Pointer, and USB Host Input

- Add `<tabos/pointer.h>` instead of changing the existing keyboard event structure.
- Define down, move, up, and cancel events with device ID, contact ID, logical
  1280x720 coordinates, button state, and optional normalized pressure.
- Normalize rotation and controller differences inside the Tab5 backend.
- Support GT911, ST7123, and ST7121 touch paths using the runtime-detected controller.
- Map SDL mouse to contact 0 and SDL touch contacts to stable contact IDs.
- Add a USB host service for HID keyboard and mouse. Feed keyboards into the existing
  normalized keyboard queue and mice into the pointer queue.
- Allow the built-in I2C keyboard and USB HID devices simultaneously.
- Keep USB-A VBUS off until the host service explicitly starts. Existing boot-time
  USB MSC device mode remains mutually exclusive.
- Add USB mass-storage enumeration and mount the first device as `U:`, then `V:`
  through `Z:`. Require clean unmount on removal where possible.

Acceptance: synthetic host input matches hardware-independent semantics, all three
Tab5 touch revisions receive physical validation, USB hotplug does not disturb the
built-in keyboard, and process focus prevents background input consumption.

## Milestone 3: RTC, Battery, Power, and IMU

- Add a UTC wall-clock API backed by RX8130CE and the host clock. The kernel stores
  UTC only; timezone belongs in user configuration.
- Keep monotonic time independent from wall-clock changes.
- Add INA226 telemetry: bus voltage, signed current, power, charging or external-power
  state, and validity flags. Battery percentage remains optional unless calibration
  proves reliable.
- Add BMI270 typed samples for acceleration and angular velocity plus a motion-event
  source.
- Add system power states: active, idle, suspending, suspended, and shutting down.
- Implement orderly shutdown: stop applications, flush filesystems, stop services,
  then power off.
- Add wake-source registration for RTC alarm, power button, keyboard, and IMU motion.
- Defer deep sleep until every mounted filesystem and active driver implements
  suspend/resume. Initial idle mode dims the display and reduces polling.
- Add shell `date`, `power`, and `sensors` diagnostics.

Acceptance: wall-clock changes never alter monotonic timers, simulated low-battery
and wake events work on the host, physical telemetry values are plausible, shutdown
leaves microSD clean, and service tasks remain responsive.

## Milestone 4: Accelerated Graphics and Display Scheduling

- Keep the current RGB565 public drawing contract.
- Route fill, copy, rotation, scaling, color conversion, and supported blend operations
  through portable graphics backend capabilities.
- Add ESP32-P4 PPA and 2D-DMA acceleration with a pixel-identical software fallback.
- Add damage regions and asynchronous present completion as a waitable source.
- Keep display ownership in the OS. Fullscreen applications never receive MIPI-DSI
  or framebuffer-driver ownership.
- Benchmark framebuffer placement, cache behavior, memory bandwidth, and present
  latency before selecting buffering policy.

Acceptance: accelerated and software framebuffer hashes match, terminal restoration
remains exact, graphics load does not starve input, timers, or filesystem services,
and Tab5 measurements document the chosen buffering strategy.

## Milestone 5: Audio Service

- Add an OS-owned mixer and typed playback and capture streams.
- Use signed 16-bit little-endian PCM at 48 kHz, mono or stereo, as the initial
  mandatory format.
- Use bounded ring buffers, nonblocking I/O, waitable readable/writable state,
  underrun/overrun counters, and per-stream volume.
- Implement an SDL3 host backend and Tab5 ES8388/ES7210 backend.
- Support speaker, headphone, and dual-microphone routes. Expose AEC only when the
  backend reports that capability.
- Stop and release streams automatically on process exit.

Acceptance: at least four playback streams mix, capture and playback coexist, route
changes do not leak handles, host waveform tests pass, and physical speaker,
headphone, microphone, and AEC behavior receive separate checks.

## Milestone 6: Camera and Media Pipeline

- Add camera discovery and capability negotiation for SC2356.
- Use a kernel-owned bounded frame pool with opaque leased-frame handles.
  Applications inspect metadata and copy or read frame contents; direct OS-buffer
  pointers remain unavailable in ABI v1.
- Require explicit frame release and reclaim all leases on process exit.
- Support raw capture first, then ISP conversion, JPEG, and H.264 through ESP32-P4
  accelerators.
- Expose frame readiness through wait sets. Apply backpressure by dropping the oldest
  unleased capture frame while incrementing a drop counter.
- Permit display, encoder, and storage consumers to share internal frames without
  exposing hardware ownership.

Acceptance: a deterministic host fixture drives the capture pipeline, leaked frames
are reclaimed, slow consumers cannot exhaust kernel memory, and physical capture,
preview, JPEG, and bounded H.264 recording work without starving input.

## Milestone 7: ESP32-C6 Networking

- Use a bounded BSD-style API: IPv4 and IPv6 sockets, TCP, UDP, DNS, connect, bind,
  listen, accept, send, receive, shutdown, and socket options needed by normal apps.
- Use TabOS wait sets instead of exposing `select()` internals.
- Keep socket handles process-owned and automatically closed.
- Implement the host backend with native sockets.
- Use the official ESP-IDF 5.4.4-compatible ESP32-C6 hosted transport matching Tab5
  wiring. If the pinned stack lacks working support, stop after an isolated transport
  proof and record the blocker; do not create a custom protocol.
- Add network link and address events plus shell `netctl` diagnostics.
- Keep Wi-Fi credentials volatile during this roadmap. Persistent credential storage
  waits for security design.

Acceptance: host loopback and DNS tests pass, disconnect and reconnect preserve
unrelated services, child cleanup closes sockets, and Tab5 completes DNS, TCP, and
UDP tests.

## Milestone 8: Expansion and Industrial I/O

- Register RS-485, M5-Bus, HY2.0, GPIO expansion, and detected modules as devices.
- Add a high-level serial/RS-485 service with baud, framing, direction control, and
  switchable termination.
- Add a typed sensor service for timestamped scalar or vector samples and capability
  metadata.
- Keep arbitrary raw bus operations unavailable to external applications.
- Let built-in drivers claim buses and addresses exclusively; reject conflicts
  deterministically.
- Add hotplug and state events where hardware detection permits them.

Acceptance: host fake devices test enumeration, contention, disconnect, queue
overflow, and cleanup; physical RS-485 loopback and at least one expansion sensor pass.

## Milestone 9: Low-Power Integration

- Move eligible monitoring and wake coordination to ESP32-P4 low-power facilities
  only after functional services stabilize.
- Suspend applications cooperatively, reject suspend while unsafe storage or media
  operations remain active, quiesce drivers in dependency order, and resume in
  reverse order.
- Use RTC, power button, keyboard, and BMI270 as supported wake sources.
- Preserve the monotonic-time contract across sleep by accounting for elapsed RTC time.
- Measure power before and after each optimization.

Acceptance: repeated suspend and resume preserve filesystem integrity, wall clock,
device registry transitions, input, and foreground process behavior; measured power
savings justify enabled paths.

## Test and Documentation Requirements

- Add unit tests for registries, ID lifetime, capability grants, metadata parsing,
  wait timeouts, cancellation, queue overflow, ownership, and cleanup.
- Add component tests using host fake devices for every public service.
- Extend `apps/tester` with separate modules for device, wait, pointer, RTC, power,
  sensor, audio, camera, and network APIs.
- Run the same independently built RV32 tester artifact on host and Tab5.
- Verify old ABI-v1 fixtures unchanged after every transport extension.
- Build and test macOS Debug/Release, Linux Debug/Release, and Tab5 Debug/Release at
  each milestone.
- Add subsystem-specific hardware checklists. A hardware-dependent milestone remains
  incomplete until checked.
- Update relevant `docs/` files whenever commands, APIs, configuration, or behavior land.
- Keep roadmap, context, architecture, and testing agent documents synchronized after
  each milestone.

## Fixed Decisions and Deferrals

- Application ABI remains v1; the private transport grows compatibly.
- Raw touch and pointer APIs land before GUI work. GUI, gestures, focus hierarchy,
  compositor, and window manager remain later.
- Security, signed applications, secure boot policy, flash encryption, and key
  provisioning remain outside this roadmap.
- Capability gating provides resource policy, not an authenticity or isolation claim.
- External applications receive high-level services only; raw hardware access remains
  unavailable.
- Persistent Wi-Fi credentials wait for security design.
- Host behavior must match portable semantics; electrical behavior remains
  hardware-tested.
- Existing filesystem, shell, process, console, graphics ownership, and multi-target
  requirements remain mandatory.
