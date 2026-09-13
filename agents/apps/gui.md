# GUI Implementation Tasks

Status: software implemented and permitted automated validation refreshed; native/physical
release acceptance remains pending, 2026-09-13. Desktop, Files,
Calculator, Text editor and Canvas are implemented as independent RV32 applications.
Concurrent processes, session pause/restore, IPC, surfaces, launch inspection and
GUI toolkit are integrated. Service and fullscreen failure-injection coverage is recorded below. Remaining gates
include native contention and physical touch/performance/memory acceptance under user restrictions.

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
- [x] GUI-004: Specify public copied IPC operations, endpoint discovery/grant handoff, queue/message limits, stale-handle behavior, generic waits, and disconnect semantics. Reserve bounded lifecycle/control delivery so data/input saturation cannot prevent pause, close, or recovery.
- [ ] GUI-005: Specify surface create/upload/commit/read-grant/release semantics, bounded staging, atomic visibility, damage bounds, read/commit synchronization, and abort/failure cleanup. Choose buffering from measurements rather than assuming two full buffers per client.
- [ ] GUI-006: Measure representative maximized RGB565 client memory on Tab5, including client canvas/heap, staging, retained surface, compositor, scanout, executable, and OS allocations. Measure prototype upload/composition time and existing Starfall/DOOM requirements; record peak/headroom and choose initial configurable resource limits. Do not promise a reserved game budget.
- [x] GUI-007: Specify the minimal SDK-generated ELF GUI marker and pre-execution query through public services. Define absent-marker compatibility and malformed-marker errors using existing ELF metadata/validation conventions; marker grants no privileges.
- [x] GUI-008: Record concurrent execution as the successor to initial foreground-only architecture/context decisions and scope old tests to synchronous execution. Keep PID 0 liveness, platform boundaries, current pre-release ABI policy, and shell recovery intact.

## Phase 1 — Concurrent Processes and Session Ownership

Exit gate: independent real RV32 clients progress on host and native tasks remain safe on Tab5; synchronous shell nesting still behaves as before.

- [x] GUI-101: Separate runnable/blocked execution state from console ownership, keyboard focus, display ownership, and parent/child relationships.
- [x] GUI-102: Implement asynchronous `tabos_spawn()`, child-owned launch data, actual PIDs, child-exit wait sources, status retention, and single reaping. Preserve synchronous `tabos_exec()`; update shell launch/wait handling.
- [x] GUI-103: Schedule all runnable host RV32 contexts in bounded round-robin slices. Suspend blocked call gates without starving clients or service/runtime work; wake from existing readiness/deadline mechanisms.
- [ ] GUI-104: Extend managed Tab5 task lifecycle to concurrent processes. Audit caller identity, per-process libc/filesystem state, locking, cancellable waits, and stop-before-resource-release across multiple service callers.
- [x] GUI-105: Implement session membership and owner-exit cleanup from GUI-003. Launch desktop as shell child with shell retained; reject unauthorized raw input/display access from background clients.
- [x] GUI-106: Add deterministic process/session tests for two progressing clients, blocked waits, exit before wait, repeated reaping attempts, capacity failures, descendant cleanup, slot reuse, and cancellation during service calls. Retain nested shell/child/grandchild and PID 0 panic coverage.
- [x] GUI-107: Extend maintained `tester` with concurrent RV32 child progress, wait/status, ownership, and repeated cleanup cases. Physical native contention remains tracked by GUI-104 and Phase 6.

## Phase 2 — IPC and Retained Surfaces

Exit gate: separate RV32 clients publish coherent retained images through public services under bounded memory and queue pressure.

- [x] GUI-201: Implement copied IPC endpoints, process ownership, endpoint grants/discovery, bounded queues, lifecycle/control delivery, and disconnect notification.
- [ ] GUI-202: Connect IPC readiness to generic waits on host and Tab5; cover lost-wakeup races, finite deadlines, cancellation, and stale handles.
- [x] GUI-203: Implement OS-owned RGB565 surfaces, clipped/bounded rectangle uploads, staged commits, compositor read grants, and explicit release using GUI-005's contract.
- [ ] GUI-204: Enforce measured process/aggregate surface and staging limits. Preserve the last committed image on failed upload/commit; release endpoints, staging, surfaces, and grants during stopped-process teardown.
- [x] GUI-205: Add public SDK wrappers and private native/host gates consistently; rebuild bundled apps under the current pre-release ABI policy.
- [x] GUI-206: Add sanitizer-backed IPC/surface tests for saturation, lifecycle delivery under saturation, foreign/stale handles, invalid dimensions/rectangles, atomic visibility, reader/commit races, exit during commit, allocation failures, and repeated cleanup. Add real RV32 `tester` coverage.

