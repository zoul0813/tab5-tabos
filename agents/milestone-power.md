# TabOS Low-Power Integration Milestone

## Goal

Add measurable low-power behavior without breaking TabOS process continuity,
filesystem integrity, device-registry identity, foreground input, or service ownership.
Implement portable policy and deterministic host simulation above narrow platform
adapters. Keep ESP-IDF, FreeRTOS, SDL, and Tab5 wiring details below the platform
boundary.

This document expands Phase 8 of [`agents/hardware-services.md`](hardware-services.md).
That checklist remains the source of completion status.

## Current Baseline

- [x] Deferred orderly reboot and power-off already stop applications, close resources,
  unmount storage, stop services, and perform the platform action at a safe runtime boundary.
- [x] RTC wall time and application-visible monotonic time already exist on host and Tab5.
- [x] Device registry IDs remain stable and non-reusable for one boot.
- [x] Audio, pointer, camera, network, filesystem, display, and input services have explicit
  ownership and cleanup paths that can supply suspend-blocker state.
- [x] Tab5 display initializes brightness support and enables the backlight at 75 percent.
- [ ] No portable power-state manager or coordinated service suspend/resume path exists.
- [ ] Tab5 keyboard remains polled; its interrupt mode is disabled.
- [ ] RX8130 alarm programming and wake routing do not exist.
- [ ] BMI270 initialization, sampling, and motion wake remain incomplete Phase 3 work.
- [ ] Expansion I/O remains incomplete Phase 7 work; every future active driver must join
  the suspend contract before deep sleep can be enabled.

## Core Decisions

- [DECIDED] `suspended` initially means transparent light sleep. RAM, process state,
  registry IDs, service handles, and foreground ownership remain intact.
- [DECIDED] Deep sleep is not transparent suspend. ESP32-P4 deep-sleep wake runs the
  bootloader and application startup again after losing CPU context. Treat later deep
  sleep as orderly reboot-style hibernation unless process persistence is deliberately
  designed.
- [DECIDED] Implement idle dimming and reduced polling before coordinated light sleep.
- [DECIDED] Portable power policy must not expose ESP-IDF, FreeRTOS, SDL, GPIO, or native
  driver types.
- [DECIDED] Suspend depends on explicit service participation. An active service or driver
  without suspend support blocks the transition.
- [DECIDED] Suspend dependents before their dependencies. Resume dependencies before their
  dependents. Rollback uses the same resume path.
- [DECIDED] Existing reboot and power-off requests outrank pending idle or suspend work.
- [DECIDED] Missing or unverified wake hardware remains unavailable and is never
  registered speculatively.
- [DECIDED] Power improvements require before/after measurement. A successful build alone
  is not evidence of lower consumption.

## Power-State Contract

Use these portable states:

```text
ACTIVE --inactivity--> IDLE --timeout/request--> SUSPENDING
   ^                      |                          |
   | activity             | blocker                  | success
   +----------------------+                          v
                                                SUSPENDED
                                                    |
                                                    | wake
                                                    v
                                                 RESUMING
                                                    |
                                                    v
                                                  ACTIVE

Any running state --reboot/power-off--> SHUTTING_DOWN
```

State behavior:

- `active`: normal brightness, normal service cadence, applications runnable.
- `idle`: dimmed display and reduced safe polling; processes remain runnable.
- `suspending`: new unsafe operations are rejected, application reaches a safe point,
  blockers are checked, and services quiesce in ordered callbacks.
- `suspended`: platform is in resumable light sleep.
- `resuming`: platform and services restore before foreground event delivery resumes.
- `shutting-down`: existing orderly reboot or power-off path owns the runtime.

Transition rules:

- Any keyboard or pointer activity returns `idle` to `active` and restores brightness.
- A blocker keeps automatic suspension in `idle`; a future explicit request reports a
  stable busy reason.
- A callback failure resumes every already-suspended service, restores normal operation,
  records the failed service and error, and returns to `active`.
- Once `suspending` starts, services must not admit new work that would invalidate the
  completed preflight.
- Wake input is retained but not delivered to the foreground process until resume finishes.
- Reboot or power-off accepted during a transition cancels suspend progress and proceeds
  through the existing orderly shutdown path.

## Implementation Plan

### Slice 1: Portable power manager

- [ ] Add a portable internal power subsystem with current state, transition reason,
  last-activity timestamp, timeout policy, and last transition error.
- [ ] Add fixed-capacity service registration with stable names, dependencies,
  preflight/blocker queries, suspend callbacks, and resume callbacks.
- [ ] Validate registrations at startup: reject duplicate names, missing dependencies,
  cycles, and capacity overflow deterministically.
- [ ] Compute one stable dependency order at initialization rather than hardcoding a
  second runtime shutdown list.
