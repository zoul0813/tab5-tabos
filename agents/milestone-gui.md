# TabOS Retro Desktop

## Summary

Status: agreed milestone direction, updated 2026-09-12; implementation remains pending. Work checklist: [GUI implementation tasks](apps/gui.md).

Build optional desktop launched from shell, with independently loaded GUI programs and a touch-first interface for the 5-inch screen. Combine 80s/90s OS character, bitmap artwork, and beveled controls with modern iOS/Android-inspired layouts, large icons, and generous spacing. Touch handles navigation; physical keyboard handles text.

Apps open maximized above a persistent bottom launcher/switcher dock. Restored windows support opaque overlap, movement, and resizing. Keep the full first-release scope: desktop launcher/file browser, calculator, small text editor, custom-canvas example, and fullscreen launch/return with GUI state retained.

**Core work needed first: concurrent app execution, IPC, window surfaces, input routing, and safe fullscreen handoff.** Existing graphics, audio, filesystem, pointer streams, and RV32 loader provide strong base. USB HID need not block GUI.

Current implementation has two relevant limits:

- Only foreground user process runs; parents remain loaded but blocked. Existing `tabos_spawn()` still wraps nested execution.
- Graphics targets fullscreen display; closing graphics restores terminal. No general window compositor or desktop restoration contract yet.

Concurrent execution below is the planned successor to the initial foreground-only architectural decision, not a description of current behavior. Preserve synchronous nested execution and the PID 0 invariant while introducing it. Record the successor decision in architecture/context/testing documents with that implementation.

## Implementation sequence

### 1. Enable concurrent applications

- Separate process execution state from keyboard focus, console ownership, and display ownership.
- Keep shell as persistent PID 0. Launch `desktop` as shell child; desktop launches concurrent GUI children.
- Implement real `tabos_spawn()` returning actual PID, child-exit notification, and wait/reap behavior. Preserve synchronous nested `tabos_exec()` semantics for existing shell/apps; migrate shell launch/wait handling when spawn becomes asynchronous.
- Define GUI-session membership and inheritance for GUI-client descendants, child reaping, and owner-exit cleanup before implementing concurrency. Fullscreen handoff children are outside the parked GUI-client set.
- Host schedules runnable RV32 contexts in bounded round-robin slices. Tab5 retains managed FreeRTOS task per process.
- Audit API caller identity, shared service state, locking, teardown, and waits for multiple callers. Background execution must not grant raw input or display access.
- Add orderly GUI-session shutdown: request client closure, honor unsaved-work cancellation, then return desktop to shell. PID 0 panic invariant stays unchanged.
- Provide explicit force-close for hung clients, with a clear unsaved-data warning. Recoverable desktop failure terminates remaining GUI-session processes, reclaims their resources, restores terminal ownership, and resumes shell. Never leave orphaned clients owning input or display.

### 2. Add small IPC and surface services

- Add bounded message endpoints with copied messages, process ownership, stale-handle rejection, disconnect notification, and generic-wait integration. Ordinary event saturation must not indefinitely block lifecycle/control delivery.
- Add OS-owned RGB565 surfaces: create, upload changed rectangles, commit, grant compositor read access, release. No cross-process raw pointers.
- Retain committed pixels independently of client execution. Specify bounded staging and atomic committed-frame visibility so upload/composition never exposes partial frames. Define failed/aborted commit behavior and release compositor read grants on teardown.
- Enforce process and aggregate surface limits. Allocation failure leaves existing desktop usable; release surfaces/endpoints during process teardown.
- Keep existing fullscreen graphics API available for games.

One native-size RGB565 buffer costs about **1.76 MiB**. Avoid an unconditional full-screen buffer pair per window; allocate actual client dimensions and track damage. Maximized clients still approach native size: account for SDK canvas, upload staging, retained surfaces, compositor buffers, and platform scanout together, not just surface-service allocations.

Run an early Tab5 memory/performance gate before fixing numerical quotas or buffering strategy. Measure maximized-client heap requirements, PSRAM peak, composition cost, and game headroom. Surface/process limits must be bounded and documented from those results; they do not reserve a guaranteed game budget.

### 3. Build desktop compositor and window manager

Desktop remains ordinary TabOS application using public services. Kernel supplies resource ownership; desktop supplies window policy.

- Desktop owns composed display and GUI input during desktop session.
- Implement opaque overlapping windows, title bars, move/resize, minimize, maximize, close, stacking, and focus.
- Open apps maximized within the work area above the dock. Dock provides large launcher and running-app controls, including restoration of minimized apps; restore enables floating windows.
- Show outline only while resizing; allocate and redraw client content on drag release. Failed resize keeps the old window geometry and committed content usable.
- Composite damaged regions; sleep when idle. Reuse existing blits and platform acceleration.
- Route keyboard/text to focused client; transform pointer coordinates into client space. Capture drag contacts until release/cancel.
- Extend pointer contract and SDL backend for mouse hover and wheel; existing host mouse model mainly covers pressed contacts.
- Cancel held keys/contacts on focus loss, removal, overflow, and fullscreen transition.
- Provide keyboard focus traversal and window switching. Touch actions must not depend on hover, right-click, or double-click.
- Use native 1280×720 layout, bitmap icons/font, beveled controls, large icons, and roughly 44-pixel minimum primary touch targets. Validate physical finger usability on the 5-inch display; desktop-era density is not the goal. No required precision corner grips for primary touch actions.