## Phase 3 — Desktop, Dock, and Input Routing

Exit gate: desktop switches between independent window clients with usable touch controls and deterministic repaint/focus behavior.

- [x] GUI-301: Build independent `desktop` application through the ordinary SDK/build/install path. Acquire session display/input through public services; draw decorations in desktop and content in clients.
- [x] GUI-302: Implement opaque damage-based composition, clipped surface blits, and retained repaint using existing portable acceleration. Wait when idle; do not introduce a periodic idle repaint loop.
- [x] GUI-303: Define and implement the 1280×720 touch layout: retro bitmap/beveled styling, large icons, generous spacing, and roughly 44-pixel minimum primary touch targets. Use modern phone/tablet layout cues without copying dense historical desktop layouts. Physical usability acceptance remains GUI-603.
- [x] GUI-304: Implement persistent bottom launcher/switcher dock and maximized client work area above it. Open apps maximized; support restore, minimize, dock restore/switching, stacking, focus, and close.
- [x] GUI-305: Implement movable restored windows and touch-accessible outline resize. Commit dimensions/redraw only on release; failed resize preserves old geometry and committed pixels. Cancelled drags preserve a usable window.
- [x] GUI-306: Extend normalized pointer contract and SDL backend with mouse hover/wheel. Route pointer events in client coordinates; capture contacts through release/cancel. Touch navigation must work without hover, right-click, or double-click.
- [x] GUI-307: Route keyboard/text only to focused client; implement keyboard focus traversal/window switching. Cancel held keys/contacts on focus loss, device removal, overflow, close, and fullscreen transitions.
- [x] GUI-308: Add deterministic framebuffer and synthetic input tests for maximized bounds/dock occlusion, overlap/clipping, damage, stacking, move/outline resize, failed resize, capture cancellation, focus traversal, and exactly-once text routing.

## Phase 4 — GUI SDK, Discovery, and First Applications

Exit gate: all first-release apps run as independent RV32 clients and use toolkit lifecycle/pause support rather than privileged shortcuts.

- [x] GUI-401: Add portable C GUI SDK with connection/window lifecycle, bounded event loop, layout, invalidation, custom RGB565 canvas, and safe-point hooks for pause/resume and close. Keep application work bounded so control events can be handled promptly.
- [x] GUI-402: Implement labels, buttons, checkboxes, menus, text fields, scrollbars, lists, and dialogs with shared touch/focus behavior. Use CP437 and physical-keyboard text entry; no on-screen keyboard dependency.
- [x] GUI-403: Emit and query the minimal ELF GUI marker. Enumerate extensionless executables under `T:/bin`; generic icon and filename suffice. Marked apps use GUI launch; unmarked programs use fullscreen handoff after Phase 5.
- [x] GUI-404: Implement launcher/file browser with directory navigation, explicit executable launch, and visible filesystem/launch errors. Keep file associations and full manifests deferred.
- [x] GUI-405: Implement independent calculator with touch buttons and keyboard input, including clear error handling for invalid arithmetic.
- [x] GUI-406: Implement independent small text editor with Open/Save, unsaved-change tracking, save/discard/cancel close flow, and retained document state across pause. Failed open/save preserves existing document and dirty state; reuse verified filesystem save patterns where appropriate.
- [x] GUI-407: Ship standalone custom-canvas client demonstrating drawing beyond stock widgets, invalidation, resize, close, and pause/resume.
- [x] GUI-408: Add toolkit/app tests covering focus/touch activation, bounded event processing, text routing, calculator errors, editor failed I/O and close cancellation, allocation failures, and repeated client launch/exit. Verify marker routing with marked and legacy unmarked RV32 fixtures.

## Phase 5 — Fullscreen Handoff and Recovery

Exit gate: kernel-launched fullscreen programs return to the same usable GUI session; failures have bounded cleanup and clear ownership restoration.