- [ ] Add platform operations for brightness, sleep preparation, light-sleep entry,
  wake-cause collection, and resume completion.
- [ ] Add deterministic host controls for synthetic activity, time advancement, callback
  failure, and wake injection.
- [ ] Keep first slice internal. Add public ABI only when an application use case requires
  explicit suspend, status, or a process-owned inhibitor.

### Slice 2: Idle policy

- [ ] Record user activity when normalized keyboard or pointer events enter TabOS.
- [ ] Add configurable idle-dim and suspend-delay values with conservative defaults.
- [ ] Add a display brightness operation that dims without destroying framebuffer or
  display ownership and restores the prior active brightness.
- [ ] Make fullscreen graphics ownership inhibit display dimming.
- [ ] Make active media streams inhibit automatic display dimming where user-visible
  playback or capture requires it.
- [ ] Reduce keyboard, pointer, battery, and other periodic polling only where latency and
  correctness remain acceptable.
- [ ] Restore brightness before delivering the activity that ended idle state.
- [ ] Measure idle draw before and after display dimming and each polling reduction.

### Slice 3: Suspend blockers and admission freeze

- [ ] Define stable internal blocker reasons and diagnostic names.
- [ ] Add filesystem mutation accounting around writes and metadata-changing operations.
- [ ] Freeze new filesystem mutations during `suspending`, wait a bounded time for active
  operations, then synchronize mounted storage.
- [ ] Do not block light sleep merely because a read-only descriptor is open.
- [ ] Block on active audio streams until explicit pause/suspend semantics exist.
- [ ] Block on active camera streams and separately identify unreleased frame leases.
- [ ] Block on DNS, echo, socket, TLS, Wi-Fi transitions, and other in-flight network work.
- [ ] Initially block on open sockets because connection continuity across sleep is not
  promised. Later replacement may close them with defined hangup behavior.
- [ ] Block while foreground native application is outside a known cooperative safe point.
- [ ] Block on every active registered driver that lacks suspend/resume support.
- [ ] Ensure process teardown releases any process-owned blocker or inhibitor state.

### Slice 4: Ordered suspend, resume, and rollback

Use this initial dependency shape, refined during callback inventory:

```text
foreground application
    -> camera / audio / network
    -> filesystem / storage
    -> pointer / input wake preparation
    -> display / backlight
    -> platform light sleep
```

- [ ] Park foreground application at a defined safe point without violating persistent
  nested-process semantics.
- [ ] Quiesce camera and audio work.
- [ ] Stop new network work and place ESP32-C6 transport into a defined recoverable state.
- [ ] Freeze and synchronize filesystem/storage.
- [ ] Cancel active pointer contacts and arm input wake paths.
- [ ] Blank display and disable backlight only after all user-visible services quiesce.
- [ ] Enter platform light sleep.
- [ ] Resume platform clocks and buses, then services in reverse dependency order.
- [ ] Resume foreground application last.
- [ ] Inject failure at every callback position and prove complete rollback.

### Slice 5: Initial Tab5 light sleep

- [ ] Enable required ESP-IDF power-management configuration and FreeRTOS tickless idle.
- [ ] Audit each active peripheral for dynamic-frequency-scaling and light-sleep behavior.
- [ ] Use ESP-IDF power-management locks only inside Tab5 platform drivers that genuinely
  require CPU/APB frequency or prohibit light sleep.
- [ ] Preserve required GPIO levels across sleep where peripheral power-down would
  otherwise float pins.
- [ ] Add keyboard interrupt-driven wake and stop depending on 10 ms polling while asleep.
- [ ] Confirm power-button wiring and semantics before advertising it as a wake source.
- [ ] Report wake cause to portable power manager without exposing ESP-IDF enums.

### Slice 6: RTC and BMI270 wake

- [ ] Add RX8130 alarm programming, flag clearing, cancellation, and absolute UTC deadline
  validation below the platform boundary.
- [ ] Confirm RTC interrupt routing can wake ESP32-P4 on Tab5 hardware.
- [ ] Complete normal BMI270 detection and sampling work from Phase 3 before motion wake.
- [ ] Add bounded BMI270 motion threshold configuration and interrupt clearing.
- [ ] Confirm BMI270 interrupt routing and false-wake behavior on physical hardware.
- [ ] Register RTC and motion wake sources only when detected, initialized, routed, and
  armed successfully.

### Slice 7: Time semantics

- [ ] Guarantee that `tabos_time_monotonic_ms()`, `tabos_monotonic_ms()`, and
  `CLOCK_MONOTONIC` include time spent in light sleep.
- [ ] Keep wall-clock UTC sourced from RX8130 on Tab5 and host clock simulation on hosts.
- [ ] Make elapsed finite waits become ready after resume when their deadlines passed.
- [ ] Define periodic-timer behavior so long sleep does not create callback storms or
  replay every missed interval.
