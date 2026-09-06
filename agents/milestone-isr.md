# TabOS Interrupt and Event-Driven Runtime Milestone

## Goal

Replace fixed 10 ms hardware and service polling with interrupt, completion-event, and
deadline-driven execution. Improve input latency, reduce needless I2C traffic and CPU
wakeups, and give TabOS a runtime that can block whenever no work is ready.

Implement this milestone independently of low-power policy. Power management may later
consume the idle intervals and wake sources created here, but this work must be useful,
testable, and complete without enabling dynamic frequency scaling, light sleep, deep
sleep, idle dimming, or automatic suspend.

## Current Baseline

- [x] Portable keyboard and pointer services already accept normalized events from
  platform backends.
- [x] Generic wait sources already wake applications for socket, device, pointer, camera,
  and audio readiness.
- [x] Tab5 display VSYNC and PPA completion already use ISR-to-semaphore signaling.
- [x] Tab5 audio uses render, capture, and error callbacks rather than runtime polling.
- [x] ESP-IDF Wi-Fi and IP changes already arrive through event callbacks below the
  platform boundary.
- [ ] Tab5 run loop wakes every 10 ms whether work exists or not.
- [ ] Tab5 keyboard interrupt is disabled and its I2C event count is polled every 10 ms.
- [ ] Tab5 touch interrupt is ignored and controller data is polled every runtime update.
- [ ] Portable key repeat and terminal cursor blink depend on periodic update calls.
- [ ] Portable network state is copied from the platform on every runtime update.
- [ ] Tab5 camera dequeue uses repeated 10 ms timeout attempts.
- [ ] Native application completion is detected by polling an atomic flag.
- [ ] Device health and registry synchronization are polled every runtime update.

## Core Decisions

- [DECIDED] Event-driven runtime is an independent architecture milestone and a
  prerequisite for efficient power management, not an implementation detail hidden
  inside the power milestone.
- [DECIDED] Target architecture is event/deadline driven. Not every event requires a
  hardware ISR.
- [DECIDED] Hardware ISRs do minimum bounded work and notify task context. I2C, memory
  allocation, service state changes, event translation, and application delivery never
  execute in ISR context.
- [DECIDED] Portable code receives TabOS-owned events and deadlines. ESP-IDF, FreeRTOS,
  SDL, GPIO, and native driver types remain below the platform boundary.
- [DECIDED] Event notifications may coalesce when the hardware controller or service owns
  the authoritative queue or state. Notifications are wake hints, not payload storage.
- [DECIDED] Deadline-driven work uses one nearest-deadline calculation rather than a
  blanket periodic tick.
- [DECIDED] Interrupt conversion must preserve event ordering, bounded queue behavior,
  foreground ownership, normalized repeat behavior, and process cleanup semantics.
- [DECIDED] A slow safety poll is permitted only when physical measurements prove a
  controller can lose interrupts. It must be documented, measured, and remain separate
  from normal event delivery.
- [DECIDED] Host implementation uses blocking SDL/native waits and synthetic notifications
  while preserving the same portable scheduling semantics.

## Runtime Model

```text
hardware IRQs            service completions       software deadlines
------------             -------------------       ------------------
keyboard GPIO50          Wi-Fi/IP events            key repeat
touch GPIO23             camera frame ready         cursor blink
RTC/BMI/power later      audio buffer events        network retry
display/PPA done         application completion     generic wait timeout
       |                         |                         |
       +-------------------------+-------------------------+
                                 |
                                 v
                      platform runtime wake primitive
                                 |
                                 v
                     run only currently ready work
                                 |
                                 v
                   compute nearest pending deadline
                                 |
                                 v
                         block until next wake
```

The wake primitive transports readiness, not application data. Each subsystem drains its
own bounded queue or reads its authoritative state after waking.

## ISR Rules

ISR code may:

- Read or acknowledge a hardware interrupt only when required and documented ISR-safe.
- Set an atomic flag or event bit.
- Notify a task using the platform's ISR-safe primitive.
- Request a context switch when a higher-priority task became ready.