- [x] GUI-501: Implement desktop → pause clients → fullscreen child → restore desktop → resume clients state machine. Keep coordinator and core services available while GUI workloads are parked.
- [x] GUI-502: Pause the full GUI-client session, including descendants. Clients finish bounded work, release outstanding service leases, quiesce audio/capture, retain state, and acknowledge at toolkit safe points.
- [x] GUI-503: Enforce two-second acknowledgement deadline. On timeout, abort launch, resume acknowledged clients, and identify blockers. Handle client exit/disconnect and late acknowledgements without leaving processes parked or accepting stale transitions.
- [x] GUI-504: Launch fullscreen child through normal kernel process services, outside GUI surface quotas. Check headroom and handle actual load/allocation failures with GUI resident; explain insufficient RAM and permit closing apps before retry. Do not unload/checkpoint GUI or promise game-memory reservation.
- [x] GUI-505: Transfer display/input exclusively to fullscreen child and its nested foreground chain. On normal exit, failed launch, or recoverable fault, reclaim child resources, restore prior display owner, repaint retained windows, clear stale input, and resume GUI clients.
- [x] GUI-506: Implement orderly desktop exit honoring unsaved-work cancellation. Add explicit force-close with unsaved-data warning for hung clients; stop execution before reclaiming resources and include descendants in cleanup.
- [x] GUI-507: Handle recoverable desktop failure by tearing down remaining GUI-session processes and any active handoff chain, restoring terminal ownership, and resuming shell. Keep PID 0 panic and arbitrary Tab5 native fault limitations explicit.
- [x] GUI-508: Test repeated Starfall/DOOM and console-app handoffs with unsaved editor text and retained window positions. Inject pause timeout, late acknowledgement, saturated IPC, failed load, insufficient memory, recoverable child fault, client force-close, and desktop failure both during GUI operation and fullscreen handoff.

## Phase 6 — Delivery and Physical Acceptance

Exit gate: complete first-release experience, automated evidence, hardware results, and documentation agree. A compiling binary or screenshot is insufficient.

- [x] GUI-601: Run permitted portable tests with ASan/UBSan and real RV32 integration through maintained `tester`. Complete macOS and Tab5 Debug/Release builds using existing project workflows; verify ordinary app build/install and incremental SDK/marker/resource-setting rebuilds. Linux and simulator smoke execution are excluded by user instruction, not claimed as validated.
- [ ] GUI-602: Validate physical touch orientation, contact routing/cancellation, dock switching, and window controls on ILI9881C/GT911, ST7123, and ST7121 revisions. Record each revision independently.
- [ ] GUI-603: Validate large icons/targets and maximized/restored workflows on the actual 5-inch screen, including Tab5 keyboard text/modifiers and window navigation. Adjust spacing/targets from observed usability.
- [ ] GUI-604: Record input latency, damage-composition time, idle behavior, process heap/stack, PSRAM peak, staging/surface usage, and fullscreen game headroom. Exercise near-limit windows and repeated open/resize/close/handoff cycles; reconcile limits with Phase 0 measurements.
- [ ] GUI-605: Physically verify unsaved editor state survives repeated game/console launches, failed load returns usable desktop, orderly close can cancel, and force-close returns control without watchdog or resource leaks where the native fault boundary permits.
- [x] GUI-606: Add user-facing desktop/GUI SDK documentation under `docs/` and update relevant application/build/input/graphics references and documentation index. Explain launch markers, touch/keyboard controls, quotas, resident-memory limits, recovery, and deferred features.
- [x] GUI-607: Synchronize architecture/context/testing documents and roadmap after each implemented stage. Record commands, actual outcomes, platform coverage, measured limits, and remaining hardware gaps here or in linked validation records before marking delivery complete.

## Deferred Work

Marker/query evidence: metadata version 2 uses offset 24 bit 0 for GUI; version 1
and absent metadata remain unmarked. Seven macOS Debug loader/SDK/boundary tests
pass, covering malformed metadata and query output preservation on failure.
Real RV32 tester queries itself and a missing executable before concurrency rounds.
macOS Debug and Tab5 Debug builds pass; Tab5 retains 5,648 partition bytes.
Desktop launch routing remains GUI-403 work.

Session lifecycle increment: kernel closes admission before enumerating inherited
members, assigns monotonic pause tokens and rolls back after two seconds with a
blocking PID. Clients acknowledge at a public safe-point gate that retains their
execution and memory until resume; runtime closes caller audio/camera streams before
publishing readiness. Only the coordinator may resume or force-close session members.
Fullscreen exec requires a parked session and creates an outside-session chain.
Forced termination of a blocked coordinator unwinds that chain before cleanup;
handoff clears queued keyboard input and repeat state.

Eight targeted macOS Debug process/input/native/boundary tests pass, including
descendant admission, timeout, stale acknowledgements, force-close and nested recovery.
Real RV32 tester pauses and resumes two live clients across three rounds while
surfaces remain resident. Native physical contention and complete desktop handoff
remain pending; these checks do not claim final GUI acceptance.
macOS Debug and Tab5 Debug builds pass; Tab5 has 4,064 app-partition bytes free.

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

### Concurrent ELF Transport Evidence

