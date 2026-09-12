# GUI Implementation Tasks

Status: implementation in progress, 2026-09-12. Contracts and the initial internal
process scheduling foundation are implemented; desktop and public concurrent SDK
launch are not yet available. Evidence below is scoped to completed work.

Direction and scope: [TabOS Retro Desktop milestone](../milestone-gui.md). This file tracks executable work packages; the milestone records agreed product behavior. Read [context](../TABOS_CONTEXT.md), [architecture](../architecture.md), [testing](../testing.md), and [roadmap](../roadmap.md) before implementation, plus [coding style](../coding-style.md) before C changes.

## Working Rules and Dependencies

- Work through phases in order. Each phase includes its own tests; final acceptance does not replace incremental validation.
- Mark a task complete only with implementation and its required automated evidence. Track physical validation separately and record unavailable hardware as pending.
- Keep desktop and GUI clients ordinary independent RV32 applications using public TabOS services. Kernel owns resources and launch; desktop owns window policy. Do not expose ESP-IDF, FreeRTOS, SDL, or private ELF transport to application code.
- Keep full release scope. An early compositor demo is an internal milestone, not the completed desktop.
- Resource quotas, staging strategy, and low-level protocol details need concrete specifications and measurements in Phase 0. These are explicit tasks, not already settled numerical contracts.

Current user restrictions: no Linux builds/tests, Tab5 flashing, host simulator
launch or UI automation. macOS and Tab5 builds and macOS automated suites are
allowed. Exclude `integration.host_smoke`, which launches the simulator binary.
Physical gates remain pending. Foundational work may proceed from recorded
contracts without claiming measured hardware acceptance.

## Phase 0 — Contracts and Resource Feasibility

Exit gate: ownership/lifecycle contracts are recorded and measured memory feasibility supports maximized independent clients before committing to the surface implementation.

- [x] GUI-001: Audit current process manager, SDK spawn/wait wrappers, host RV32 scheduling, native task guards, input ownership, graphics close, loader metadata, and build integration. Record relevant entry points and stale documentation in this task file.
- [x] GUI-002: Specify concurrent spawn, actual PID return, copied launch arguments, child-exit readiness, wait/reap, process-table exhaustion, and parent-exit cleanup. Preserve synchronous `tabos_exec()` and define the shell migration from the existing spawn wrapper.
- [x] GUI-003: Specify GUI-session membership, inherited membership for GUI-client descendants, exclusive display/input grants, and ownership restoration. Keep fullscreen handoff descendants outside the paused GUI-client set; prevent launches from escaping a pause/shutdown already in progress.
- [ ] GUI-004: Specify public copied IPC operations, endpoint discovery/grant handoff, queue/message limits, stale-handle behavior, generic waits, and disconnect semantics. Reserve bounded lifecycle/control delivery so data/input saturation cannot prevent pause, close, or recovery.
- [ ] GUI-005: Specify surface create/upload/commit/read-grant/release semantics, bounded staging, atomic visibility, damage bounds, read/commit synchronization, and abort/failure cleanup. Choose buffering from measurements rather than assuming two full buffers per client.
- [ ] GUI-006: Measure representative maximized RGB565 client memory on Tab5, including client canvas/heap, staging, retained surface, compositor, scanout, executable, and OS allocations. Measure prototype upload/composition time and existing Starfall/DOOM requirements; record peak/headroom and choose initial configurable resource limits. Do not promise a reserved game budget.
- [ ] GUI-007: Specify the minimal SDK-generated ELF GUI marker and pre-execution query through public services. Define absent-marker compatibility and malformed-marker errors using existing ELF metadata/validation conventions; marker grants no privileges.
- [ ] GUI-008: Record concurrent execution as the successor to initial foreground-only architecture/context decisions and scope old tests to synchronous execution. Keep PID 0 liveness, platform boundaries, current pre-release ABI policy, and shell recovery intact.

## Phase 1 — Concurrent Processes and Session Ownership