ISR code must not:

- Perform I2C transactions.
- Allocate or free memory.
- Acquire ordinary mutexes.
- Call filesystem, network, console, graphics, or application APIs.
- Perform keyboard translation, touch contact matching, camera conversion, or encoding.
- Publish directly into process-owned queues unless that queue explicitly supports the
  ISR contract.

## Event Classes

### Hardware interrupts

- Keyboard event pending on Tab5 GPIO50.
- Touch report pending on Tab5 GPIO23.
- Future RTC alarm, BMI270 data-ready/motion, power-button, and storage-detect signals.
- Display VSYNC and graphics accelerator completion, which already follow this model.

### Driver and service completions

- Wi-Fi and IP state changes from ESP-IDF event callbacks.
- Socket and TLS readiness from existing wait adapters.
- Camera buffer completion or interruptible blocking dequeue.
- Audio buffer demand/capture completion, which already uses callbacks.
- Native application return, exit request, child-exec request, and other process work.
- Device state changes reported by the owning service.

### Software deadlines

- Held-key repeat.
- Terminal cursor blink.
- Network retry.
- Application sleep and cooperative scheduling deadlines.
- Generic wait timeouts.
- Future idle and suspend deadlines owned by power management.

## Implementation Plan

### Phase 1: Portable runtime wake and deadline foundation

- [x] Define a narrow portable runtime-event vocabulary and bitset.
- [x] Add platform operations to notify runtime, wait indefinitely, and wait until an
  absolute monotonic deadline.
- [x] Make notification safe from task context and, where supported, ISR context.
- [x] Coalesce duplicate readiness bits without losing authoritative queued data.
- [x] Define shutdown behavior that wakes every blocked runtime/service task before
  destroying its notification primitive.
- [x] Add portable deadline discovery for every service that currently requires periodic
  updates.
- [x] Replace fixed run-loop delay with wait-until-event-or-nearest-deadline behavior.
- [x] Keep runtime runnable while active host RV32 interpretation requires execution
  slices; block only when no runnable process or ready service work exists.

Physical Tab5 Phase 1 smoke validation passed on 2026-09-05: firmware booted into
the shell, `touchtest` loaded and ran, input/touch/cursor/application behavior remained
responsive, and no watchdog, crash, freeze, or other runtime failure was reported.
Two intermittent PI4IOE5V6408 input-register failures came from the previously recorded
headphone-detect poll issue and remain deferred outside Phase 1.

### Phase 2: Tab5 keyboard interrupt

- [x] Configure GPIO50 as the active-low keyboard interrupt input.
- [x] Enable the Normal-mode interrupt in keyboard register `0x00`.
- [x] Install a minimal ISR that notifies task context.
- [x] Drain event count and event data over I2C only after task wake.
- [x] Read and clear keyboard interrupt status through register `0x01` as required by the
  controller protocol.
- [x] Recheck interrupt level/status after clearing so events arriving during drain or
  clear cannot be lost.
- [x] Drain any boot-time queued events before entering normal interrupt-driven operation.
- [x] Preserve physical press/release, Sym and Aa/Shift behavior, cooked text, raw input,
  multi-key state, and portable repeat timing.
- [x] On shutdown, disable interrupt delivery, wake and stop dependent task work, remove
  ISR handler, then release keyboard I2C resources.
- [x] Keep missing or failed keyboard nonfatal and accurately reflected in device registry.

Physical Tab5 Phase 2 smoke validation passed on 2026-09-05: rapid typing and key
repeat remained responsive, holding Sym or Aa/Shift while typing preserved modifier
behavior, and holding Delete during reset still entered USB mass-storage mode.

### Phase 3: Tab5 touch interrupt

- [x] Configure detected GT911 or ST712x driver with `BSP_LCD_TOUCH_INT` on GPIO23 instead
  of `GPIO_NUM_NC`.