The next increment implements actual SDK spawn PIDs and wait/reap, copied loader
requests, background ELF startup with no console grant, graphics-open ownership
checks, shell migration to synchronous exec and native mapping-table synchronization.
GUI-102 remains open for generic child-exit wait sources and complete error mapping;
GUI-104/105 remain open for full service/session audit and physical contention.

`unit.sdk_process` checks validation, multiple outstanding children, pending launch
and wait replies, status preservation, repeated reaping and POSIX errno adaptation.
`component.shell_session` retains command/history regression using synchronous exec.
`tabos_process_rv32 build/apps/tester/tester` passes ASan/UBSan with SDK-built tester:
three pairs rendezvous while parent waits, background display is denied, exact
statuses return and the root regains console with only one process left.

macOS and Tab5 Debug/Release builds pass. `./apps/build.sh build --with-doom`
rebuilt all bundled RV32 apps. Debug macOS suite excluding simulator/Linux: 82/83
passed; Lua touch timed out during a run with large wall-clock stalls, then passed
an isolated rerun in 0.17 seconds. Ten targeted Release process/shell/native/boundary
tests and the Release real-RV32 concurrency harness passed. Tab5 firmware remaining
app-partition headroom: Debug 10,704 bytes; Release 113,264 bytes. No Linux build/test,
flash or simulator executable was run.

### Session IPC Evidence

Public `tabos_session_open()` establishes a non-root foreground coordinator;
concurrent descendants inherit its session. Session-owned listener/connect/accept
create paired channels with 32 endpoint slots, eight pending accepts, eight data
messages and two separate control messages per endpoint, with 224-byte payloads.
Native and RV32 gates, SDK wrappers, generic waits and owner cleanup are integrated.
Admission closure during pause/shutdown remains GUI-105/502 work.

`unit.ipc` passes ASan/UBSan: session/foreign/stale denial, copied sender identity,
FIFO data, control priority under full data queues, independent capacity bounds,
peer hangup, closed-peer errors, listener-backlog cleanup and 100 reuse rounds.
The SDK-built real RV32 tester passes three session/IPC rounds, including listener
waits, channel waits, authenticated sender PIDs, control replies and drain-after-close.
Seven IPC/process/native/boundary tests pass; macOS Debug and Tab5 Debug builds
pass. Tab5 Debug retains 8,720 bytes of app-partition headroom.

### Child-Exit Wait Sources

`tabos_process_wait_source(pid)` now supplies generic READABLE readiness for an
exited direct child until reaping. Native gates inspect copied state behind a
short mutex; runtime callbacks/cleanup never run while that mutex is held.
Reaping invalidates its source. Spawn capacity/PID exhaustion returns EAGAIN;
invalid requests return EINVAL, busy launch EBUSY and load failure EIO.

Eight targeted macOS Debug SDK/process/IPC/native/boundary tests pass. The actual
RV32 tester verifies finite child-exit waits and stale-source rejection after
reaping across three IPC/concurrency rounds. macOS Debug and Tab5 Debug builds
pass; Tab5 Debug app-partition headroom is 8,064 bytes. GUI-104/105/106/107 still
track the remaining full service/session/cancellation and hardware coverage.

### Retained Surface Prototype Evidence

GUI-203 is implemented with public SDK/native/RV32 gates: copied RGB565 uploads,
lazy transaction staging, atomic committed-image swap, IPC peer read grants,
explicit abort/release and owner cleanup. Configurable provisional budgets are
12 MiB aggregate and 6 MiB per process, including retained and staging buffers.
GUI-005/006/204 remain open for measured physical resource selection/acceptance;
these prototype limits are explicitly not measured Tab5 production limits.

Eight targeted macOS Debug surface/IPC/process/native/boundary tests pass.
`unit.surface` covers invisible staging, owner/read-grant separation, invalid upload
rollback, stale handles, quota failure with old pixels preserved, and concurrent
reader versus 500 committed frames. Real RV32 tester reads granted pixels while
new pixels are staged and checks explicit plus leaked staging cleanup to baseline.
macOS Debug and Tab5 Debug builds pass; Tab5 app-partition headroom is 6,016 bytes.
No Linux build/test, flashing or simulator executable was run.

### Shared Filesystem Concurrency

Replaced the filesystem spinlock around storage operations with the platform mutex.
Initialization/shutdown remain runtime lifecycle operations before/after application
tasks. macOS filesystem tests now link the real SDL mutex; four threads each perform
100 create/write/read/close/unlink rounds and verify thread-local errno isolation.
Eight filesystem/process/boundary tests pass under ASan/UBSan; macOS Debug builds.
The native service audit remains open for network cancellation ownership.
Tab5 Debug build passes with 4,048 app-partition bytes free.

