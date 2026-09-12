# TabOS Power Milestone Implementation Plan

## Agreed behavior

Deliver portable power manager, idle dimming, and idle screen-off first; coordinated light sleep follows after service safety and rollback tests pass.

| Policy | Decision |
|---|---|
| Idle dimming | Enabled by default after validation; 60 seconds inactivity, 20% brightness |
| Idle screen-off | 180 seconds total inactivity: backlight off, panel enabled; 300 seconds: panel disabled too; CPU and services continue running |
| Display activity restoration | Touch/keyboard restore from dimmed or backlight-only off before input delivery. After 300-second panel-off, keyboard restores; pointer activity deliberately does not restore or reset inactivity |
| Display policy versus sleep | Dimming and screen-off are display power savings, not system sleep. Actual system sleep also requires screen off |
| Brightness restoration | Restore previous active brightness before delivering activity |
| Automatic suspend | Disabled by default; 10 minutes inactivity when enabled |
| Configuration | Runtime changes immediate; explicit `powerctl save` persists settings |
| Fullscreen graphics | Blocks dimming, screen-off, and every suspend request |
| Media | Open audio/camera streams block suspend; initially also inhibit dimming and screen-off |
| Wi-Fi | May disconnect before suspend and reconnect afterward; IP continuity not promised |
| Application deadlines | Continue aging but do not wake suspended system; complete after another wake |
| Wake sources | Investigate keyboard, touch, power button, RTC alarm, and BMI270 motion |
| Motion wake | Opt-in |
| Unsupported wake hardware | Document evidence and report unavailable; no speculative registration |
| Unrecoverable essential-service resume failure | Panic, keep applications parked, require explicit reset/reboot |
| Deep sleep | Deferred; no process-persistence or reboot-style hibernation implementation |

“Suspend” means transparent light sleep: preserve RAM, process stack, foreground ownership, handles, and boot-local device identities.

All display timeouts run from the same last-activity timestamp: dim at 60 seconds,
backlight off at 180 seconds, then panel off at 300 seconds. Screen-off must not park
applications, pause application deadlines, or disconnect networking. Preserve display
contents in RAM for restoration. Retain any shared supply needed for touch operation;
touch restoration while the CPU runs is separate from proving touch wake from light sleep.

## Phase 0 — Establish evidence and platform limits

