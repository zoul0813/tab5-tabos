# “Will It DOOM?” Milestone

## Summary

Port pinned `doomgeneric` as an optional filesystem-loaded RV32 application.
Completion means silent playable DOOM on host and physical Tab5: menus, gameplay,
keyboard controls, WAD loading, configuration, saves, clean exit, and repeat launch.

Preserve Application ABI v1. Add reusable raw-input transport and ELF resource
metadata instead of DOOM-specific kernel behavior.

## TabOS Enabling Work

- Expose existing `tabos_input_poll()` to loaded applications by appending a
  fixed-layout input call to the private ELF transport and adding an SDK runtime stub.
- Transport key-down, key-up, text, key code, modifier bits, and repeat state. Only
  the foreground process may consume events.
- Keep the private transport prefix and version compatible so existing ABI-v1
  binaries continue running.
- Validate guest pointers in host interpreter and native Tab5 paths.
- Add versioned `.note.tabos` metadata with descriptor size, Application ABI version,
  requested heap bytes, stack bytes, capability field, and reserved fields.
- Keep legacy defaults at 256 KiB heap and 16 KiB stack when metadata is absent.
- Accept heap requests from 256 KiB through 16 MiB, aligned to 4 KiB. Accept stack
  requests from 16 KiB through 256 KiB, aligned to 16 bytes.
- Reject malformed, duplicate, unsupported, or out-of-range metadata before process
  creation.
- Extend SDK Make rules with `TABOS_APP_HEAP_BYTES` and `TABOS_APP_STACK_BYTES`; emit
  metadata automatically.
- Pass validated limits through process and platform executable creation.
- Replace the host interpreter's fixed 2 MiB guest RAM with bounded per-process
  allocation derived from image, heap, stack, API table, and argument space; cap it
  at 24 MiB.
- Keep Tab5 heaps lazy and process-owned. Allocate the requested task stack from PSRAM
  and reclaim all resources on exit.
- Have DOOM request an 8 MiB heap and 64 KiB stack.

## DOOM Application

- Add optional `apps/doom` application named `doom`.
- Fetch `ozkl/doomgeneric` commit
  `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284` into ignored `apps/doom/doomgeneric/`
  storage.
- Fetch by exact commit, verify the checked-out hash, and never follow moving branch
  state.
- Support `DOOMGENERIC_SOURCE_DIR` for pre-fetched or offline source.
- Keep an explicit upstream source allowlist; do not compile platform-specific SDL,
  X11, Windows, or Emscripten files.
- Store required compatibility patches and the TabOS adapter in the repository. Apply
  patches only to the fetched build copy.
- Compile RV32I without compressed instructions. Link newlib and `libm` where required.
- Keep upstream warnings isolated; project-owned adapter remains under the normal
  warning policy.
- Record GPLv2 provenance, pinned commit, patches, and build instructions. Do not
  include the DOOM binary in standard TabOS release artifacts.

### Runtime Adapter

- Implement `DG_Init`, `DG_DrawFrame`, `DG_SleepMs`, `DG_GetTicksMs`, `DG_GetKey`,
  and a no-op `DG_SetWindowTitle`.
- Build at 320x200 with a 32-bit `DG_ScreenBuffer`.
- Convert each frame from upstream RGB pixels to a reusable 320x200 RGB565 buffer.
- Clear the frame to black, scale with nearest-neighbor `tabos_graphics_blit_ex()`
  into 960x720 at `(160, 0)`, then present.
- Preserve authentic 4:3 display shape with 160-pixel side bars.
- Use TabOS VSYNC pacing; DOOM retains its native 35 Hz game timing.
- Exit nonzero on graphics, conversion, or presentation failure.
- Patch the normal upstream quit path to call `exit(0)`; errors call
  `exit(nonzero)`. Process cleanup restores the terminal and shell.

### Controls

- W/S: forward and backward.
- A/D: strafe left and right.
- Left/Right arrows: turn.
- J or Control: fire.
- E or Space: use or open.
- Shift: momentary run.
- R: toggle always-run.
- Number keys: weapon selection.
- Escape, Enter, arrows, Y/N, and F-keys: menus and prompts.
- Ignore repeat key-down events.
- Track modifier transitions and synthesize Control and Shift state so
  modifier-plus-movement works with the Tab5 keyboard's single ordinary HID usage.
- Use a bounded 32-event adapter queue. On overflow, release all synthesized keys and
  reset state to prevent stuck controls.

### WAD, Config, and Saves