### Native Cancellation Audit

Native DNS/echo, socket and TLS worker requests publish their caller identity;
process stop cancels only that owner. Worker mutex waiters check their own stop
state, so stopping a queued caller does not interrupt another owner. Native socket
cleanup relies on joined synchronous requests and no longer takes an unrelated
workers mutex. TLS cleanup may still wait for another bounded TLS operation;
physical contention/latency acceptance remains open. PID 0 panic now stops and
reclaims background descendants while retaining the panicked root.

Ten targeted process/native/network/boundary tests pass; eight focused panic and
cancellation tests also pass after root cleanup changes. Native socket-loop tests
verify foreign cancellation leaves accept/receive/infinite wait running, matching
owner cancellation stops them, and late cancellation cannot affect the next owner.
macOS Debug and Tab5 Debug builds pass. No simulator, Linux or flashing was used.

### GUI Client SDK and Drawing Foundation

Added a versioned copied window protocol and portable SDK client lifecycle. Clients
publish OS surfaces, carry geometry serials with input and retain old surfaces until
compositor acknowledgement. Failed resize restores old geometry/canvas and preserves
committed pixels. The bounded event loop checks kernel pause safe points, supports
close confirmation/cancellation and retries queued lifecycle replies. Idle waits are
bounded to 100 ms for cooperative pause detection; idle does not repaint.

Portable drawing uses the existing CP437 8x12 bitmap, RGB565 clipped fills and bevels.
Initial controls include labels/buttons/checkboxes/text fields/lists/scrollbars,
contact capture and keyboard focus/text handling. Menus/dialogs, richer editing,
complete client applications and desktop integration remain unfinished; GUI-401/402
are not checked complete yet.

Six macOS Debug drawing/client/boundary tests pass under ASan/UBSan. Client tests
use production IPC/surface services with deterministic gate/wait adapters, verifying
publication, deferred old-surface release, failed upload resize rollback, close
cancellation, saturated control reply retry and zero remaining surface allocations.
Input tests cover clipping guards, CP437 drawing, capture cancellation, focus,
exactly-once printable text, buffer bounds and cursor preservation across layout.
The SDK tester cross-build compiles the new GUI sources; real RV32 GUI execution
remains the next integration gate. These tests do not launch the host simulator.

### Desktop and Canvas Integration

Added the independent desktop coordinator, retained damage compositor, window
stack/focus/dock, move/outline resize, close/force-close controls and resident
fullscreen handoff. Shutdown closes session launch admission while clients remain
runnable for confirmation. Canvas retains normalized strokes and exercises custom
drawing and fullscreen launch through the public SDK.

The dedicated macOS Debug RV32 component harness executes the actual desktop and
Canvas ELF images without launching the host simulator. It verifies pointer launch,
drawing, restore/maximize with old-surface release, fullscreen execution while the
desktop is blocked and its surface retained, restoration of exact drawing pixels,
client cleanup and desktop exit to the persistent root. Desktop model tests cover
stacking, bounds, resize adoption/failure and drag cancellation. Physical touch,
layout and memory acceptance remain pending; Files/calculator/editor remain next.

### GUI Applications, Input and Save Safety

Files, Calculator and Text editor now join Canvas as independent GUI clients. Files
uses public directory listing and executable query; Calculator has bounded arithmetic
with precedence and invalid-result errors. Editor uses a retained 32767-byte document,
Open/Save/Save as, menu and dirty Save/Cancel/Discard flows. Saves stage the full file,
move an existing destination to a backup for FAT compatibility, and roll back failed
replacement; rollback failure preserves recovery copies and identifies suffixes.

Toolkit now includes popup menus, modal panels, visible caret, multiline wrapping,
vertical navigation, touch caret placement and list keyboard/wheel navigation.
Normalized hover/wheel events reach public SDK and Lua clients. Minimum restored
window size is provisionally 320x400, keeping touch controls visible. Desktop cancels
input on modal/focus transitions and suppresses error dialogs after explicit force-close.

Automated evidence: renderer tests verify overlap, dock coverage, precise damage and
guard pixels; menu/text tests cover activation and multiline navigation. Editor tests
inject read, write, close and rename failure, partial writes, rollback, binary/oversized
Open and arithmetic errors. Dedicated real RV32 GUI harness verifies all four client
launches, editor Save and dirty-close Cancel/Discard in addition to Canvas resize and
fullscreen retention. Further recovery cases are being added.

