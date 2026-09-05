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

- [ ] Define a narrow portable runtime-event vocabulary and bitset.
- [ ] Add platform operations to notify runtime, wait indefinitely, and wait until an
  absolute monotonic deadline.
- [ ] Make notification safe from task context and, where supported, ISR context.
- [ ] Coalesce duplicate readiness bits without losing authoritative queued data.
- [ ] Define shutdown behavior that wakes every blocked runtime/service task before
  destroying its notification primitive.
- [ ] Add portable deadline discovery for every service that currently requires periodic
  updates.
- [ ] Replace fixed run-loop delay with wait-until-event-or-nearest-deadline behavior.
- [ ] Keep runtime runnable while active host RV32 interpretation requires execution
  slices; block only when no runnable process or ready service work exists.

### Phase 2: Tab5 keyboard interrupt

- [ ] Configure GPIO50 as the active-low keyboard interrupt input.
- [ ] Enable the Normal-mode interrupt in keyboard register `0x00`.
- [ ] Install a minimal ISR that notifies task context.
- [ ] Drain event count and event data over I2C only after task wake.
- [ ] Read and clear keyboard interrupt status through register `0x01` as required by the
  controller protocol.
- [ ] Recheck interrupt level/status after clearing so events arriving during drain or
  clear cannot be lost.
- [ ] Drain any boot-time queued events before entering normal interrupt-driven operation.
- [ ] Preserve physical press/release, Sym and Aa/Shift behavior, cooked text, raw input,
  multi-key state, and portable repeat timing.
- [ ] On shutdown, disable interrupt delivery, wake and stop dependent task work, remove
  ISR handler, then release keyboard I2C resources.
- [ ] Keep missing or failed keyboard nonfatal and accurately reflected in device registry.

### Phase 3: Tab5 touch interrupt

- [ ] Configure detected GT911 or ST712x driver with `BSP_LCD_TOUCH_INT` on GPIO23 instead
  of `GPIO_NUM_NC`.
- [ ] Install controller-supported interrupt callback that only notifies task context.
- [ ] Read touch reports over I2C after notification.
- [ ] Drain/recheck controller readiness so movement or release arriving during processing
  cannot be lost.
- [ ] Preserve contact matching, logical rotation, stable contact IDs, foreground routing,
  queue overflow, and cancellation behavior.
- [ ] Validate down, movement, release, multitouch, rapid retouch, and long stationary
  contact behavior independently for GT911, ST7123, and ST7121 revisions.
- [ ] Cancel active contacts deterministically if controller faults, disappears, resets,
  or shuts down.
- [ ] Remove callback/ISR before deleting touch handle and panel I/O resources.

### Phase 4: Deadline-driven portable services

- [ ] Change key repeat from periodic polling to an explicit next-repeat deadline.
- [ ] Cancel repeat deadline immediately on matching key release or input reset.
- [ ] Change cursor blink from periodic polling to an explicit next-blink deadline.
- [ ] Suppress cursor deadline while fullscreen graphics owns display.
- [ ] Change network autoconnect retry into an explicit monotonic deadline.
- [ ] Integrate generic finite-wait deadlines without early wake or busy polling.
- [ ] Define missed-period behavior: perform one current update and advance to next future
  deadline rather than replaying every missed interval.
- [ ] Handle monotonic saturation and infinite wait without arithmetic wraparound.

### Phase 5: Network and device-state notifications

- [ ] Propagate ESP-IDF Wi-Fi/IP events into portable network service immediately.
- [ ] Notify runtime when host network simulation changes state.
- [ ] Remove unconditional `platform_network_status()` polling from every runtime update.
- [ ] Preserve bounded autoconnect attempts and retry delays.
- [ ] Make RTC, battery, audio, pointer, camera, storage, and Wi-Fi registry state update
  when owning service health changes.
- [ ] Remove unconditional full hardware-health scans from every runtime update.
- [ ] Add optional low-rate health audit only where hardware cannot report failures and
  on-demand operations cannot observe them.