- [ ] Make host simulator advance synthetic monotonic time by the simulated sleep duration.
- [ ] Treat deep-sleep boot as a new monotonic epoch unless future persisted-time policy
  explicitly defines otherwise.

### Slice 8: Deep-sleep gate

- [ ] Do not enable deep sleep until every mounted storage backend and active driver has a
  tested quiesce/shutdown path.
- [ ] Define deep sleep as reboot-style hibernation, including boot reason and user-facing
  application behavior.
- [ ] Unmount storage and use orderly service shutdown before entering deep sleep.
- [ ] Never promise restoration of foreground process memory, handles, sockets, streams,
  or registry IDs across deep-sleep boot.
- [ ] Add deep sleep only after light-sleep measurements show a concrete remaining need.

## Host Validation

- [ ] Test every legal transition and reject every illegal transition.
- [ ] Test activity timestamp updates, dimming, timeout cancellation, and brightness restore.
- [ ] Test every blocker individually and in combinations.
- [ ] Test a blocker appearing during `suspending`; admission freeze must prevent the race.
- [ ] Verify exact suspend and resume dependency order.
- [ ] Inject failure before and after every callback and verify complete rollback.
- [ ] Test wake retention and delivery only after resume completion.
- [ ] Test monotonic advancement and expired finite waits across simulated sleep.
- [ ] Verify device registry IDs, service handles, wait sources, process IDs, foreground
  ownership, and nested parent/child state remain valid across light sleep.
- [ ] Verify reboot/power-off wins races with idle and suspend transitions.
- [ ] Run hundreds or thousands of deterministic suspend/resume cycles.
- [ ] Run unit and component tests under supported host sanitizers.
- [ ] Run macOS and Linux Debug/Release builds, tests, and maintained RV32 tester.

## Physical Tab5 Validation

- [ ] Record test setup, firmware revision, board/display revision, power source, and
  measurement instrument.
- [ ] Record active baseline before enabling any optimization.
- [ ] Measure display dimming, reduced polling, dynamic frequency scaling, tickless idle,
  and coordinated light sleep separately.
- [ ] Verify keyboard, power button, RTC alarm, and BMI270 motion wake independently.
- [ ] Verify wake event is not lost or delivered twice.
- [ ] Repeat suspend/resume during filesystem stress and verify TF integrity afterward.
- [ ] Exercise network, audio, camera, outstanding camera leases, graphics, pointer input,
  and nested foreground processes before attempted suspend.
- [ ] Verify blocked transitions leave services fully operational.
- [ ] Verify successful cycles preserve registry state, input, display, networking, and
  foreground-process execution.
- [ ] Verify rollback after an injected or observed driver failure.
- [ ] Repeat physical cycles long enough to expose interrupt, SMP, lock, and resource leaks.
- [ ] Validate display/backlight recovery on ILI9881C, ST7123, and ST7121 revisions.
- [ ] Run Tab5 Debug and Release builds plus full maintained application build.

## Documentation and Tracking

- [ ] Update `agents/architecture.md` when power-state and deep-sleep semantics become
  accepted architecture decisions.
- [ ] Update `agents/testing.md` with portable and hardware power validation requirements.
- [ ] Update `agents/TABOS_CONTEXT.md`, `agents/hardware-services.md`, and
  `agents/roadmap.md` after each completed slice.
- [ ] Add user-facing power-state and application-behavior documentation under `docs/`
  when observable behavior or public API lands.
- [ ] Document every enabled default, timeout, wake source, blocker, failure mode, and
  measured power result.

## Completion Criteria

- [ ] Idle mode measurably reduces power and restores immediately on activity.
- [ ] Transparent light sleep preserves foreground process and boot-local identities.
- [ ] Every active service either suspends safely or reports a deterministic blocker.
- [ ] Dependency ordering and rollback are exhaustively host-tested.
- [ ] RTC, keyboard, power-button, and BMI270 wake sources pass physical validation.
- [ ] Monotonic and wall-clock contracts remain correct across sleep.
- [ ] Repeated physical cycles produce no filesystem corruption, lost input, stale
  registry state, broken networking, or foreground-process failure.
- [ ] Before/after power measurements exist for every enabled optimization.
- [ ] All required documentation and milestone checklists agree with implementation.

## References

- ESP-IDF v5.4.4 power management:
  `https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32p4/api-reference/system/power_management.html`
- ESP32-P4 sleep modes:
  `https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/api-reference/system/sleep_modes.html`
- ESP Timer sleep behavior:
  `https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/esp_timer.html`
- M5Stack Tab5 hardware documentation:
  `https://docs.m5stack.com/en/core/Tab5`
- M5Stack Tab5 reference implementation:
  `https://github.com/m5stack/M5Tab5-UserDemo`