macOS Release passed all 89 permitted CTest cases. Debug passed 88/89 initially;
build.application_tracking failed because its synthetic SDK omitted new source files.
Updated that fixture and added GUI-marker rebuild/no-rebuild checks; focused rerun
passed all three build/application tests. All bundled apps, including Doom, cross-built.
Tab5 Debug/Release builds pass with 3776 / 106416 partition bytes free. No physical
validation, Linux build/test, flashing or host simulator execution was performed.

Recovery increment: real RV32 tests now retain dirty editor text while a second
Canvas client launches an unmarked fullscreen executable, then verify Cancel and
Discard after return. Two additional rounds terminate the actual desktop while
clients run and while a fullscreen child is active; both restore root console,
return the forced status and free all surfaces. The complete macOS Debug suite now
passes 89/89 permitted cases (ASan/UBSan); real RV32 tester concurrency also passes.

### Delivery Validation and Remaining Acceptance

GUI protocol v2 fences cancelled input by issuance sequence, independently of
geometry, so priority cancellation cannot be followed by stale queued presses.
A deterministic client test queues old input, priority Cancel and new input; only
new input reaches the client. Keyboard queue overflow clears old events/repeat state
and exposes a public flag; desktop, Starfall and Doom cancel held input. Desktop
also subscribes to keyboard/pointer lifecycle and overflow events. Controls were
reviewed from an actual RV32 framebuffer; toolbar labels now fit and dock titles
ellipsize. Home cards include large bitmap-style icons drawn through public graphics.

Final macOS Debug and Release suites each pass all 89 permitted CTest cases.
Debug uses ASan/UBSan. SDK applications, including Doom, rebuilt successfully.
Tab5 Debug/Release compile successfully with 3744 / 106368 app-partition bytes free.
The standalone real RV32 process tester passes three concurrent session rounds.
GUI RV32 coverage includes desktop Exit cancellation, explicit dirty-client force
close, and coordinator termination both during GUI use and a fullscreen handoff.

Earlier stage evidence above is historical, not a claim that pending work was later
skipped. Unchecked tasks remain acceptance work: measured buffering/resource choices,
physical native contention/latency, exhaustive IPC/wait/allocation/fault injection,
and physical display/controller usability and game headroom. Linux coverage is
explicitly excluded by the user's instruction; no Linux builds/tests, Tab5 flashing,
host simulator binary or UI automation were run. These limits prevent declaring
all milestone release gates complete, despite the implemented software workflow.

Real-game validation: dedicated Debug RV32 harness passed two normal Starfall
handoffs and two Doom/Freedoom2 handoffs, one with dirty editor plus Canvas resident,
followed by desktop-failure teardown during active handoff. Game exit status is
explicitly asserted zero. An intermediate Starfall run overlapped the SDK rebuild
and copied the old 87017-byte image, returning status 1; rerun after build completion
used the matching 87041-byte image and passed. The harness now checks status as well
as return ownership. Release GUI workflow also passes. These are automated emulation
results, not native game performance or physical-memory measurements.

The five GUI applications also pass their ordinary SDK `install` targets into the
local `.local/rootfs/T/bin` directory. No mounted-device installation was requested
or performed. Use matching rebuilt SDK binaries with matching firmware; pointer and
keyboard structures changed under the existing mutable prerelease ABI policy.

## Follow-up: macOS pointer burst backpressure

- [x] Reproduce the reported input-queue dialog with a 48-hover burst followed by a click in the standalone RV32 GUI component test.
- [x] Add bounded per-window pending input, motion coalescing, FIFO retry on IPC EAGAIN, writable waits and cancellation cleanup. Keep discrete events ordered and report other transport errors accurately.
- [x] Add deterministic tests for motion coalescing around press/release, saturation, repeated eight-message drains without duplication and peer disconnect.

The regression fails against the previous desktop binary: the expected Canvas ink
pixel is replaced by the error dialog. The updated desktop passes the same burst
and the complete standalone GUI workflow in macOS Debug and Release. Seven focused
CTest cases pass in each configuration, including the new desktop-input test,
GUI client cancellation, desktop model, pointer services, keyboard queue and public
API boundary. The SDK desktop binary was rebuilt; kernel and wire ABI are unchanged.
No Linux tests, host simulator launch/control or hardware flashing were performed.

## Follow-up: GUI Application Source Organization

- [x] Move Desktop, Files, Canvas and Editor into `apps/gui/`, retaining independent SDK projects and existing executable/output/install names.
- [x] Extend application build, install and MSC packaging discovery to include `apps/gui/*/Makefile`; update test source paths and recursive API boundary coverage.
- [x] Update contributor documentation for the source layout and individual build commands.

