# TabOS Hardware Services Milestone Roadmap

## Canonical Plan

The detailed, itemized, multi-session implementation checklist is:

- [`agents/hardware-services.md`](hardware-services.md)

Use that document to select work, preserve phase dependencies, record progress, and
apply validation gates. Do not duplicate its task-level checklist here.

## Goal

Move existing and future TabOS hardware drivers behind portable OS-owned services.
Applications discover present devices and their features through a common registry,
wait on generic process-owned sources, and use typed APIs instead of
ESP-IDF, FreeRTOS, SDL, controller, bus, or native-handle interfaces.

## Current Baseline

- [x] Display, keyboard, microSD, RTC, battery, reboot/shutdown, graphics acceleration,
  ESP32-C6 networking, TLS, and socket wait support exist.
- [x] Existing application resources have deterministic cleanup paths for normal and
  failed process exit.
- [x] The fixed-capacity device registry model, generation-tagged IDs, tombstones,
  copied lookup, and state transitions are implemented and tested.
- [x] Public copied enumeration and process-owned lifecycle subscriptions are implemented,
  including bounded overflow reporting, cleanup, and ELF forwarding.
- [x] Existing display, keyboard, storage, Wi-Fi, RTC, and battery drivers register and
  report lifecycle state through the common device registry.
- [x] Generic wait-source API, socket adapter, process ownership, stale-handle rejection,
  and teardown cancellation are implemented.
- [ ] Device subscriptions and future asynchronous hardware services still need generic
  wait-source adapters.

## Agreed Phase Order

1. Device registry and migration of existing drivers.
2. Generic wait sources and migration of existing socket wait users.
3. RTC, battery, and BMI270 IMU completion.
4. ES8388/ES7210 audio playback and capture.
5. GT911/ST7123/ST7121 touch and portable pointer input.
6. SC2356 camera and media pipeline.
7. Expansion and industrial I/O.
8. Low-power integration.

USB host HID and mass storage are deferred until after camera work.

## Corrected Decisions

- [DECIDED] The device registry is a prerequisite for new hardware services.
- [DECIDED] Existing drivers migrate into the registry; registry work is not limited
  to newly implemented devices.
- [DECIDED] The registry lists detected/present devices only. Device IDs remain stable
  and are never reused during one boot.
- [DECIDED] Applications may use any available driver or service without declaring
  capabilities or permissions in advance.
- [DECIDED] Device feature flags describe supported operations for discovery; they do
  not grant or deny access.
- [DECIDED] The socket-specific wait API will be replaced by generic process-owned wait
  sources supporting sockets, device events, sensors, audio, pointer input, camera,
  and expansion I/O.
- [DECIDED] The application ABI is pre-release and may change freely while TabOS is
  being defined. Rebuild all bundled applications after an ABI change; do not add
  compatibility shims for unreleased binaries.
- [DECIDED] Persistent Wi-Fi credentials are already supported at `T:/etc/wifi.conf`;
  they are not deferred by this roadmap.
- [DECIDED] Networking, RTC, battery, PPA, PIE, and socket waits are existing services
  to migrate or validate, not unstarted hardware milestones.
- [DECIDED] New service order is sensors/power, audio, touch, camera, expansion, then
  low power. USB host remains deferred.
- [DECIDED] Small diagnostics such as `devices`, `sensors`, `power`, `audiotest`,
  `touchtest`, `cameratest`, and `serialctl` belong in `apps/coreutils`.
- [DECIDED] Raw GPIO, I2C, SPI, and UART access remains unavailable to external apps.
- [DECIDED] GUI, gestures, compositor, window manager, signed-app policy, secure boot,
  and native application isolation remain outside this milestone.

## Cross-Phase Requirements

- Hardware remains owned by TabOS services and platform backends.
- Host implementations must match portable semantics with deterministic virtual devices
  or fixtures where physical behavior cannot exist.
- Every handle, stream, lease, subscription, and wait source is process-owned and cleaned
  during normal exit, failed launch, or process failure.
- Bounded queues must define overflow behavior; loss-sensitive streams report overruns.
- Foreground ownership continues to govern keyboard, pointer, touch, and fullscreen graphics.
- No permanent core affinity is added without measurements demonstrating need.
- Each phase updates architecture, context, testing, roadmap, and affected user documentation.
- Each phase runs macOS and Linux Debug/Release validation, Tab5 Debug/Release builds,
  all application builds, and the maintained RV32 tester.
- Hardware-dependent phases remain incomplete until their physical checks pass.

## Completion

This umbrella milestone is complete only when every non-deferred phase and the completion
criteria in [`agents/hardware-services.md`](hardware-services.md) are checked.
