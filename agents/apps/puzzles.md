# Simon Tatham's Puzzles Implementation Plan

Status: proposed implementation plan, 2026-09-07. Implementation has not started.

This expands the candidate in [Application Port Candidates](../milestone-apps.md). Packaging, initial games, budgets, UI choices, and proposed shared API additions are recommendations, not new `[DECIDED]` architectural requirements.

## Outcome and Initial Scope

Provide a keyboard- and touch-accessible puzzle collection using the original portable puzzle engines and one shared TabOS frontend. Users launch `puzzles`, choose a game, play, save/resume, and return to the launcher or shell. The same RV32 artifacts run on Tab5 and through the macOS/Linux host interpreter.

Initial rollout:

| Order | Puzzle | Purpose in the port |
| --- | --- | --- |
| 1 | Net | Prove basic board drawing, cursor input, pointer actions, and completed-game feedback. |
| 2 | Solo, displayed as Sudoku (Solo) | Exercise numeric text, selection, secondary actions, and readable grid typography. |
| 3 | Mines | Exercise distinct reveal/mark actions, game status, and timing. |
| 4 | Bridges | Exercise dragging, line rendering, and more involved pointer gestures. |
| 5 | Pattern | Exercise grid interaction, labels, and primary/secondary drawing gestures. |

Verify each selected backend's actual keyboard and pointer conventions at the pinned revision. Start with a few measured, screen-fitting presets per game. Do not claim the full collection works when only these engines and controls have been validated.

First milestone includes new game, restart, undo/redo, explicit save/resume, preset selection, game ID display/entry, help, and solve where supported. Confirm before replacing a game with unsaved progress or revealing its solution. Defer printing, clipboard integration, arbitrary custom configurations, unrestricted board sizes, zoom/pan, online features, sound, achievements, and collection-wide packaging.

## Upstream and Project Boundaries

