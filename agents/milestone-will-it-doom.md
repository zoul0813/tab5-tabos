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
  `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284` into ignored build storage.
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