Exit gate: independent real RV32 clients progress on host and native tasks remain safe on Tab5; synchronous shell nesting still behaves as before.

- [ ] GUI-101: Separate runnable/blocked execution state from console ownership, keyboard focus, display ownership, and parent/child relationships.
- [ ] GUI-102: Implement asynchronous `tabos_spawn()`, child-owned launch data, actual PIDs, child-exit wait sources, status retention, and single reaping. Preserve synchronous `tabos_exec()`; update shell launch/wait handling.
- [ ] GUI-103: Schedule all runnable host RV32 contexts in bounded round-robin slices. Suspend blocked call gates without starving clients or service/runtime work; wake from existing readiness/deadline mechanisms.
- [ ] GUI-104: Extend managed Tab5 task lifecycle to concurrent processes. Audit caller identity, per-process libc/filesystem state, locking, cancellable waits, and stop-before-resource-release across multiple service callers.
- [ ] GUI-105: Implement session membership and owner-exit cleanup from GUI-003. Launch desktop as shell child with shell retained; reject unauthorized raw input/display access from background clients.
- [ ] GUI-106: Add deterministic process/session tests for two progressing clients, blocked waits, exit before wait, repeated reaping attempts, capacity failures, descendant cleanup, slot reuse, and cancellation during service calls. Retain nested shell/child/grandchild and PID 0 panic coverage.
- [ ] GUI-107: Extend maintained `tester` with concurrent RV32 child progress, wait/status, ownership, and repeated cleanup cases. Validate native task/service contention on physical Tab5 separately.

## Phase 2 — IPC and Retained Surfaces

Exit gate: separate RV32 clients publish coherent retained images through public services under bounded memory and queue pressure.

- [ ] GUI-201: Implement copied IPC endpoints, process ownership, endpoint grants/discovery, bounded queues, lifecycle/control delivery, and disconnect notification.
- [ ] GUI-202: Connect IPC readiness to generic waits on host and Tab5; cover lost-wakeup races, finite deadlines, cancellation, and stale handles.
- [ ] GUI-203: Implement OS-owned RGB565 surfaces, clipped/bounded rectangle uploads, staged commits, compositor read grants, and explicit release using GUI-005's contract.
- [ ] GUI-204: Enforce measured process/aggregate surface and staging limits. Preserve the last committed image on failed upload/commit; release endpoints, staging, surfaces, and grants during stopped-process teardown.
- [ ] GUI-205: Add public SDK wrappers and private native/host gates consistently; rebuild bundled apps under the current pre-release ABI policy.
- [ ] GUI-206: Add sanitizer-backed IPC/surface tests for saturation, lifecycle delivery under saturation, foreign/stale handles, invalid dimensions/rectangles, atomic visibility, reader/commit races, exit during commit, allocation failures, and repeated cleanup. Add real RV32 `tester` coverage.

## Phase 3 — Desktop, Dock, and Input Routing

Exit gate: desktop switches between independent window clients with usable touch controls and deterministic repaint/focus behavior.

- [ ] GUI-301: Build independent `desktop` application through the ordinary SDK/build/install path. Acquire session display/input through public services; draw decorations in desktop and content in clients.
- [ ] GUI-302: Implement opaque damage-based composition, clipped surface blits, and retained repaint using existing portable acceleration. Wait when idle; do not introduce a periodic idle repaint loop.
- [ ] GUI-303: Define and implement the 1280×720 touch layout: retro bitmap/beveled styling, large icons, generous spacing, and roughly 44-pixel minimum primary touch targets. Use modern phone/tablet layout cues without copying dense historical desktop layouts.
- [ ] GUI-304: Implement persistent bottom launcher/switcher dock and maximized client work area above it. Open apps maximized; support restore, minimize, dock restore/switching, stacking, focus, and close.
- [ ] GUI-305: Implement movable restored windows and touch-accessible outline resize. Commit dimensions/redraw only on release; failed resize preserves old geometry and committed pixels. Cancelled drags preserve a usable window.
- [ ] GUI-306: Extend normalized pointer contract and SDL backend with mouse hover/wheel. Route pointer events in client coordinates; capture contacts through release/cancel. Touch navigation must work without hover, right-click, or double-click.
- [ ] GUI-307: Route keyboard/text only to focused client; implement keyboard focus traversal/window switching. Cancel held keys/contacts on focus loss, device removal, overflow, close, and fullscreen transitions.
- [ ] GUI-308: Add deterministic framebuffer and synthetic input tests for maximized bounds/dock occlusion, overlap/clipping, damage, stacking, move/outline resize, failed resize, capture cancellation, focus traversal, and exactly-once text routing.