Validation: `./apps/build.sh build` passes for the default application set. All four
relocated projects pass their individual SDK install targets with byte-identical
local installed outputs. The affected macOS Debug test targets rebuild; desktop
model/input, GUI apps, application build tracking and public API boundary checks
pass (five tests). Shell syntax and Git whitespace checks pass. No Linux tests,
host simulator launch/control, MSC copy or hardware flashing was performed.

## Follow-up: IPC and Surface Failure Injection

- [x] GUI-206: Inject failure of each IPC connection queue allocation; verify partial connection rollback, empty accept readiness, successful retry and zero leaked queues across 100 rounds.
- [x] GUI-206: Exhaust the endpoint table with one slot remaining, repeat failed pair creation and prove the remaining slot is reusable; reject stale listener handles after shutdown/reinitialization.
- [x] GUI-202/206: Verify writable readiness stays blocked while only control messages drain, returns when data capacity frees, and queued control/data remain readable with hangup until drained after peer teardown.
- [x] GUI-206: Inject committed-surface and staging allocation failures across 100 rounds; preserve committed pixels/revision/accounting, retry successfully and reclaim retained plus unfinished staging buffers during owner teardown.
- [x] GUI-202/206: Add host RV32 IPC waits covering readiness before wait and after empty poll/before runtime sleep, finite deadlines under repeated unrelated wakeups, peer close, endpoint replacement and forced blocked-child cleanup.
- [x] GUI-202/206: Add maintained SDK tester cases for empty/ready listener, channel timeout, queued control with hangup, close invalidation and stale source rejection after reuse.
- [ ] GUI-202/206: Validate native Tab5 wait cancellation/interleavings and remaining service-contention cases.
- [x] GUI-206: Add service-level concurrent teardown/commit and invalid-input failure coverage.
- [x] GUI-206: Extend maintained real RV32 tester with invalid surface transfer and revoked-grant rejection cases.

Both `unit.ipc` and `unit.surface` pass in macOS Debug (ASan/UBSan) and Release.
Allocation interception is local to the test translation units; production services
and public ABI are unchanged. The surface fixture uses the build's generated GUI
quota configuration. These checks narrow the remaining work without completing
GUI-202 or GUI-206. No Linux builds/tests, simulator launch/control or Tab5 flashing
were performed. Kilo launcher changes remain deferred by user request.

### Generic IPC Wait Evidence

`component.elf_wait` now provisions guest-owned IPC endpoints in its fixture and
executes the actual RV32 wait-source and generic-wait gates. It injects a message
after the first empty poll, before runtime sleep, verifies a 70 ms deadline under
repeated application wakeups, and repeats message, hangup, endpoint replacement
and forced-exit cases three times. Endpoint replacement rejects the old source
with EBADF; forced termination restores the parent and leaves peer hangup.

Maintained `tester --concurrent` adds three public-SDK listener/channel lifecycle
rounds, including 20 ms timeout, control delivery after peer close, exact readiness
bits and stale-source rejection after reuse. The rebuilt tester passes the
standalone RV32 process harness in macOS Debug and Release; `component.elf_wait`
also passes both configurations (Debug ASan/UBSan). This is host scheduler and SDK
evidence, not validation of native Tab5 wait interleavings. No production runtime
or ABI changes were needed. No Linux tests, host simulator launch/control or
Tab5 flashing were performed.

## Follow-up: Fullscreen Failure Recovery

- [x] GUI-508: Hold a desktop session member outside its safe point, observe the timeout dialog, verify session resume and harmless late acknowledgement, then draw through Canvas again.
- [x] GUI-508: Remove the fullscreen executable after successful inspection while pause is held; acknowledge pause, verify load-error recovery, retained surfaces and resumed Canvas input.
- [x] GUI-508: Execute a valid ELF containing an illegal RV32 instruction; assert loader fault status 5, child cleanup, runnable desktop, retained surfaces and resumed Canvas input.
- [x] GUI-508: Keep Editor dirty across all three failures, save and verify the exact `retained!` bytes, then make it dirty again and complete the existing normal handoff and close-cancellation workflow.
- [x] GUI-508: Add deterministic fullscreen executable-memory exhaustion and saturated lifecycle-IPC fault injection.

The component fixture creates an inert descriptor child before starting desktop
and temporarily assigns it to that desktop session to control acknowledgement
timing. Desktop, Canvas and Editor remain actual SDK-built RV32 programs. Missing
file injection occurs only after the pause token proves inspection succeeded.
Fault recovery preserves existing behavior: loader reports status 5 and desktop
returns without a modal; negative launch failures and pause timeout show dialogs.
These checks do not change Kilo launch behavior or claim native fault containment.