- [x] Install controller-supported interrupt callback that only notifies task context.
- [x] Read touch reports over I2C after notification.
- [x] Drain/recheck controller readiness so movement or release arriving during processing
  cannot be lost.
- [x] Preserve contact matching, logical rotation, stable contact IDs, foreground routing,
  queue overflow, and cancellation behavior.
- [ ] Validate down, movement, release, multitouch, rapid retouch, and long stationary
  contact behavior independently for GT911, ST7123, and ST7121 revisions.
- [x] Cancel active contacts deterministically if controller faults, disappears, resets,
  or shuts down.
- [x] Remove callback/ISR before deleting touch handle and panel I/O resources.

Controller-neutral fake GT911 and ST712x tests cover down, movement, release,
multitouch contact matching, rapid retouch, stationary-report suppression, arrival during drain,
bounded rescheduling, I2C fault cancellation, and shutdown cancellation. Physical
validation remains required independently on GT911, ST7123, and ST7121 hardware.

ST7121 boot currently emits
`gpio_install_isr_service(...): GPIO isr service already installed` after keyboard
initialization installs the shared ESP-IDF GPIO ISR service first. Touch remains
functional because GPIO23 attaches to the existing service, but ownership should be
centralized or made explicitly idempotent so later peripheral initialization does not
attempt a duplicate global service install or emit an error-level boot message.

### Phase 4: Deadline-driven portable services

- [x] Change key repeat from periodic polling to an explicit next-repeat deadline.
- [x] Cancel repeat deadline immediately on matching key release or input reset.
- [x] Change cursor blink from periodic polling to an explicit next-blink deadline.
- [x] Suppress cursor deadline while fullscreen graphics owns display.
- [x] Change network autoconnect retry into an explicit monotonic deadline.
- [x] Integrate generic finite-wait deadlines without early wake or busy polling.
- [x] Define missed-period behavior: perform one current update and advance to next future
  deadline rather than replaying every missed interval.
- [x] Handle monotonic saturation and infinite wait without arithmetic wraparound.

Portable timers now reserve `UINT64_MAX` exclusively for no deadline and clamp finite
deadlines to the latest representable value. Repeating timers emit one expiration after
a late update and preserve cadence by advancing directly to the next future period.
Input repeat and network retry use these timers; cursor ownership changes wake runtime
to add or remove its blink deadline immediately. Generic finite waits calculate one
absolute monotonic deadline and recompute remaining blocking time after early wakes.
Physical Tab5 testing exposed application-task starvation when the input consumer spun
on an atomic queue lock while the runtime producer was preempted on the same core. Input
queue and repeat state now use the priority-inheriting platform mutex; keyboard ISRs
still only set readiness and never acquire it.

### Phase 5: Network and device-state notifications

- [x] Propagate ESP-IDF Wi-Fi/IP events into portable network service immediately.
- [x] Notify runtime when host network simulation changes state.
- [x] Remove unconditional `platform_network_status()` polling from every runtime update.
- [x] Preserve bounded autoconnect attempts and retry delays.
- [x] Make RTC, battery, audio, pointer, camera, storage, and Wi-Fi registry state update
  when owning service health changes.
- [x] Remove unconditional full hardware-health scans from every runtime update.
- [x] Add optional low-rate health audit only where hardware cannot report failures and
  on-demand operations cannot observe them.
- [x] Ensure state changes still produce exactly one device-subscription event per actual
  transition.

Network backends now publish coalesced change notifications that wake runtime; portable
network service reads backend status only after notification and retains deadline-driven,
three-attempt autoconnect behavior. Audio, pointer, and camera faults wake device-state
synchronization immediately. In-memory service states are synchronized without hardware
I/O on each runtime pass. Keyboard, RTC, battery, and mounted-storage health—whose current
drivers lack change callbacks—use one 60-second audit deadline. Registry transition
coalescing continues to suppress duplicate subscription events.

### Phase 6: Camera completion path