- [ ] Ensure state changes still produce exactly one device-subscription event per actual
  transition.

### Phase 6: Camera completion path

- [ ] Replace repeated 10 ms camera dequeue attempts with a dedicated capture worker or an
  interruptible blocking driver wait.
- [ ] Wake worker on capture start and stop.
- [ ] Block until completed buffer, stop request, fault, or bounded watchdog deadline.
- [ ] Keep I/O, RAW conversion, RGB preview, JPEG encoding, H.264 encoding, and frame-pool
  submission in task context.
- [ ] Preserve H.264 backpressure: do not dequeue when no unleased destination slot can
  accept next reference-dependent frame.
- [ ] Interrupt blocked dequeue during close, device removal, process teardown, and system
  shutdown before buffers or mutexes are destroyed.
- [ ] Retain slow stall diagnostics without turning watchdog deadline into normal polling.

### Phase 7: Application and process notifications

- [ ] Notify process manager when native application task returns instead of polling its
  `finished` atomic flag every runtime iteration.
- [ ] Notify runtime for exit request, child-exec request, and other ELF call-gate work that
  currently waits for next update slice.
- [ ] Preserve persistent nested foreground process ordering and process-0 panic invariant.
- [ ] Ensure notification cannot target a destroyed or generation-reused process object.
- [ ] Wake and join/terminate application work in deterministic cleanup order.
- [ ] Keep host RV32 interpreter scheduling explicit: runnable guests receive bounded
  instruction slices; blocked guests do not force periodic runtime wakeups.

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

- [ ] Immediate wake from task notification.
- [ ] Immediate wake from simulated ISR notification.
- [ ] Wake at exact nearest deadline without early dispatch.
- [ ] Indefinite block when no event or deadline exists.
- [ ] Coalesced duplicate notification without lost queued work.
- [ ] Simultaneous event bits processed fairly and deterministically.
- [ ] New earlier deadline interrupts an existing longer wait.
- [ ] Shutdown safely wakes and disposes blocked runtime.

### Keyboard

- [ ] One interrupt drains one event and burst queue.
- [ ] Event arriving during interrupt clear is retained.
- [ ] Press/release order and independent multi-key state remain correct.
- [ ] Cooked/raw mode, Sym, Aa/Shift, modifiers, and text remain correct.
- [ ] Repeat fires at deadlines without keyboard polling.
- [ ] Missing device, I2C failure, ISR setup failure, and teardown are safe.

### Touch

- [ ] Down, move, up, multitouch, and rapid replacement preserve contact semantics.
- [ ] Event arriving during controller read/recheck is retained.
- [ ] Stationary contact does not cause busy polling.
- [ ] Focus change, reset, fault, removal, and shutdown cancel contacts.
- [ ] GT911 and ST712x fake backends satisfy same contract.

### Services and process lifecycle

- [ ] Wi-Fi/IP transitions update portable state and device registry exactly once.
- [ ] Network retry fires only at deadline.
- [ ] Camera worker blocks when idle and wakes for frame, stop, fault, and shutdown.
- [ ] H.264 backpressure and camera lease behavior remain unchanged.
- [ ] Native application completion wakes parent and preserves child status.
- [ ] Child/grandchild unwind and process-0 panic paths remain correct.
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
- [ ] Validate rapid typing, held keys, chords, Sym, Aa/Shift, and event bursts.
- [ ] Validate keyboard interrupt recovery after unplug/replug or observed bus fault where
  supported.
- [ ] Measure touch-down and movement latency before and after conversion.
- [ ] Validate GT911, ST7123, and ST7121 down/move/up/multitouch behavior.
- [ ] Confirm idle keyboard and touch produce no periodic I2C traffic.
- [ ] Validate Wi-Fi connect/disconnect/failure/retry transitions.
- [ ] Validate camera capture, close, process teardown, and repeated start/stop.
- [ ] Validate shell, child, and grandchild execution while event-driven services remain
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