Validation: the complete standalone GUI RV32 workflow passes macOS Debug with
ASan/UBSan and macOS Release using the existing SDK application artifacts. Git
whitespace checks pass. No production application/runtime code changed; no Linux
builds/tests, host simulator launch/control or Tab5 flashing were performed.

### Executable Memory and IPC Pressure Recovery

The GUI component fixture compiles the production loader and IPC service with
test-local interception. After successful launch inspection, it fills both data
and control queues in both directions for Canvas and Editor, then fails exactly
one executable-image allocation before releasing the pause barrier. It asserts
the allocation failure was consumed, retained surfaces and executable/endpoint
counts return to their prior baseline, desktop resumes and Canvas accepts input.
Exact dirty Editor bytes survive this fourth recovery case.

A separate injection fills the destination queue immediately before the first
desktop CLOSE control send. The test records EAGAIN and a later successful retry,
then verifies Editor's dirty-close cancellation. Final runtime teardown asserts
zero live executable images and IPC endpoints. Full standalone GUI RV32 workflows
pass macOS Debug (ASan/UBSan) and Release. No production hooks, ABI changes or
application rebuilds were needed; no Linux builds/tests, host simulator launch or
control, or Tab5 flashing were performed.

Together with the previously recorded game, pause, failed-load, fault, force-close
and coordinator-failure checks, this completes GUI-508 automated coverage. Native
contention, physical memory/headroom and device acceptance remain open under
GUI-104/202 and Phase 6; injected allocation failure is not a hardware headroom
measurement.

### Surface Validation and Teardown Races

`unit.surface` now rejects zero/oversized/overflowing creation dimensions and
repeats 13 invalid rectangle/buffer cases 100 times. Failed reads leave output
guards and staging untouched; failed uploads abort staging, preserve committed
pixels/revision and release their allocation. Final accounting returns to zero.

An additional 102 synchronized rounds cover commit/read before owner teardown,
teardown before either operation, and all three competing for the service lock.
Successful reads contain one coherent frame; operations after teardown reject
stale handles. Reusing the freed slot does not revive the previous handle or
inherit its reader grant. Every round ends with zero surface bytes and allocations.

The expanded unit test passes macOS Debug with ASan/UBSan and Release. These tests
use the existing host fixture mutex and do not establish native process stop or
physical contention behavior. No production code changed. No Linux builds/tests,
host simulator launch/control or Tab5 flashing were performed. Real RV32 SDK
rejection cases remain the next GUI-206 increment.

### Real RV32 Surface Rejection Evidence

Maintained `tester --concurrent` now checks five invalid creation dimensions and
repeats ten invalid rectangles plus null buffers across three rounds. Public SDK
reads preserve output/staging on rejection; failed uploads discard staging and
preserve committed pixels/revision. Allocation accounting returns to baseline.
After each pair of real child processes exits, previously granted surfaces reject
metadata and pixel reads with EBADF while preserving caller output.

The rebuilt SDK tester passes the standalone process harness in macOS Debug
(ASan/UBSan) and Release; the public application API boundary check passes. With
the recorded service-level saturation, invalid-input, allocation-failure, commit
race and cleanup evidence, GUI-206 automated coverage is complete. Native wait
interleavings and hardware acceptance remain separate open tasks. No Linux
builds/tests, host simulator launch/control or Tab5 flashing were performed.

## Delivery Validation Refresh — 2026-09-13

- macOS Debug and Release builds pass via `./tools/tabos macos <configuration> build`.
- Each full permitted CTest suite passes 90/90 with `-E 'integration.host_smoke|linux'`; Debug uses ASan/UBSan. Application build tracking covers incremental header, Makefile, resource-setting and GUI-marker changes.
- Tab5 Debug and Release builds pass via `./tools/tabos tab5 <configuration> build`. Firmware sizes are `0x176160` and `0x15d080`; app-partition headroom is 3,744 and 106,368 bytes respectively.
- `./apps/build.sh build --with-doom` and `./apps/build.sh install --with-doom` pass. Outputs install only into the local rootfs; no device copy was performed.
- Standalone real RV32 process/tester and complete GUI recovery harnesses pass Debug and Release using the rebuilt application artifacts.

GUI-107's maintained tester work, GUI-303's implemented layout and GUI-601's
permitted build/test delivery checks are complete. Their physical counterparts
remain explicitly separate. Open release work is measured buffering/limits
(GUI-005/006/204), native task/wait contention (GUI-104/202), and physical display,
touch, keyboard, latency, memory and recovery acceptance (GUI-602–605). Linux
validation remains excluded. No host simulator executable or UI automation was
run, and no Tab5 flashing was attempted. Kilo launcher work remains deferred.