- [x] Replace repeated 10 ms camera dequeue attempts with a dedicated capture worker or an
  interruptible blocking driver wait.
- [x] Wake worker on capture start and stop.
- [x] Block until completed buffer, stop request, fault, or bounded watchdog deadline.
- [x] Keep I/O, RAW conversion, RGB preview, JPEG encoding, H.264 encoding, and frame-pool
  submission in task context.
- [x] Preserve H.264 backpressure: do not dequeue when no unleased destination slot can
  accept next reference-dependent frame.
- [x] Interrupt blocked dequeue during close, device removal, process teardown, and system
  shutdown before buffers or mutexes are destroyed.
- [x] Retain slow stall diagnostics without turning watchdog deadline into normal polling.

Host and Tab5 backends now own dedicated capture workers. Frame/error callbacks notify
runtime, while `kernel_runtime_update()` and camera wait adapters no longer request
periodic dequeue work. Tab5 blocks in `VIDIOC_DQBUF` with a two-second diagnostic watchdog;
its worker is pinned to CPU0 to preserve the physically validated V4L2/ISP processing
context. An unpinned worker produced corrupted gray RGB565 previews even though capture
completed without driver errors. CPU0-pinned hardware validation produced clear, visible
`cameratest preview 300` and `cameratest preview 1000` output; RAW capture and the maintained
tester also completed successfully.
stop marks capture inactive, stops the V4L2 stream, wakes the worker, and waits for its idle
acknowledgment before unmapping buffers. H.264 queries kernel pool capacity before dequeue
and resumes only when lease release signals new capacity. Real host-worker coverage proves
completion wake, capacity blocking/resume, and joined close; Tab5 Debug cross-build passes.

### Phase 7: Application and process notifications

- [x] Notify process manager when native application task returns instead of polling its
  `finished` atomic flag every runtime iteration.
- [x] Notify runtime for exit request, child-exec request, and other ELF call-gate work that
  currently waits for next update slice.
- [x] Preserve persistent nested foreground process ordering and process-0 panic invariant.
- [x] Ensure notification cannot target a destroyed or generation-reused process object.
- [x] Wake and join/terminate application work in deterministic cleanup order.
- [x] Keep host RV32 interpreter scheduling explicit: runnable guests receive bounded
  instruction slices; blocked guests do not force periodic runtime wakeups.

Native Tab5 completion, ELF exit requests, ELF child-exec requests, process launch, and
parent restoration now publish the coalesced application readiness bit. Notifications
carry no process pointer or generation-sensitive payload; process state remains
authoritative, so a late coalesced wake cannot address a destroyed or reused slot. ELF
teardown first cancels blocking waits and terminates the execution context, then releases
process-owned services, descriptors, heap, and executable image. Host RV32 execution
retains bounded explicit interpreter slices and does not create a completion worker.

Physical Tab5 validation on 2026-09-06 completed three consecutive full tester runs,
including child/grandchild execution and reverse-order status propagation, and repeated
`hello` launches returned promptly to the shell without watchdog or error logs. Rapid
keyboard input during tester remained queued for the shell because tester stops consuming
stdin after its nonblocking-input case; this is current controlling-terminal behavior, not
an application-interrupt mechanism.

An isolated `tester --camera` run still reports the pre-existing combined pool-exhaustion
assertion intermittently. Capture diagnostics show RAW conversion taking approximately
154--211 ms per frame while the test allows only 200 ms to prove a dropped frame. Five
frames complete without dequeue misses or camera errors, consistent with the released slot
becoming available before another frame can be dropped. Make this camera test
frame-rate-aware and split replacement, lease-generation, and drop-count diagnostics; this
is a Phase 6 test follow-up, not a Phase 7 process-notification failure.

### Phase 8: Central run-loop conversion

- [ ] Remove direct keyboard polling from Tab5 `platform_run()`.
- [ ] Remove unconditional touch, network, camera, hardware-health, and application-
  completion polling from `kernel_runtime_update()`.
