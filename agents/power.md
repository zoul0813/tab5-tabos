# TabOS Power Milestone Implementation Plan

## Agreed behavior

Deliver portable power manager and idle dimming first; coordinated light sleep follows after service safety and rollback tests pass.

| Policy | Decision |
|---|---|
| Idle dimming | Enabled by default after validation; 60 seconds inactivity, 20% brightness |
| Brightness restoration | Restore previous active brightness before delivering activity |
| Automatic suspend | Disabled by default; 10 minutes inactivity when enabled |
| Configuration | Runtime changes immediate; explicit `powerctl save` persists settings |
| Fullscreen graphics | Blocks dimming and every suspend request |
| Media | Open audio/camera streams block suspend; initially also inhibit dimming |
| Wi-Fi | May disconnect before suspend and reconnect afterward; IP continuity not promised |
| Application deadlines | Continue aging but do not wake suspended system; complete after another wake |
| Wake sources | Investigate keyboard, touch, power button, RTC alarm, and BMI270 motion |
| Motion wake | Opt-in |
| Unsupported wake hardware | Document evidence and report unavailable; no speculative registration |
| Unrecoverable essential-service resume failure | Panic, keep applications parked, require explicit reset/reboot |
| Deep sleep | Deferred; no process-persistence or reboot-style hibernation implementation |

“Suspend” means transparent light sleep: preserve RAM, process stack, foreground ownership, handles, and boot-local device identities.

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

- [ ] Record activity at normalized keyboard/pointer ingress before foreground delivery.
- [ ] Count physical presses/releases, pointer movement/contact changes, and button events. Software key repeat, cursor blink, background output, and service completions do not reset inactivity.
- [ ] Treat held keys and active pointer contacts as temporary dim/suspend inhibitors; avoid dimming beneath stationary interaction.
- [ ] Apply 60-second idle deadline and 20% brightness. Never increase brightness when active setting already below 20%.
- [ ] Preserve active brightness independently from effective dimmed brightness; restore before delivering activity.
- [ ] Immediately restore brightness when fullscreen ownership or inhibiting media starts.
- [ ] On final inhibitor release, begin fresh inactivity interval; avoid immediate dim/suspend after long playback.
- [ ] Preserve framebuffer, display ownership, terminal contents, and input ordering.
- [ ] Simulate dimming in SDL presentation without modifying framebuffer pixels.
- [ ] Report brightness failures and keep logical/effective brightness truthful.

**Tests:** exact timeout boundary, activity races, held input, graphics/media acquisition and release, configuration changes while idle, and brightness failure.

## Phase 3 — Remove avoidable idle service work

- [ ] Make audio hardware processing demand-driven: stop codec transfers when no audio streams exist; restart before admitting first stream.
- [ ] Preserve sample-rate arbitration, routing, capture/playback behavior, and error reporting across worker restarts.
- [ ] Run headphone monitoring only while speaker routing needs detection. Sample jack state before enabling speaker.
- [ ] Retain existing active-playback detection latency unless measurements support a change; no headphone polling while audio hardware suspended.
- [ ] Keep 60-second health audit during normal operation initially; suppress it during suspend and perform one overdue audit after resume.
- [ ] Quiesce display scanout/VSYNC during system suspend; preserve display data and allocations.
- [ ] Measure each optimization separately. Do not claim whole-system idle savings from runtime wake counters alone.

**Tests:** repeated first-open/last-close, idle audio silence, route correctness, fault recovery, and no new worker spin loops.

**Delivery gate:** manager plus dimming can ship independently once functional and measurement requirements pass.

## Phase 4 — Admission freeze and process safe points

- [ ] Add short-lived synchronization around admission state and operation counters. Never hold power-manager lock across driver I/O or callbacks.
- [ ] Once transition starts, prevent new unsafe operations from racing past completed preflight.
- [ ] Add cooperative parking handshake to native yield, supported blocking waits, and explicit suspend request.
- [ ] Park only where application owns no kernel/service lock or active hardware operation. Do not forcibly suspend arbitrary native instruction execution.
- [ ] Track every live process, including parents waiting for children. Preserve nesting, child status, foreground owner, and process-0 invariant.
- [ ] Stop host RV32 execution only at equivalent safe boundaries; host instruction slicing must not conceal native noncooperation.
- [ ] Wake blocked application waits into parking handshake without exposing false cancellation or consuming their readiness.
- [ ] Use two-second default parking/drain deadline. On timeout, report stable blocker, release freeze, and restore operation.
- [ ] Freeze process launch/foreground changes during transition; prioritize existing exit, reboot, and power-off requests.
- [ ] Ensure process teardown releases counters, pending requests, and parking state exactly once.

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