### 4. Provide developer toolkit and first apps

- Add portable C GUI SDK with window lifecycle, event loop, layout, invalidation, and custom RGB565 canvas.
- Initial widgets: labels, buttons, checkboxes, menus, text fields, scrollbars, lists, and dialogs.
- Toolkit renders client content; desktop draws window decorations. Apps remain separate RV32 ELF programs built against SDK.
- Reuse CP437 text convention. No on-screen keyboard in first release.
- Ship desktop launcher/file browser, calculator, and small text editor. Include standalone custom-canvas example proving graphics beyond stock widgets.
- Discover extensionless executables under `T:/bin`; generic icon and filename suffice initially. File browser supports navigation and explicit launch; editor supplies Open/Save.
- Add a minimal SDK-generated ELF GUI marker readable before execution. Marked programs launch as GUI clients; unmarked existing executables use console/fullscreen handoff. Use the normal loader validation path; the marker selects launch policy, not privileges. Full app manifests remain deferred.
- Existing console programs launch through fullscreen terminal handoff. Independent terminal windows deferred.

### 5. Implement fullscreen launch and return

State sequence: **desktop → pause clients → fullscreen child → restore desktop → resume clients**.

- Add cooperative session pause/resume events and acknowledgements.
- Clients finish current work, release outstanding service leases, quiesce audio/capture, retain document/window state, then park at toolkit safe point.
- Pause whole GUI client session before launching game. Core services continue running.
- Use two-second acknowledgement deadline. If client cannot pause, abort launch, resume acknowledged clients, and identify blocking app.
- Kernel launches the fullscreen child using normal process/resource rules, independently of GUI surface quotas. Pausing retains GUI allocations and never reclaims their RAM. There is no GUI-imposed allocation quota for fullscreen apps, but resident GUI competes for the same physical memory.
- Check available launch headroom with GUI memory retained and handle actual allocation/load failures. Insufficient memory returns a usable desktop with an explanation and lets the user close apps before retrying. No guaranteed game-memory reservation or automatic GUI unload/checkpoint is part of this release.
- Transfer display/input exclusively to child. Keep desktop coordinator available for lifecycle handling while GUI clients remain parked.
- On normal exit, launch failure, or recoverable child fault: reclaim child resources, restore compositor ownership, repaint retained windows, clear stale input, resume clients.
- Fullscreen close restores previous display owner—desktop or terminal—as appropriate.
- First release returns when game exits. Switching away from live game deferred.

Tab5 native faults can currently panic/reboot device. Reliable return after arbitrary game crash requires recoverable execution isolation; this remains separate OS work.

## Validation and delivery gates

- **Processes:** two independent RV32 clients make progress; blocked waits do not starve others; existing nested shell execution and PID 0 tests still pass.
- **Launch:** GUI marker selects concurrent launch; unmarked console/fullscreen apps remain launchable through normal kernel process services.
- **IPC/surfaces:** queue saturation including lifecycle traffic, foreign/stale handles, client exit during commit, atomic visibility, disconnects, memory exhaustion, and repeated cleanup.
- **GUI:** deterministic framebuffer tests for maximized work area, dock switching, restored overlap, clipping, outline resize, failed resize, damage, and focus. Synthetic touch/mouse/keyboard tests cover drag cancellation and text routing.
- **Fullscreen:** retain unsaved editor text and window positions across repeated Starfall/DOOM launches; test pause timeout, failed load, insufficient memory, and reported child faults.
- **Recovery:** close cancellation, explicit force-close, descendant cleanup, and recoverable desktop failure return ownership to the correct surviving owner without stale input or leaked resources.
- **Hardware:** validate touch orientation on all three display/controller revisions; measure input latency, composition cost, PSRAM peak, and game headroom.
- Run portable tests with sanitizers, real RV32 integration through maintained `tester`, and Debug/Release builds for macOS, Linux, and Tab5.
- Update architecture/context/testing decisions, roadmap, and user-facing SDK/GUI documentation alongside each implementation stage. Keep planned work distinct from implemented and hardware-validated behavior.

## Defaults and later work

- First usable release includes live independent GUI apps; early milestones remain internal foundations.
- Fullscreen games pause GUI workloads while retaining memory and state; launch is best-effort against remaining physical memory, without a GUI surface quota on the fullscreen process.
- Shell stays primary recovery/entry interface; desktop auto-start deferred.
- No third-party GUI framework dependency initially; small toolkit built on TabOS public APIs.
- USB HID keyboard/mouse backends later feed normalized input services. Controllers need separate portable button/axis API, then GUI navigation mapping.
- Terminal windows, clipboard, drag-and-drop, full app manifests, transparency, GUI session checkpoint/unload, switching away from a live fullscreen game, and arbitrary native crash recovery follow initial desktop. The minimal ELF GUI marker is included now.