- [ ] Dispatch only services whose event bits or deadlines are ready.
- [ ] Recompute nearest deadline after each dispatch because callbacks may add, cancel, or
  shorten deadlines.
- [ ] Prevent starvation when interrupts arrive continuously by bounding work per dispatch
  before checking other ready sources.
- [ ] Preserve prompt input, cursor, networking, display, filesystem, camera, and process
  progress while foreground native application remains active.
- [ ] Record wake counts by source in debug diagnostics so remaining periodic wakeups can
  be found and measured.

## Host Design

- SDL keyboard, mouse, touch, window, and quit events wake host runtime through blocking
  event wait with nearest-deadline timeout.
- Synthetic test events use same portable notification path after SDL translation.
- POSIX socket and other worker completions wake runtime without exposing descriptors as
  portable event handles.
- Headless runtime uses condition variable or equivalent platform primitive and fake
  monotonic clock; tests must not sleep in real time.
- Runnable RV32 guest prevents indefinite blocking but still uses bounded slices so
  services and deadlines progress.

## Concurrency and Lifetime Rules

- Notification source must stop before owner state or wait primitive is destroyed.
- ISR handlers must be removed before GPIO, I2C, controller, or service teardown.
- Worker shutdown first sets stop state, then wakes blocked operation, then joins worker,
  then releases resources.
- Event bits may coalesce; subsystem queues remain bounded and retain existing overflow
  policy.
- Callback and worker entry must validate initialization/generation before accessing
  retained state.
- Portable service locks remain platform mutexes; no FreeRTOS primitive crosses boundary.
- Event dispatch must not hold central runtime lock while calling subsystem code.
- Runtime shutdown and reboot must wake blocked loops and remain first-request-wins.

## Automated Validation

### Runtime foundation

- [x] Immediate wake from task notification.
- [x] Immediate wake from simulated ISR notification.
- [x] Wake at exact nearest deadline without early dispatch.
- [x] Indefinite block when no event or deadline exists.
- [x] Coalesced duplicate notification without lost queued work.
- [x] Simultaneous event bits processed fairly and deterministically.
- [x] New earlier deadline interrupts an existing longer wait.
- [x] Shutdown safely wakes and disposes blocked runtime.

### Keyboard

- [x] One interrupt drains one event and burst queue.
- [x] Event arriving during interrupt clear is retained.
- [x] Press/release order and independent multi-key state remain correct.
- [ ] Cooked/raw mode, Sym, Aa/Shift, modifiers, and text remain correct.
- [x] Repeat fires at deadlines without keyboard polling.
- [ ] Missing device, I2C failure, ISR setup failure, and teardown are safe.

### Touch

- [ ] Down, move, up, multitouch, and rapid replacement preserve contact semantics.
- [ ] Event arriving during controller read/recheck is retained.
- [ ] Stationary contact does not cause busy polling.
- [ ] Focus change, reset, fault, removal, and shutdown cancel contacts.
- [ ] GT911 and ST712x fake backends satisfy same contract.

### Services and process lifecycle

- [ ] Wi-Fi/IP transitions update portable state and device registry exactly once.
- [x] Network retry fires only at deadline.
- [x] Camera worker blocks when idle and wakes for frame, stop, fault, and shutdown.
- [x] H.264 backpressure and camera lease behavior remain unchanged.
- [x] Native application completion wakes parent and preserves child status.
- [x] Child/grandchild unwind and process-0 panic paths remain correct.
- [ ] Device events, wait sources, and resource cleanup remain generation-safe.

### Cross-target validation

- [ ] macOS Debug and Release builds and tests pass.
- [ ] Linux Debug and Release builds and tests pass.
- [ ] Host sanitizer suites pass.
- [ ] Tab5 Debug and Release cross-builds pass.
- [ ] All standalone application builds pass.
- [ ] Maintained RV32 tester passes on host and physical Tab5.

## Physical Tab5 Validation