- Do not commit or download WAD files.
- Create and use `T:/games/doom` as the application working directory; the parent
  shell cwd remains unchanged.
- Without `-iwad`, search in order: `doom1.wad`, `doom.wad`, `doom2.wad`,
  `freedoom1.wad`, `freedoom2.wad`.
- Accept explicit drive-qualified `-iwad` paths.
- Store configuration, screenshots, demos, and savegames under `T:/games/doom`.
- Print concise installation instructions and exit nonzero when no usable IWAD exists.
- Document that users must supply shareware, Freedoom, or legally owned commercial data.

### Build Workflow

- Keep DOOM outside the default `apps/build.sh` path.
- Add `apps/build.sh --with-doom` for explicit fetch, build, and install.
- Allow `make -C apps/doom fetch`, `build`, `install`, and `clean`.
- Copy the resulting extensionless executable to `T:/bin/doom`.
- Have existing `--msc` installation copy an already-built DOOM binary when
  `--with-doom` is selected.
- Have CI perform explicit DOOM fetch and compile validation without publishing its
  binary.

## Validation

- Test ELF metadata defaults, valid limits, alignment, malformed notes, duplicates,
  unsupported versions, caps, and cleanup.
- Verify unchanged ABI-v1 fixtures still launch after the input-table extension.
- Test loaded-application raw key down/up, modifiers, repeats, foreground ownership,
  guest-pointer validation, and queue consumption.
- Unit-test key mapping, modifier synthesis, overflow reset, RGB-to-RGB565 conversion,
  destination geometry, default argument construction, and WAD search order.
- Build the pinned DOOM RV32 artifact on macOS and Linux CI, then cross-build Tab5
  Debug and Release.
- Without a WAD, verify a clear error, nonzero status, terminal restoration, and shell
  prompt recovery.
- With a user-supplied WAD on host, verify title screen, menu navigation, gameplay,
  save/load, quit, and repeat launch.
- Run the same RV32 artifact through the host interpreter and native Tab5 execution.
- On physical Tab5, require stable 35 Hz gameplay, correct 4:3 output, responsive
  controls, no watchdog resets, clean shell restoration, and successful second launch.
- Record executable size, peak heap, guest RAM, PSRAM usage, frame conversion time,
  present time, and sustained FPS.
- Verify keyboard polling, timers, display service, and filesystem remain responsive
  during gameplay.

## Fixed Decisions

- First milestone is silent; sound effects and music wait for the TabOS audio service.
- No touch, mouse, networking, multiplayer, mods UI, launcher GUI, or package-manager
  integration.
- `doomgeneric` is pinned-fetch, not vendored.
- WAD data is always user-supplied.
- DOOM remains an optional developer application and is excluded from standard releases.
- Generic ELF metadata and raw input APIs are permanent platform improvements; no
  executable-name special cases are allowed.
- Application ABI remains v1.

## Execution Task List

Track implementation and validation here. Mark a task `[x]` only after its required
tests or hardware check pass.

### Phase 0: Baseline

- [x] Confirm clean `feature/will-it-doom` worktree and record starting commit.
- [x] Run host configure, build, and test workflow.
- [x] Confirm existing ELF fixtures and core utilities still build.
- [x] Record baseline host test result and application image sizes.

Phase 0 baseline: commit `927d19302e0c0848a9fd4b29e8aabaf9a1ebad00`; macOS Debug
host test passed 30/30; `hello` image is 148 KiB; coreutils images range from
148 KiB (`cp`, `mkdir`, `mv`, `rm`) to 168 KiB (`wc`) after `--strip-unneeded`.

### Phase 1: ELF Application Metadata

- [x] Define `.note.tabos` name, type, version, descriptor layout, and endianness.
- [x] Add metadata constants and shared validation helpers.
- [x] Parse note sections before process creation.
- [x] Accept legacy ELF files with 256 KiB heap and 16 KiB stack defaults.
- [x] Reject missing required fields, wrong note type, unsupported version, and bad size.
- [x] Reject duplicate notes and nonzero reserved fields.
- [x] Validate heap range and 4 KiB alignment.
- [x] Validate stack range and 16-byte alignment.
- [x] Validate capability bits and reject unsupported capabilities.
- [x] Add tests for valid metadata, defaults, malformed notes, duplicates, alignment,
  limits, unsupported versions, and reserved fields.
- [ ] Pass validated heap, stack, and capabilities through loader/process creation.

### Phase 2: Process Memory Limits