Use the authoritative source linked by [Simon Tatham's project](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/), pin an exact Git revision or source-archive digest, and retain the full [upstream licence](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/licence.html). Record source URL, revision, checksum where applicable, imported files, generated metadata, and local patches in `apps/puzzles/UPSTREAM.md`.

Vendor the selected upstream source and shared support code so ordinary builds work offline. Preserve upstream formatting in unchanged files; use project C style for the TabOS frontend. Keep backend rule changes out of the port unless a demonstrated upstream bug requires a separately explained patch.

The upstream [mid-end API](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/devel/midend.html) owns the interaction between game rules and the frontend. Use it for game lifecycle, configuration, undo/redo, timer advancement, and serialization rather than duplicating game state in the UI. The [drawing interface](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/devel/drawing.html) requires persistent pixels and frontend callbacks for drawing, clipping, text, and saved bitmap regions. Use headers at the pinned revision as the signature authority.

Read [architecture](../architecture.md), [context](../TABOS_CONTEXT.md), [testing](../testing.md), and [coding style](../coding-style.md) before implementation. Relevant current code and contracts:

- `sdk/include/tabos/graphics.h` and `sdk/lib/graphics.c`: RGB565 drawing, explicit present, and SDK-owned logical canvas. Direct pixels are available for explicitly requested canvases, not the zero-dimension native-mode opening.
- `sdk/include/tabos/input.h` and `sdk/include/tabos/tty.h`: physical keys plus cooked CP437 text, process-owned modes, normalized repeats, and one foreground keyboard queue.
- `sdk/include/tabos/pointer.h` and `docs/pointer.md`: logical display coordinates, button masks, stable contact IDs, cancellation, and pointer wait sources.
- `sdk/include/tabos/wait.h`: mixed service waits exist, but keyboard readiness is not yet exposed as a wait source.
- `loader/elf_loader.c`: current per-executable limits include 2 MiB file size and 1 MiB loaded image. Verify these again before choosing engine/build settings.
- `sdk/make/application.mk`: independent RV32I/`ilp32` C17 applications, resource metadata, and declared output/asset reporting.
- `sdk/include/tabos/process.h`: synchronous nested foreground execution, retaining the parent while a child runs.

## Phase 1: Source Integration and Packaging

Prefer a small `puzzles` launcher plus one independently loadable executable per selected engine:

```text
T:/bin/puzzles
T:/bin/puzzles-net
T:/bin/puzzles-solo
T:/bin/puzzles-mines
T:/bin/puzzles-bridges
T:/bin/puzzles-pattern
```

Compile the same frontend/support sources into each game, linked with just its selected backend and actual support dependencies. This avoids combining every engine into an image that may exceed current loader limits. Code duplication on storage is acceptable initially; do not introduce shared libraries or enlarge loader limits merely to package the collection.

The launcher uses a fixed manifest of the five installed programs, labels, and help summaries. It closes its graphics and pointer resources before `tabos_exec`, retains only small menu state, and reopens the launcher after the child exits. Failed/missing child launches report a recoverable error. Launcher exit returns to the existing shell; child failure must not terminate process 0.

Suggested structure:

```text
apps/puzzles/
  Makefile
  game.mk
  launcher.mk
  LICENSE
  README.md
  UPSTREAM.md
  vendor/puzzles/
  include/puzzles_tabos/
  src/launcher.c
  src/main.c
  src/frontend.c
  src/drawing.c
  src/font.c
  src/input.c
  src/menu.c
  src/storage.c
  assets/
docs/puzzles.md
```

Use a top-level application Makefile to orchestrate isolated launcher/game sub-builds, each including shared SDK rules with correct paths, distinct build directories, output names, and explicit sources. Implement aggregate `tabos-list-outputs` and `tabos-list-runtime-assets` so ordinary `apps/build.sh` and MSC installation discover every binary and asset. Do not change global discovery just to hardcode this collection.

Exclude desktop GTK/Windows/macOS/Java frontends and printing backends. Inspect upstream generated version/header requirements; generate only necessary artifacts deterministically. Use pinned upstream build definitions to determine engine support sources, not guessed file lists. Add trailing math-library support to shared Make rules only if needed, reusing any equivalent Lua work already implemented.

Build Net first and record stripped size, loaded image size, heap/stack metadata, and link dependencies. Keep a common frontend test target independent of any desktop GUI library.

## Phase 2: Persistent Canvas and Drawing Adapter

Open a **640×360 logical RGB565 canvas**, explicitly setting both dimensions before the single graphics-open call. It scales exactly 2× to the Tab5 display and consumes 460,800 bytes. Reserve UI bands for actions and status; use the remaining rectangle for the board. Ask the mid-end to fit its puzzle to that rectangle, then center the returned board size. Reject presets that cannot fit with readable labels and usable targets.

All rendering initially writes through one software adapter to the SDK-owned canvas pixels. This gives consistent clipping, readback, and saved-region behavior without mixing deferred OS drawing commands with immediate pixel access. Present through the SDK. Hardware acceleration can follow measurements; one ABI call per pixel is not an acceptable drawing path.

Required adapter behavior:

- Convert the engine's palette to RGB565 with bounds checks; handle any sentinel for absent fill/outline separately from palette indices.
- Implement filled rectangles, lines, polygons, circles, and the thick-line/fallback operations required by the pinned selected backends. Verify edge coverage and zero/degenerate shapes against the upstream contract.
- Intersect puzzle clipping with board and canvas bounds for every primitive, text operation, and saved-region copy. Check signed arithmetic before pointer conversion; malformed/extreme dimensions must not produce out-of-bounds writes or unbounded raster loops.
- Implement saved bitmap regions as bounded heap-owned RGB565 buffers. Honor the pinned API's saved-position and restore-position conventions, partial offscreen regions, and clipping. Free all blitters on game destruction.
- Preserve canvas contents between draw calls. Treat dirty rectangles as presentation hints, union them during a drawing batch, and present once after completed work. Do not clear the whole puzzle before incremental redraws.
- When menus overlay the board, preserve the covered pixels or compose the overlay from a retained board image. Closing a menu must restore the exact board without assuming the engine will repaint unchanged cells.
- Copy transient status strings into bounded frontend storage. Draw all application errors inside graphics mode; terminal output is not a visible dialog while graphics owns the display.

Use an application-owned bitmap font with explicit redistribution rights and sufficient ASCII/digit/punctuation coverage. The SDK has no public general graphics text renderer; do not call `graphics/font.c` internals. Support required alignment, baseline, font type, and size semantics. Measure text consistently with rendering. Audit selected engines' non-ASCII labels and use the drawing API's supported text fallback mechanism where available; never treat UTF-8 bytes as independent CP437 glyphs. Any new font asset must ship with its licence.

## Phase 3: Keyboard, Touch, Menus, and Timing

### Keyboard and UI policy

Keep cooked text translation enabled for digits and Aa/Sym punctuation. Consume the event API exclusively rather than mixing it with stdin. Translate navigation/control keys from key-down and printable input from text events, deduplicating Enter/Space/Backspace and other synthesized control text. Preserve engine modifiers and normalized repeats where meaningful; suppress repeated destructive menu actions.

Map the engine's documented cursor/select/secondary-select keys rather than inventing one universal control scheme for every game. Provide an always-visible Menu button and Escape to open/close it. Route events to exactly one owner: board, launcher, menu, or text-entry dialog. Implement collection commands through the mid-end's supported mechanisms and state queries; never mutate undo chains or solved flags directly.

Menus expose New, Restart, Undo, Redo, Save, Resume, Preset, Game ID, Help, Solve when supported, and Back. Keep disabled actions visibly disabled. Confirm discard/solve with a fresh action, not a held key. Show game-specific controls in Help. Preset menus use actual backend-provided descriptions and validated parameters.

### Pointer conversion and secondary actions

Discover and open the portable pointer device if available; keyboard play must still work when touch is absent. Convert display coordinates using the graphics output rectangle and scale, then subtract the board origin. Reject initial presses in letterboxes or UI regions before forwarding to the engine.

Capture one contact/button per gesture initially. Map down, move, and up into the corresponding puzzle mouse press/drag/release events. Track the original button because the up event's current button mask may be empty. Ignore extra contacts until the captured gesture ends; do not let them replace the active drag.

Provide a visible primary/secondary action selector for touch, with primary as default; latch its value at gesture start. This supports actions such as marking or reverse operations without requiring an ambiguous long press. Preserve host primary/secondary/middle buttons where the game uses them. Add a game-specific alternative only where the backend requires a gesture that these controls cannot express.

For `CANCEL`, focus loss, device removal, queue reset, or menu takeover, clear captured state and restore a consistent engine UI state. Audit whether the selected engine commits on release before synthesizing one: cancellation must not become an unintended move. If a backend lacks a cancellation entry point, define and test the smallest frontend/engine-UI adaptation. Never silently turn a canceled stroke into a click.

### Event loop and timer contract

Use mixed keyboard/pointer readiness with the mid-end's next active timer deadline. Reuse the keyboard wait-source work proposed in [Kilo](kilo.md) if it exists; otherwise implement that shared prerequisite separately, with ownership, teardown, and lost-wakeup tests. Pointer-only waiting would leave keyboard input unable to wake the application.

Use monotonic elapsed time for animation and puzzle timers. Implement upstream timer activation/deactivation callbacks; schedule only while requested, advance by actual elapsed time, and avoid replaying a backlog of frames after a stall. Define menu pause behavior consistently, including restoring the time baseline. Drain bounded event batches, update once, present only changed content, then wait. No unconditional 60 Hz redraw or keyboard polling loop.

## Phase 4: Generation, Parameters, and Memory

Use upstream random generation and seed parsing. Supply a frontend seed assembled from available wall-clock/monotonic values and a process-local counter; it is for game variety, not cryptography. Explicit seeds and descriptive game IDs must remain available for reproduction. Tests use fixed inputs; document that generated layouts may change with upstream revisions.

Only expose a curated set of measured presets initially. Validate game-ID parameters before generation, including requests arriving from saved games; a valid upstream parameter string is not proof it fits TabOS RAM or display. Reject unsupported sizes with a useful message rather than silently shrinking the requested puzzle.

Provisional game budget: **4 MiB heap and 64 KiB stack**, including the logical canvas, board state, undo history, blitters, fonts, menus, and serialization staging. Keep the launcher budget small and account for the retained launcher alongside the child. Measure per-engine allocations and stack use before expanding presets; do not assume these budgets guarantee every puzzle configuration fits.

Generation and solving can execute synchronously for a long time. Display a generation/solve status before entering the engine, measure native duration, and restrict available presets to acceptable latency. Do not promise responsive Cancel during a synchronous engine call. If a required preset blocks too long, defer it or add a narrow, tested cooperative progress/cancellation adaptation; do not expose FreeRTOS threads to application code.

Audit upstream allocation helpers and fatal-error handling. If allocation failure invokes a non-returning frontend fatal callback, report it, close application resources safely, and exit the child with failure while leaving saved data intact. Do not return NULL into code that assumes successful allocation, or longjmp across engine allocations without a proven unwind strategy. Treat loss of unsaved progress on fatal OOM as a documented limitation; bound ordinary workloads so it is exceptional.

Measure undo-history growth with long sessions. Do not silently trim engine history or clear undo to hide exhaustion. If a bound is needed, add a deliberate, documented policy through supported state operations and test it separately. Avoid arbitrarily increasing system loader or heap limits.

## Phase 5: Persistence and Resume

Store per-game state beneath `T:/data/puzzles/`, using stable backend IDs such as `net.save` and `solo.save`. Keep preferences/preset selection separate from game state. Support explicit Save and Resume first; show unsaved progress and offer Save/Discard/Cancel before leaving or replacing the current game. Do not write the card after every pointer move.

Use upstream serialization/deserialization, including undo history where the format provides it. Do not invent a second save format for game rules. Record frontend metadata separately only when necessary, and treat upstream save compatibility as something to test for each pinned update.

- Bound total input bytes and advertised parameters before loading. Validate game identity and report unsupported/corrupt/truncated saves without overwriting them.
- Load into a temporary mid-end and replace the current session only after successful validation. Suppress temporary-session drawing/timers until commit. Account for the memory needed to retain the old and candidate states.
- Stream serialization to a uniquely created sibling temporary file. Latch write failures in callback context where the upstream callback cannot return errors; do not install a partial save after serialization returns.
- Check write, flush, and close results. Verify rename-over-existing semantics on host and FAT; use a recoverable backup/rename transaction if needed. Never delete the only previous save to make replacement possible.
- Preserve recoverable files and report their paths if replacement/rollback fails. Reuse the verified save approach from Kilo if implemented, without importing its editor code or prematurely creating a general persistence framework.
- Missing storage does not prevent starting a new game. Save failure retains the active game and reports a visible error.

Do not claim power-loss-atomic FAT saves or `fsync` durability that TabOS does not provide. Automatic checkpointing is follow-up work after explicit save/resume is reliable.

## Phase 6: Validation

Use deterministic game IDs, synthetic input, temporary drives, and fake monotonic clocks. Native frontend/engine tests run under ASan/UBSan with the same feature selection as the RV32 build. Actual RV32 integration remains required in addition to native tests.

| Area | Required coverage |
| --- | --- |
| Drawing | Pixel checks for clipped shapes, palette sentinels, text alignment, font fallback, saved-region restoration, board/UI boundaries, overflow cases, incremental drawing, and menu restoration. |
| Input | Cooked text and key deduplication, repeats, modifiers, button masks on release, coordinate transforms, drag outside board, touch secondary selector, extra contacts, cancellation, and absent/removed pointer. |
| Mid-end lifecycle | New/restart, undo/redo, solve availability, completion/loss status, preset changes, game IDs, timers, and clean destruction. |
| Persistence | Move-save-reload equivalence, undo after resume, wrong-game files, truncation/corruption, oversized parameters, partial writes, full storage, close/rename/rollback failures, and preserved prior save. |
| Resource behavior | Repeated new games and launches, long histories, OOM/fatal paths, allocation bounds, deep solver/generator stack use, and cleanup while timers or gestures are active. |
| Packaging | All declared binaries/assets installed, no desktop dependencies, offline build, resource/header changes trigger rebuilds, and each ELF meets current loader limits. |
| Real RV32 host | Persistent shell to launcher to game and back; deterministic moves and file checks; repeated launch; fault cleanup; terminal restoration; keyboard/pointer wake behavior. |

For each of the five games, record at least one deterministic input trace with expected state/status and frontend image checks. Compare rule outcomes to the pinned upstream backend. Exact pixels need only match TabOS's defined renderer, not GTK anti-aliasing. Use targeted pixel assertions plus a few reviewed whole-board captures rather than regenerating broad snapshots blindly.

Validate macOS/Linux Debug and Release builds and Tab5 cross-builds through the project workflow and pinned toolchain. Follow existing optional application RV32 harness conventions where normal CTest does not build applications. Add maintained `apps/tester` coverage for any shared SDK additions; puzzle-specific tests remain application tests.

Physical acceptance is separate:

- Readable boards, labels, status, and menus at 2× scaling; touch targets usable with a finger.
- Keyboard-only completion of representative interactions and Aa/Sym numeric/punctuation entry.
- Touch primary/secondary actions, dragging, rapid retouch, cancellation, and correct orientation on available hardware revisions; record untested revisions explicitly.
- Save, exit, reopen, reboot, and resume with matching progress.
- Measured generation/solve latency, memory/stack usage, idle behavior, and service responsiveness without watchdog failures.
- Repeated launcher/game/shell transitions without leaked buffers, stale pointer gestures, stuck timers, or broken terminal state.

## Delivery Checklist

Deliver Net and the shared frontend first, then add Solo, Mines, Bridges, and Pattern one at a time with their acceptance traces. Keep newly enabled engines out of completion claims until their controls, presets, saves, and resource use pass.

- [ ] Pin/import upstream source and notices; build launcher and Net through shared SDK rules.
- [ ] Implement persistent canvas, fonts, clipping, primitives, blitters, and batching.
- [ ] Implement keyboard/touch routing, menus, timer callbacks, and event-driven waiting.
- [ ] Validate bounded presets, random seed/game-ID handling, and fatal-error cleanup.
- [ ] Implement explicit save/resume with recoverable failures.
- [ ] Add and validate Solo, Mines, Bridges, and Pattern independently.
- [ ] Pass native sanitizer tests, actual RV32 host integration, and supported-target builds.
- [ ] Complete separate physical Tab5 acceptance and record remaining revision gaps.
- [ ] Write `docs/puzzles.md`; update `docs/README.md`, `docs/applications.md`, build/install documentation, and any changed SDK contracts.
- [ ] Update `agents/roadmap.md` when implementation starts and as validation completes; record accepted shared API changes in architecture/context/testing documents.

Completion means the selected games are playable with documented controls, can save/resume progress, respect current memory/image limits, and reliably return to the launcher and shell. Merely displaying a generated board is not completion.