- [x] Record firmware revision, pinned ESP-IDF v5.4.4, resolved components, P4 silicon revision, display/touch controller, power source, and attached peripherals.
- [x] Inventory every initialized driver, worker, timer, interrupt, DMA operation, bus dependency, and existing PM lock. Include idle services with no application handles.
- [x] Classify each participant as safely suspendable, inactive, or blocking. Missing lifecycle support must prevent suspend.
- [x] Record current runtime wake counters and worker activity. Include continuous audio codec I/O, headphone monitoring, display VSYNC, cursor blink, and hardware-health audit.
- [x] Resolve shared GPIO ISR-service ownership before adding more interrupt consumers.
- [x] Trace keyboard/touch and RTC/IMU/power-controller routes using published schematics and available physical evidence. Published net trace and physical input checks complete. User accepts PCB revision `unknown` and defers exact matching and circuit-specific wake features ([routing evidence](../docs/power-routing.md)); no electrical or retained-wake validation claimed.
- [x] Distinguish transparent light-sleep wake from hardware power-on/reset. RTC and BMI270 routing through PMS150G requires particular attention. [Tab5 hardware documentation](https://docs.m5stack.com/en/core/Tab5)
- [x] Confirm pinned SDK sleep restrictions, PSRAM retention, GPIO wake support, and chip-revision workarounds. Initial profile retains digital peripheral power, flash, and PSRAM; deeper power-domain shutdown remains disabled.
- [x] Carry forward outstanding ISR cross-target and board-validation evidence without repeating completed interrupt conversion.
- [x] Define measurement worksheet now. Current board permits functional testing; instrumented power evidence remains pending until equipment available.

Source audit and software validation recorded in
[Phase 0 evidence](validation/power-phase0-2026-09-07.md); maintained procedure and
inventory in [power baseline](../docs/power-baseline.md). Local build identity/config
and resolved-component snapshot are complete. Final `740ba1d-dirty` Debug firmware
boots matching rebuilt apps on P4 v1.3, ST7121/ST712x-v1, keyboard FW1 and C6 FW1.4.1.
A quiet 60-second interval records 101 runtime wakes (100 cursor, one health), 6,000
codec I/O pairs, 1,200 headphone reads, 3,928 VSYNC and 100 PPA completions, with no
codec/headphone errors. Duplicate GPIO service-install error is absent at boot.
Operator confirms keyboard/touch with `hello` and `touchtest`. Published connector,
RTC/IMU conditioning and power-controller nets are traced. Operator confirms battery
and USB-C connected, no headphones, Tab5 keyboard expansion and microSD attached,
and USB-A connected to the host for MSC. PCB revision is `unknown`; user explicitly
accepts deferring exact matching and circuit-specific RTC, BMI270 and power-button
wake support. SDK source audit and conservative unavailable-wake classification are
complete. All ten Phase 0 items are closed with this documented limitation. Actual
sleep-retention validation belongs to Phase 7. No sleep path enabled.

**Exit gate:** participant inventory complete; unsupported paths explicit; baseline reproducible.

## Phase 1 — Portable manager and deterministic host model

- [x] Add internal power subsystem with `active`, `idle`, `suspending`, `suspended`, `resuming`, and `shutting-down` states.
- [x] Store current policy, last activity, transition generation/reason, blockers, wake causes, transition timings, and last failure.
- [x] Keep runtime dispatcher as sole transition owner. Requests and worker completions notify existing runtime event mechanism.
- [x] Add power deadline to existing nearest-deadline calculation; introduce no periodic power tick.
- [x] Add fixed-capacity participant registration with stable name, dependency list, blocker query, suspend operation, and resume operation.
- [x] Reject duplicate names, missing dependencies, cycles, and capacity overflow during initialization. Disable suspend on invalid registration; retain normal operation and diagnostics.
- [x] Compute deterministic dependency order once. Registration order breaks ties between independent participants.
- [x] Support asynchronous callback completion through events and transition generations. Ignore stale completions; never retain unprotected process pointers.
- [x] Require callbacks to report success, pending, or failure. No unbounded callback may run on runtime task.
- [x] Add narrow platform contracts for brightness, sleep preparation/abort, sleep entry, wake-cause collection, and restoration.
- [x] Add host fake clock, synthetic activity/wake injection, callback-failure injection, and ordered callback trace. Simulator suspends TabOS execution without suspending host operating system.
- [x] Keep this phase internal; public SDK additions land with actual shell controls.

Phase 1 validation: macOS Debug power tests and full suite pass (socket test requires
normal loopback permission); Tab5 Debug cross-build passes. No Tab5 sleep path is enabled.

**Tests:** every legal/illegal transition, registration failures, ordering, stale completion, deadline saturation, and indefinite blocking.

## Phase 2 — Activity and idle dimming

- [x] Record activity at normalized keyboard/pointer ingress before foreground delivery.
- [x] Count physical presses/releases, pointer movement/contact changes, and button events. Software key repeat, cursor blink, background output, and service completions do not reset inactivity.
- [x] Treat held keys and active pointer contacts as temporary dim/suspend inhibitors; avoid dimming beneath stationary interaction.
- [x] Apply 60-second idle deadline and 20% brightness. Never increase brightness when active setting already below 20%.
- [x] Preserve active brightness independently from effective dimmed brightness; restore before delivering activity.
- [x] Immediately restore brightness when fullscreen ownership or inhibiting media starts.
- [x] On final inhibitor release, begin fresh inactivity interval; avoid immediate dim/suspend after long playback.
- [x] Preserve framebuffer, display ownership, terminal contents, and input ordering.
- [x] Simulate dimming in SDL presentation without modifying framebuffer pixels.
- [x] Report brightness failures and keep logical/effective brightness truthful.

Phase 2 software validation: macOS Debug full suite passes, including exact-boundary,
activity-race, held-input, inhibitor-release, policy-change, and brightness-failure cases;
Tab5 Debug cross-build passes. Operator validates 60-second dimming, brightness restoration
from touch and keyboard input, fullscreen `gdemo` inhibition, and held-contact inhibition.
Captured USB-C setup reads 0.07–0.09 A active and 0.04 A dimmed, a coarse 43–56% reduction.
Meter precision and two-second stability interval limit accuracy. Light sleep remains disabled.

**Tests:** exact timeout boundary, activity races, held input, graphics/media acquisition and release, configuration changes while idle, and brightness failure.

## Phase 3 — Remove avoidable idle service work

Display timings and normal/dim brightness now have optional boot-time configuration in
`T:/etc/power.conf` (`version=1`, `[display]`). Defaults remain 60/180/300 seconds and
75/20 percent. See `docs/power.md` for supported keys and validation. File loading adds
no periodic work or sleep behavior; edits apply on reboot, and invalid files retain
safe defaults without rewriting user data.

- [x] Extend display policy with 180-second idle screen-off, separate from system suspend, using the existing runtime deadline mechanism.
- [x] Restore screen and previous active brightness on touch/keyboard activity during runtime input dispatch through the backlight-only stage; preserve framebuffer contents.
- [x] Add 300-second panel-off stage with keyboard restoration and deliberate pointer-restoration suppression; separate panel and brightness control with ordered failure handling.
- [x] Add automated validation for exact 180-second boundary, activity/off races, touch down/move/up restoration, repeated dim/off/restore cycles, and display failure recovery with CPU and services still running.
- [ ] Physically validate combined 60-second dim, 180-second backlight-off, 300-second panel-off, stage-specific restoration, repeated cycles, and current savings on supported panel revisions.
- [x] Make audio hardware processing demand-driven: stop codec transfers when no audio streams exist; restart before admitting first stream.
- [x] Preserve sample-rate arbitration, routing, capture/playback behavior, and error reporting across worker restarts.
- [x] Run headphone monitoring only while speaker routing needs detection. Sample jack state before enabling speaker.
- [x] Retain existing active-playback detection latency unless measurements support a change; no headphone polling while audio hardware suspended.
- [x] Keep 60-second health audit during normal operation initially; suppress it during suspend and perform one overdue audit after resume.
- [ ] Add reversible display scanout/VSYNC quiescence for screen-off and system suspend; preserve display data and allocations. Validate display-only cycles before integrating CPU sleep.
- [ ] Measure each optimization separately. Do not claim whole-system idle savings from runtime wake counters alone.

Phase 3 audio implementation leaves codec devices discovered but closed at idle. First
stream start configures selected rate and route before returning its handle; last close
joins worker shutdown, disables speaker routing, closes codecs, and stops jack polling.
Host lifecycle tests cover repeated start/stop, rate arbitration, shared streams, injected
start failure, recovery, and owner cleanup. Health-audit pause/resume suppresses its deadline
and runs one overdue audit immediately on resume. macOS Debug targeted tests and Tab5 Debug
cross-build pass.

Physical Tab5 validation confirms `audiotest tone speaker` opens ES8388 playback and
ES7210 capture at 44.1 kHz, plays correctly, then releases hardware. At the next 60-second
diagnostic report, cumulative activity is 199 audio chunks and 41 headphone reads: about
two seconds at the unchanged 10 ms audio cadence and 50 ms jack cadence, consistent with
work occurring only during the tone. Display dims at 89.19 seconds, about 60 seconds after
the tone stream closes. A generic inline USB meter at the Tab5 USB-C input reads 5.12 V,
0.07–0.09 A at 75% brightness with idle shell, and 0.04 A at 20% brightness after two
seconds stable. Battery is absent and charging disabled; keyboard and SD are attached,
USB-A is connected to an unpowered host, and Wi-Fi is connected. Meter resolution and
short observation window make this a coarse whole-system observation, not precise energy
or isolated Phase 3 savings. Available equipment cannot intercept the battery-only path;
battery-powered current measurement is unavailable and is not a delivery expectation.

Display scanout remains open: pinned ESP-IDF v5.4.4 has no public reversible MIPI-DPI
pause operation. `esp_lcd_panel_disp_on_off()` sends the controller display command but
does not stop DPI DMA/VSYNC, while panel deletion frees framebuffers. Do not use either as
fake quiescence. This item remains blocked pending a safe retained-buffer driver lifecycle.
Physical isolated current/activity measurement remains required.

Idle display policy now combines the tested mechanisms: backlight-only off at 180 seconds,
then panel disable at 300 seconds. Pointer activity is ignored for restoration after the
final stage; keyboard restores. Disable backlight before panel; enable panel before
restoring brightness. Failures invalidate the corresponding effective status, with no
periodic retry loop. Panic restores the display and inhibits blanking. CPU/services and
scanout remain running; combined-policy hardware validation remains pending.

Combined three-stage software validation: macOS Debug and Release full suites pass
(69 tests each); Tab5 Debug and Release cross-builds pass. Tests cover exact final-stage
timing, keyboard-only activity restoration after panel-off, panic visibility, retained
service progress, ordered hardware operations, and panel/brightness failure recovery.

Original screen-off software validation: macOS Debug and Release suites pass (69 tests each),
with targeted checks repeated after final panic/runtime-test changes. Tab5 Debug and
Release cross-builds pass. Operator subsequently reports flashing the firmware to Tab5;
the physical results below apply to that original panel-off implementation.

Subsequent operator validation of `7819dab` on ST7121 records 0.08–0.09 A active,
0.04 A dimmed, and 0.01 A panel-off. Keyboard restores the screen without a blue flash;
touch does not restore from panel-off, but `touchtest` works after keyboard restoration.
This is a failed touch-restoration check, not proof that touch wake is unavailable.
The user authorized a backlight-only trial: omit panel display-off/on from brightness
changes and compare touch restoration plus current. Trial current is recorded below;
do not carry the prior 0.01 A result forward as trial savings.

Backlight-only trial: Tab5 Debug cross-build passes (`7819dab-dirty`). Not flashed by
the agent; operator confirms tapping the screen restores it. Portable policy and host code are unchanged.

Operator also reports approximately 0.12 A after power reset, gradually settling to
0.08–0.10 A during normal operation. Startup duration and transient peak are unmeasured;
retain this separately for later startup-current characterization, not as a supply
rating. Details and remaining measurement fields are in
[startup current observation](../docs/power-baseline.md#startup-current-observation).
Backlight-only current at 180 seconds: operator reports predominantly 0.02 A, varying
between 0.01 A and 0.03 A. This is an observed display range, not a sampled mean;
operator confirms touch restoration passes. Keep these readings separate from the prior
0.01 A panel-off result and the startup-current observation.

**Tests:** repeated first-open/last-close, idle audio silence, route correctness, fault recovery, and no new worker spin loops.

**Delivery gate:** manager plus dimming can ship independently once functional and measurement requirements pass.

## Phase 4 — Admission freeze and process safe points

- [x] Add short-lived synchronization around application admission state and operation counters. Never hold power-manager lock across driver I/O or callbacks.
- [ ] Once transition starts, prevent new unsafe operations from racing past completed preflight.
- [ ] Add cooperative parking handshake to native yield, supported blocking waits, and explicit suspend request.
- [x] Park only where application owns no kernel/service lock or active hardware operation. Do not forcibly suspend arbitrary native instruction execution.
- [x] Track every live process, including parents waiting for children. Preserve nesting, child status, foreground owner, and process-0 invariant.
- [x] Stop host RV32 execution only at equivalent safe boundaries; host instruction slicing must not conceal native noncooperation.
- [x] Bring supported generic waits into parking handshake without exposing false cancellation or consuming their readiness.
- [x] Use two-second default application parking/drain deadline. On timeout, report stable blocker, release freeze, and restore operation.
- [x] Freeze process launch/foreground changes during transition; prioritize existing exit, reboot, and power-off requests.
- [x] Ensure application teardown releases counters, pending requests, and parking state exactly once.

Application foundation implemented through internal `kernel_application_power_*` requests.
Native ABI ingress/egress and lock-free yield/generic-wait checkpoints cooperate; pending
non-wait I/O must drain or block. All occupied processes participate, including parents.
Host interpreter slices never count as acknowledgement. Readiness and original wait
deadlines survive parking. Atomic admission tests exercise 1,000 concurrent freeze cycles;
native scheduler tests exercise repeated parking plus computing/active-gate blockers and
teardown of a parked wait. Component tests cover retained readiness/deadlines, nested
process metadata, launch rejection, and exact timeout rollback.

This is not yet registered in the system suspend graph and does not enable automatic
parking or CPU sleep. The remaining checklist items include service-wide freeze/preflight
integration and the explicit suspend request, which depend on Phases 5/6/9. Physical native
parking validation and power measurements remain pending; no current savings claimed.

Application-parking software validation: macOS Debug/Release full suites pass (72 tests
each), including native scheduler-fake coverage. Tab5 Debug/Release cross-builds pass.
Linux and physical parking validation were not run locally.

Initial blockers include:

| Participant | Blocking condition |
|---|---|
| Processes | Native execution outside acknowledged safe point; lifecycle transition |
| Graphics | Fullscreen ownership or unfinished display/accelerator work |
| Audio | Any open application stream |
| Camera | Open stream, capture work, or outstanding frame lease |
| Network | Open socket/TLS connection, DNS/echo operation, connection transition |
| Storage | Undrained operation, sync failure, unsupported mounted backend |
| Sensors/expansion | Active stream/driver lacking tested suspend support |
| USB | Active storage export or unsupported active USB service |
| Input | Held key/contact or pending activity requiring cancellation of suspend |

**Tests:** operation enters immediately before/after freeze, noncooperating app, nested parents, blocked waits, teardown races, and admission reopening after failure.

## Phase 5 — Filesystem safety and reversible service callbacks

- [ ] Replace filesystem spinlock held across storage I/O with scheduler-friendly mutex synchronization.
- [ ] Add admission and in-flight accounting covering reads, writes, metadata operations, descriptor close, and create/truncate opens.
- [ ] Track mutations separately for diagnostics; drain all active storage I/O before platform sleep.
- [ ] Add platform storage synchronization contract. Flush filesystem data and metadata without closing application descriptors or unmounting storage.
- [ ] Preserve descriptor generations, directory positions, offsets, mount identity, and working directories.
- [ ] Permit inactive open descriptors, including writable descriptors after successful synchronization. Open handle alone does not imply active mutation.
- [ ] Do not destroy storage resources if drain deadline expires while I/O still runs. Abort transition; let operation complete normally.
- [ ] Add reversible callbacks for audio/camera workers, display/backlight, input, health audit, networking, and storage.
- [ ] Reuse existing low-level initialization helpers where safe; do not call public service shutdown paths that invalidate identities.
- [ ] Suspend network admission, intentionally disconnect Wi-Fi, and quiesce C6 transport through supported component lifecycle.
- [ ] Preserve network configuration and reconnect intent. Restore transport first; reconnect asynchronously with existing bounded retry policy.
- [ ] Treat access-point absence as ordinary offline/retry state. Failed transport restoration reports network fault; it must not masquerade as restored connectivity.
- [ ] Keep unsupported C6 transport lifecycle as explicit blocker until validated.

**Tests:** live descriptors across cycles, concurrent mutations, sync failure, stalled I/O, worker completion during freeze, disconnected Wi-Fi restoration, and unavailable AP.

## Phase 6 — Ordered suspend, resume, and failure handling

- [ ] Instantiate dependency graph covering process execution, service workers, storage, input/wake controllers, display, shared buses, and platform sleep.
- [ ] Suspend dependents first; resume dependencies first. Foreground execution resumes last.
- [ ] Use order: freeze/park applications → quiesce media/network → drain/sync storage → prepare input/wake → blank/quiesce display → prepare platform → sleep.
- [ ] Keep shared I2C available until all dependent peripherals and wake controllers finish preparation.
- [ ] Cancel pre-suspend pointer contacts deterministically; preserve cancellation ordering relative to subsequent wake reports.
- [ ] Cancel automatic transition if new user activity arrives before sleep. Restore brightness and deliver retained activity afterward.
- [ ] Recheck pending runtime events and asserted wake lines immediately before entry; close notification-versus-sleep race.
- [ ] Record successful suspend steps. Roll back in reverse order, including cleanup of partially completed failing callback.
- [ ] Require idempotent restoration and bounded completion. A timed-out worker remains owned until completion/cancellation acknowledged.
- [ ] Restore platform/buses, essential services, display/brightness, retained input, and finally application execution.
- [ ] Make reboot/power-off supersede transition. Restore only resources needed for safe shutdown; do not briefly resume application execution.
- [ ] On unrecoverable essential restoration failure, enter kernel panic outside normal power-state progression, keep applications parked, and report through surviving diagnostics.
- [ ] Disable further automatic suspend after callback failure until policy re-enabled or reboot. Ordinary blockers retry only when blocker state changes; no retry polling loop.

**Tests:** failure before/after every callback, partial failure cleanup, repeated rollback, shutdown at every transition boundary, and restoration failure panic.

## Phase 7 — Tab5 light sleep and time semantics

- [ ] Enable required PM/tickless configuration in pinned SDK.
- [ ] Use explicit coordinated sleep entry. Leave ESP-IDF automatic light sleep disabled so ordinary runtime blocking cannot bypass service contract.
- [ ] Keep CPU frequency unchanged initially. Add DFS as separate measured optimization after driver audit; no unvalidated frequency defaults.
- [ ] Add driver-local PM locks where required. Explicit sleep still requires manager’s own admission and quiesce checks.
- [ ] Retain flash/PSRAM and digital peripheral power in initial sleep profile; preserve board power-control levels.
- [ ] Arm validated keyboard GPIO50 wake; reject sleep if requested wake setup fails or no usable wake source remains.
- [ ] Treat already-asserted wake signal as activity/sleep rejection, avoiding immediate re-entry loops.
- [ ] Verify SDK monotonic compensation; do not add sleep duration twice.
- [ ] Preserve wall-clock UTC independently from monotonic time.
- [ ] During system suspend, exclude application waits, SDK sleep, cursor blink, retry, and maintenance deadlines from sleep wake timer.
- [ ] After another wake source fires, complete expired application waits and sleeps; run each overdue periodic service at most once and advance its next deadline.
- [ ] Keep explicit RTC wake alarms separate from application timeout semantics.
- [ ] Bring SDK sleep into cooperative parking contract and correct finite-deadline overflow while preserving existing API behavior.
- [ ] Advance synthetic host time through simulated sleep and exercise same expiry behavior.

**Hardware gate:** timer-controlled diagnostic sleep first, then keyboard wake; verify retained PSRAM code/data, both cores, watchdog behavior, storage, display, and foreground continuity.

## Phase 8 — Touch, RTC, power-button, and motion wake

- [ ] Add detected/supported/enabled/armed wake-source status with stable unavailable reasons.
- [ ] Implement GPIO23 touch wake per controller revision. Keep required touch/display controller circuitry powered and preserve first report after resume.
- [ ] Verify touch wake with down/move/up, rapid retouch, stationary contact, and multiple contacts.
- [ ] Implement RX8130 alarm programming, stale-flag clearing, cancellation, and future absolute-UTC validation.
- [ ] Respect alarm hardware resolution; reject unrepresentable requests instead of silently waking early.
- [ ] Verify RTC route actually resumes light sleep. If route only powers on/restarts board, mark transparent wake unsupported.
- [ ] Verify physical power-button behavior; do not override hardware shutdown/reset semantics or invent button-to-GPIO mapping.
- [ ] Complete existing Phase 3 IMU service prerequisite: BMI270 initialization, process-owned streams, specified sample rates/units, bounded queues, generic waits, cleanup, and host fixtures.
- [ ] Initially block suspend while normal IMU streams remain open.
- [ ] Add opt-in BMI270 motion arming with bounded configuration, reported effective hardware settings, interrupt clearing, and restoration of normal sensor mode.
- [ ] Validate motion false wakes while stationary and during transport before enabling any preset.
- [ ] Investigate every planned wake source. Document unsupported routes with evidence; support does not require wiring modifications.
- [ ] Disable unavailable or unverified sources on each board revision rather than advertising blanket support.

**Tests:** missing device, failed arming, stale alarm, simultaneous causes, shared interrupt attribution, wake exactly once, and repeated rearming.

## Phase 9 — Public controls and persistent configuration

- [ ] Add narrow `<tabos/power.h>` API for copied status/policy, policy updates, explicit suspend, wake-source configuration, and explicit save.
- [ ] Keep native driver types and raw GPIO/RTC registers below platform boundary.
- [ ] Add `powerctl status`, policy-setting commands, `suspend`, `save`, wake enable/disable, RTC alarm configuration, and motion configuration.
- [ ] Status reports state, effective brightness, timeout policy, blockers, source availability, last wake, last error, and measured transition counters.
- [ ] Explicit suspend parks caller and returns after successful resume; otherwise returns stable busy, unsupported, timeout, or I/O error.
- [ ] Permit one pending suspend request. Reject overlapping explicit requests deterministically.
- [ ] Fullscreen/media/safety blockers apply equally to explicit and automatic requests; no force-suspend option.
- [ ] Load bounded versioned `T:/etc/power.conf`; missing file uses agreed defaults.
- [ ] Apply runtime updates atomically. Invalid configuration leaves current policy unchanged.
- [ ] Save through temporary-file/sync/rename workflow; preserve unknown future fields following existing configuration conventions.
- [ ] Missing/unwritable storage does not prevent runtime configuration; save reports failure.
- [ ] Keep automatic suspend disabled unless explicitly enabled; preserve configured 10-minute default separately from enable flag.
- [ ] Extend pre-release ELF API and host RV32 dispatch together; rebuild bundled applications.
- [ ] Defer general process-owned inhibitor API until concrete application need; existing service ownership supplies initial blockers.

## Phase 10 — Validation, measurements, and milestone handoff

- [ ] Add unit suites for state machine, dependency graph, admission, configuration, activity policy, and deadline behavior.
- [ ] Add component suites for real service callbacks, filesystem synchronization, process parking, networking recovery, and retained input.
- [ ] Run at least 1,000 deterministic host suspend/resume cycles, including injected failures and concurrent operations.
- [ ] Verify stable process IDs, registry IDs, descriptors, wait sources, foreground ownership, parent/child state, framebuffer, and heap contents.
- [ ] Run supported host sanitizers, macOS/Linux Debug and Release tests, maintained RV32 tester, Tab5 Debug/Release builds, and standalone application builds.
- [ ] Perform at least 100 physical cycles on current board, including blocked attempts, rollback, nested processes, and TF integrity checks.
- [ ] Validate every supported wake source independently and in combinations.
- [ ] Record suspend/resume latency and wake-to-input delivery; investigate unexplained repeated wakeups.
- [ ] Measure active baseline, dimming, unused-audio shutdown, headphone-monitor reduction, tickless idle, DFS, and coordinated sleep separately.
- [ ] Record instrument, setup, charging state, repeated samples, and measurement variability. Require savings beyond measurement uncertainty before marking power optimization validated.
- [ ] Keep instrumented measurements pending with current equipment; onboard telemetry is supplementary evidence.
- [ ] Before completing the power milestone, reevaluate Phase 0 investigation diagnostics: the kernel `platform_runtime_log_activity()` hook and Debug codec/headphone/VSYNC/PPA counters. Decide whether to retain, make explicitly opt-in, or remove them; record rationale and overhead, preserve baseline evidence, and update affected tests/documentation. Keep the shared GPIO ISR ownership correctness fix independent of this decision.
- [ ] Keep ILI9881C/GT911, ST7123, and ST7121 recovery validation separately tracked. Do not enable unverified revision-specific sleep support.
- [ ] Update architecture, testing, context, roadmap, hardware-services completion tracking, and user-facing power/time/application documentation as phases land.
- [ ] Document paused application deadlines, Wi-Fi interruption, blockers, unsupported wake sources, panic behavior, and every enabled default.
- [ ] At planning handoff, replace `agents/milestone-power.md` with accepted phased checklist before implementation. Preserve `agents/hardware-services.md` Phase 8 as completion-status authority.
- [ ] Mark implementation, automated validation, physical validation, and measured savings separately. Builds alone do not complete milestone.

No repository edits during Plan mode. Deep sleep, hardware modifications, public inhibitor handles, and arbitrary forced application suspension remain deferred.