- [x] Replace host interpreter fixed 2 MiB guest RAM with calculated per-process allocation.
- [x] Include image, heap, stack, API table, arguments, and guard space in calculation.
- [x] Enforce 24 MiB host guest-RAM cap.
- [x] Keep Tab5 heap lazy and process-owned.
- [x] Allocate requested Tab5 task stack from PSRAM.
- [x] Reclaim guest memory, stack, heap, and metadata on every exit/fault path.
- [x] Test allocation failure and cleanup after partial process creation.

Phase 2 validation: macOS Debug host tests passed 30/30. The RV32 component test
executes through a 3 MiB requested heap (above the former fixed 2 MiB guest-RAM
limit), rejects an allocation that would exceed the 24 MiB cap, then continues
creating and destroying execution contexts. On physical Tab5, the legacy `hello`
application from `main` launched and exited successfully twice after flashing the
Phase 2 firmware, confirming default resource cleanup and repeat launch.

### Phase 3: Raw Input Transport

- [x] Append fixed-layout input call to private ELF API transport without breaking prefix.
- [x] Add SDK `tabos_input_poll()` runtime stub.
- [x] Transport key-down, key-up, key code, text, modifiers, and repeat state.
- [x] Restrict event consumption to foreground process.
- [ ] Validate guest pointers in host interpreter and native Tab5 paths.
- [x] Define empty-queue and malformed-request behavior.
- [x] Add tests for event order, key-up, text, modifiers, repeats, ownership, and pointers.
- [x] Verify existing ABI-v1 fixtures launch unchanged.

Phase 3 validation: the host RV32 component test calls the fixed input gate with
guest-owned event storage and verifies a key-up event reaches guest code; an invalid
guest event pointer faults before the callback runs. Input queue, console ownership,
and SDK tests cover ordering, text, modifiers, repeat state, empty queue, and malformed
null output pointers. Existing `hello` remains an ABI-v1 fixture. Native Tab5 can reject
a null output pointer, but arbitrary bad native pointers cannot be safely contained until
application memory isolation and a recoverable native fault boundary exist. On physical
Tab5, `graphics-demo` opened and received keyboard input after flashing the Phase 3
firmware.

### Phase 4: SDK and Application Build Metadata

- [x] Add `TABOS_APP_HEAP_BYTES` and `TABOS_APP_STACK_BYTES` to SDK Make rules.
- [x] Emit `.note.tabos` automatically from application builds.
- [x] Keep defaults identical for applications that set no metadata variables.
- [x] Add metadata inspection/debugging command or documented inspection procedure.
- [x] Build and launch an existing application with explicit non-default limits.

Phase 4 build validation: `make -C apps/hello_elf metadata` shows the SDK-default
256 KiB heap and 16 KiB stack. `make -C apps/starfall metadata` shows Starfall's
explicit 1 MiB heap and 16 KiB stack after stripping. The rebuilt Starfall binary is
installed to the host rootfs. On physical Tab5, the rebuilt Starfall application loaded
and ran successfully with its explicit metadata.

### Phase 5: Pinned DOOM Source

- [x] Add `apps/doom/` build structure and Make targets: `fetch`, `build`, `install`, `clean`.
- [x] Fetch only `ozkl/doomgeneric` commit `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`.
- [x] Support `DOOMGENERIC_SOURCE_DIR` for offline/pre-fetched source.
- [x] Verify fetched commit hash before compiling.
- [x] Add explicit source allowlist; exclude SDL, X11, Windows, and Emscripten files.
- [x] Store compatibility patches and TabOS adapter in repository.
- [x] Record GPLv2 provenance and patch list.
- [x] Compile RV32I without compressed instructions.
- [x] Link required newlib and `libm` support.
- [x] Keep upstream warnings isolated from project-owned warning policy.

Phase 5 validation: fetched pinned `doomgeneric` commit
`dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`, verified its detached `HEAD`, and produced
the stripped RV32I artifact. A second clean build used `DOOMGENERIC_SOURCE_DIR` pointing
at the pre-fetched checkout and verified the same commit before copying and compiling.
The artifact is 714,368 bytes (`text` 414,148, `data` 60,084, `bss` 240,136) and its
`.note.tabos` requests 8 MiB heap and 64 KiB stack. Runtime graphics, timing, input, and
quit behavior remain Phase 6 work.

### Phase 6: DOOM Runtime Adapter

- [x] Implement `DG_Init`, `DG_DrawFrame`, `DG_SleepMs`, `DG_GetTicksMs`, `DG_GetKey`,
  and `DG_SetWindowTitle`.