- [ ] Record baseline runtime wake rate, keyboard I2C transactions, touch I2C transactions,
  idle CPU load, and input latency before conversion.
- [ ] Measure keyboard press-to-event latency before and after interrupt conversion.
- [x] Validate rapid typing, held keys, chords, Sym, Aa/Shift, and event bursts.
- [ ] Validate keyboard interrupt recovery after unplug/replug or observed bus fault where
  supported.
- [ ] Measure touch-down and movement latency before and after conversion.
- [ ] Validate GT911, ST7123, and ST7121 down/move/up/multitouch behavior.
- [ ] Confirm idle keyboard and touch produce no periodic I2C traffic.
- [ ] Validate Wi-Fi connect/disconnect/failure/retry transitions.
- [ ] Validate camera capture, close, process teardown, and repeated start/stop.
- [x] Validate shell, child, and grandchild execution while event-driven services remain
  responsive.
- [ ] Verify watchdog, deadlock, lost-interrupt, duplicate-event, and starvation behavior
  under simultaneous keyboard, touch, network, camera, and application activity.
- [ ] Record final runtime wake-source counts and unexplained periodic wakeups.

## Power-Milestone Handoff

After this milestone completes, revise `agents/milestone-power.md` so it assumes:

- Keyboard and touch already provide interrupt-driven events.
- Network, camera, audio, display, and application completion already notify runtime.
- Key repeat, cursor blink, retries, and waits already publish exact deadlines.
- Runtime already blocks when no work is ready.
- Power management owns only idle policy, brightness, suspend blockers, ordered
  suspend/resume, wake-source arming, sleep entry, time accounting, and power measurement.

Do not duplicate ISR conversion or central run-loop work in both milestones after that
rewrite.

## Documentation and Tracking

- [ ] Update `agents/architecture.md` with accepted event/deadline-driven runtime rules.
- [ ] Update `agents/testing.md` with interrupt, notification, deadline, teardown, and
  physical validation requirements.
- [ ] Update `agents/TABOS_CONTEXT.md` and `agents/roadmap.md` as phases complete.
- [ ] Rephrase `agents/milestone-power.md` after this milestone contract is accepted.
- [ ] Update user-facing input, pointer, and runtime documentation when observable behavior
  or troubleshooting workflow changes.
- [ ] Record hardware protocol references and measured results rather than relying on
  assumed interrupt behavior.

## Completion Criteria

- [ ] Tab5 runtime has no fixed 10 ms polling loop.
- [ ] Idle keyboard and touch generate no periodic I2C reads.
- [ ] Keyboard and touch events remain complete, ordered, bounded, and responsive.
- [ ] Portable timers and retries dispatch from exact deadlines.
- [ ] Network, camera, device health, and native application completion wake runtime only
  when work exists.
- [ ] Shutdown and process teardown safely interrupt and join every blocked task.
- [ ] Host behavior matches portable scheduling semantics deterministically.
- [ ] Cross-target builds, automated suites, maintained tester, and physical validation
  pass.
- [ ] Debug wake accounting shows no unexplained high-frequency periodic source.
- [ ] Power milestone is revised to consume this foundation without duplicating it.

## References

- M5Stack Tab5 Keyboard protocol and interrupt behavior:
  `https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1241/Tab5_Keyboard_User_Manual_EN.pdf`
- M5Stack Tab5 Keyboard documentation and GPIO50 wiring:
  `https://docs.m5stack.com/en/arduino/projects/tab5/tab5_keyboard`
- M5Stack Tab5 BSP API and GPIO23 touch interrupt:
  `https://github.com/espressif/esp-bsp/blob/master/bsp/m5stack_tab5/API.md`
- Espressif GT911 interrupt and sleep implementation:
  `https://github.com/espressif/esp-bsp/blob/master/components/lcd_touch/esp_lcd_touch_gt911/esp_lcd_touch_gt911.c`
- Espressif touch-controller integration documentation:
  `https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/touch_panel.html`