## Phase 4 — GUI SDK, Discovery, and First Applications

Exit gate: all first-release apps run as independent RV32 clients and use toolkit lifecycle/pause support rather than privileged shortcuts.

- [ ] GUI-401: Add portable C GUI SDK with connection/window lifecycle, bounded event loop, layout, invalidation, custom RGB565 canvas, and safe-point hooks for pause/resume and close. Keep application work bounded so control events can be handled promptly.
- [ ] GUI-402: Implement labels, buttons, checkboxes, menus, text fields, scrollbars, lists, and dialogs with shared touch/focus behavior. Use CP437 and physical-keyboard text entry; no on-screen keyboard dependency.
- [ ] GUI-403: Emit and query the minimal ELF GUI marker. Enumerate extensionless executables under `T:/bin`; generic icon and filename suffice. Marked apps use GUI launch; unmarked programs use fullscreen handoff after Phase 5.
- [ ] GUI-404: Implement launcher/file browser with directory navigation, explicit executable launch, and visible filesystem/launch errors. Keep file associations and full manifests deferred.
- [ ] GUI-405: Implement independent calculator with touch buttons and keyboard input, including clear error handling for invalid arithmetic.
- [ ] GUI-406: Implement independent small text editor with Open/Save, unsaved-change tracking, save/discard/cancel close flow, and retained document state across pause. Failed open/save preserves existing document and dirty state; reuse verified filesystem save patterns where appropriate.
- [ ] GUI-407: Ship standalone custom-canvas client demonstrating drawing beyond stock widgets, invalidation, resize, close, and pause/resume.
- [ ] GUI-408: Add toolkit/app tests covering focus/touch activation, bounded event processing, text routing, calculator errors, editor failed I/O and close cancellation, allocation failures, and repeated client launch/exit. Verify marker routing with marked and legacy unmarked RV32 fixtures.

## Phase 5 — Fullscreen Handoff and Recovery

Exit gate: kernel-launched fullscreen programs return to the same usable GUI session; failures have bounded cleanup and clear ownership restoration.

- [ ] GUI-501: Implement desktop → pause clients → fullscreen child → restore desktop → resume clients state machine. Keep coordinator and core services available while GUI workloads are parked.
- [ ] GUI-502: Pause the full GUI-client session, including descendants. Clients finish bounded work, release outstanding service leases, quiesce audio/capture, retain state, and acknowledge at toolkit safe points.
- [ ] GUI-503: Enforce two-second acknowledgement deadline. On timeout, abort launch, resume acknowledged clients, and identify blockers. Handle client exit/disconnect and late acknowledgements without leaving processes parked or accepting stale transitions.
- [ ] GUI-504: Launch fullscreen child through normal kernel process services, outside GUI surface quotas. Check headroom and handle actual load/allocation failures with GUI resident; explain insufficient RAM and permit closing apps before retry. Do not unload/checkpoint GUI or promise game-memory reservation.
- [ ] GUI-505: Transfer display/input exclusively to fullscreen child and its nested foreground chain. On normal exit, failed launch, or recoverable fault, reclaim child resources, restore prior display owner, repaint retained windows, clear stale input, and resume GUI clients.
- [ ] GUI-506: Implement orderly desktop exit honoring unsaved-work cancellation. Add explicit force-close with unsaved-data warning for hung clients; stop execution before reclaiming resources and include descendants in cleanup.
- [ ] GUI-507: Handle recoverable desktop failure by tearing down remaining GUI-session processes and any active handoff chain, restoring terminal ownership, and resuming shell. Keep PID 0 panic and arbitrary Tab5 native fault limitations explicit.
- [ ] GUI-508: Test repeated Starfall/DOOM and console-app handoffs with unsaved editor text and retained window positions. Inject pause timeout, late acknowledgement, saturated IPC, failed load, insufficient memory, recoverable child fault, client force-close, and desktop failure both during GUI operation and fullscreen handoff.