- [x] Configure 320x200 32-bit DOOM framebuffer.
- [x] Add reusable 320x200 RGB565 conversion buffer.
- [x] Convert RGB frames and blit scaled 960x720 at `(160, 0)`.
- [x] Verify 4:3 geometry and 160-pixel side bars.
- [x] Use TabOS VSYNC pacing with native 35 Hz timing.
- [x] Return nonzero on graphics, conversion, or presentation failure.
- [x] Patch normal quit to `exit(0)` and error paths to `exit(nonzero)`.
- [ ] Verify terminal, graphics ownership, and process cleanup after exit.
- [x] Unit-test RGB conversion, geometry, and frame failure paths.

Phase 6 build validation: macOS Debug host tests passed 31/31, including sanitized
RGB conversion, 4:3 geometry, and clear/blit/present failure coverage. The pinned RV32I
artifact builds with the explicit quit-status patch and is 718,888 bytes (`text` 418,604,
`data` 60,084, `bss` 240,200). Tab5 relocation inspection found that libc `atexit()`
introduced an unsupported weak undefined `__on_exit_args` target; cleanup now uses
DOOM's internal `I_AtExit()` list, and the rebuilt artifact has no undefined relocation
targets. Exit cleanup remains to validate.

Physical Tab5 validation: rebuilt DOOM loads successfully after removing the weak
undefined relocation, displays the Freedoom Phase 2 splash and teaser/intro, and runs
smoothly. Host interpreter rendering remains much slower. Normal quit, terminal
restoration, and repeat launch remain to validate after Phase 7 controls.

### Phase 7: Controls

- [x] Implement required movement, turning, fire, use, run, weapon, and menu mappings.
- [x] Ignore repeat key-down events.
- [x] Track modifier transitions across key-down and key-up events.
- [x] Synthesize Control and Shift state for Tab5 keyboard behavior.
- [x] Implement bounded 32-event adapter queue.
- [x] Reset synthesized state and release keys on queue overflow.
- [x] Unit-test mappings, modifiers, repeats, overflow, and stuck-key recovery.

Phase 7 host validation: macOS Debug tests pass 33/33. Adapter tests cover movement,
strafing, turning, fire/use aliases, weapon/menu keys, repeat suppression, modifier
transitions, always-run toggle, 32-event overflow, synthesized release, and clean input
after recovery. Rebuilt RV32I artifact is 721,040 bytes (`text` 419,900, `data` 60,084,
`bss` 241,056), retains the 8 MiB heap and 64 KiB stack metadata, and has no undefined
relocation targets. Physical control validation remains Phase 10 work.

### Phase 8: WAD, Configuration, and Saves

- [ ] Set DOOM working directory to `T:/games/doom` without changing parent shell cwd.
- [ ] Create directory when missing, with clean failure handling.
- [ ] Implement default IWAD search order.
- [ ] Support explicit drive-qualified `-iwad` paths.
- [ ] Store config, screenshots, demos, and saves under `T:/games/doom`.
- [ ] Print concise no-IWAD installation guidance and return nonzero.
- [ ] Never commit or download WAD data.
- [ ] Unit-test working-directory behavior and WAD search order.

### Phase 9: Build and Installation Workflow

- [ ] Keep DOOM excluded from default application build.
- [ ] Add `apps/build.sh --with-doom` fetch/build/install path.
- [ ] Copy extensionless executable to `T:/bin/doom`.
- [ ] Make `--msc --with-doom` copy existing DOOM binary when available.
- [ ] Add CI fetch and compile validation without publishing WAD or DOOM binary.
- [ ] Record final executable size and metadata values.

### Phase 10: Validation and Hardware

- [ ] Run no-WAD host test: clear error, nonzero status, terminal restoration, prompt recovery.
- [ ] Run host title/menu/gameplay test with user-supplied WAD.
- [ ] Test save/load, quit, and repeat launch on host.
- [ ] Run same artifact through host interpreter and native Tab5 execution.
- [ ] Build pinned artifact on macOS and Linux CI.
- [ ] Cross-build Tab5 Debug and Release.
- [ ] Validate stable 35 Hz gameplay and correct 4:3 output on physical Tab5.
- [ ] Validate all controls, menu prompts, save/load, quit, and second launch on Tab5.
- [ ] Check watchdog, keyboard, timers, display, and filesystem responsiveness.
- [ ] Record peak heap, guest RAM, PSRAM, conversion time, present time, and FPS.
- [ ] Update milestone status and user-facing application/build documentation.