## Phase 6 — Delivery and Physical Acceptance

Exit gate: complete first-release experience, automated evidence, hardware results, and documentation agree. A compiling binary or screenshot is insufficient.

- [ ] GUI-601: Run relevant portable tests with ASan/UBSan and real RV32 integration through maintained `tester`. Complete macOS, Linux, and Tab5 Debug/Release builds using existing project workflows; verify ordinary app build/install and incremental SDK/marker/resource-setting rebuilds.
- [ ] GUI-602: Validate physical touch orientation, contact routing/cancellation, dock switching, and window controls on ILI9881C/GT911, ST7123, and ST7121 revisions. Record each revision independently.
- [ ] GUI-603: Validate large icons/targets and maximized/restored workflows on the actual 5-inch screen, including Tab5 keyboard text/modifiers and window navigation. Adjust spacing/targets from observed usability.
- [ ] GUI-604: Record input latency, damage-composition time, idle behavior, process heap/stack, PSRAM peak, staging/surface usage, and fullscreen game headroom. Exercise near-limit windows and repeated open/resize/close/handoff cycles; reconcile limits with Phase 0 measurements.
- [ ] GUI-605: Physically verify unsaved editor state survives repeated game/console launches, failed load returns usable desktop, orderly close can cancel, and force-close returns control without watchdog or resource leaks where the native fault boundary permits.
- [ ] GUI-606: Add user-facing desktop/GUI SDK documentation under `docs/` and update relevant application/build/input/graphics references and documentation index. Explain launch markers, touch/keyboard controls, quotas, resident-memory limits, recovery, and deferred features.
- [ ] GUI-607: Synchronize architecture/context/testing documents and roadmap after each implemented stage. Record commands, actual outcomes, platform coverage, measured limits, and remaining hardware gaps here or in linked validation records before marking delivery complete.

## Deferred Work

- Desktop auto-start, independent terminal windows, clipboard, drag-and-drop, full app manifests, transparency, and third-party GUI framework integration.
- GUI checkpoint/unload, switching away from a running fullscreen game, and arbitrary native crash recovery.
- USB HID backends and controller navigation mapping; these may later feed normalized input services and do not block the initial desktop.

## Implementation Evidence — Process Foundation

Contracts and audited entry points: [GUI service contracts](../gui-contracts.md).
GUI-004/005/007 remain open for precise protocol encoding, capacities and measured
buffering selection. GUI-006 physical measurements remain unavailable under the
current execution restrictions.

GUI-101/103/106 are partially implemented: process manager supports internal
asynchronous descriptor launch without foreground transfer, rotating bounded
snapshot dispatch, retained exit records, single reaping and descendant cleanup.
Public SDK spawn, ELF background launch and GUI sessions remain pending, so these
whole tasks are not checked complete.

macOS Debug build passed. ASan/UBSan tests passed: `unit.application_lifecycle`,
`unit.core_smoke`, `component.file_elf_loader`, `component.rv32_execution`,
`component.elf_graphics_cleanup`, `unit.native_task` and four architecture checks.
The lifecycle test covers two progressing children, foreground preservation,
foreign/repeated reaping, exit-before-wait, forced descendant cleanup, slot reuse,
unreaped table exhaustion and all existing nested/PID-0 cases. No simulator binary
was run. `./tools/tabos tab5 debug build` also passed; firmware has 12,016
bytes of app-partition headroom. No flashing or physical validation was performed.
