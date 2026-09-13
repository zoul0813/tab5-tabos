# PICO-8 Cartridge Player Implementation Plan

Status: planned 2026-09-12. No implementation or performance claim yet.

## Outcome and Boundaries

Build an independent `T:/bin/pico8` application that runs existing PICO-8 games
from microSD. Use FAKE-08 and its matching z8lua runtime as the implementation
baseline. This is an unofficial compatible cartridge player, not a port of
Lexaloffle's proprietary editor/runtime. Display that distinction in help,
version output, provenance, and user documentation.

Keep `apps/lua` as the general-purpose Lua 5.5 interpreter. The player owns its
PICO-8 dialect, numeric behavior, virtual memory, graphics, and sound synthesis;
it must not load cartridges through the existing Lua interpreter or require
per-game C compilation. No tile-engine dependency.

Initial delivery includes `.p8` and `.p8.png` files, one-player keyboard controls,
30/60 Hz callbacks, sound/music, persistent cartridge data, and clean return to
the shell. Implement text carts first, then PNG carts before acceptance.
Defer the editor, Splore/downloads, web exports, ZIP collections, save states,
multiplayer controllers, touch controls, and broad desktop filesystem access.
Compatibility means the tested subset of the pinned runtime, not all PICO-8 games.

Preserve TabOS platform boundaries and the same RV32 artifact on host and Tab5.
Linux builds/tests remain excluded by existing user direction; keep source
portable and use macOS for host validation. Before implementation read context,
architecture, testing, roadmap, and coding-style documents. This plan's budgets
and implementation choices are proposals, not new architectural `[DECIDED]` rules.

## Baseline and First Feasibility Gate

[FAKE-08](https://github.com/jtothebell/fake-08) already targets unsupported
platforms and supports text and PNG cartridges. Its [z8lua submodule](https://github.com/jtothebell/fake-08/blob/master/.gitmodules)
points to the FAKE-08 maintainer's fork; use the exact matching revision rather
than substituting stock Lua or another z8lua head.

Vendor a reviewed immutable FAKE-08 revision and required dependencies under
`apps/pico8/vendor/`. Record upstream commit IDs, archive checksums, source URLs,
included/excluded files, patch inventory, and compatibility baseline in
`apps/pico8/UPSTREAM.md`. Normal builds must work offline. Keep vendor formatting;
put TabOS adaptation in maintained local sources.

Audit the [upstream notices](https://github.com/jtothebell/fake-08/blob/master/LICENSE.MD)
per included component. The inventory includes notices beyond MIT, including
the oval implementation's CC BY-SA attribution. Retain applicable notices and
record required distribution treatment; do not label the whole import MIT-only.
Exclude platform-specific code and bundled games not needed by the player.
Only distribute cartridges with explicit redistribution permission or original
test/demo cartridges. Official PICO-8 binaries/assets are not a dependency.

The main feasibility risk is C++ integration. The [desktop build](https://github.com/jtothebell/fake-08/blob/master/platform/SDL2Desktop/Makefile)
uses C++17 without RTTI/exceptions. TabOS's current application rules use a C17
GCC link, CRT does not run C++ initialization arrays, and the linker discards
unwind frames. A successful desktop build therefore does not prove a TabOS port.

First produce a tiny real RV32 proof using the required C++ subset, then run a
minimal z8lua cartridge and destroy/recreate its VM. Inspect undefined symbols,
relocations, generated instructions, constructors/destructors, allocations,
standard-library dependencies, and error unwinding. Failure here blocks the full
port; document the concrete blocker before expanding scope or choosing another core.

## Build and Runtime Integration

- Add opt-in mixed C/C++ support to shared SDK application rules: compile SDK C
  and assembly with their current language settings; compile selected C++ sources
  as C++17; use the target C++ link driver only for opted-in applications. Preserve
  RV32I/`ilp32`, metadata, asset installation, stripping, and relocation policy.
- Track CXX, C++ flags, source manifests, object/header dependencies and link
  inputs in effective-build invalidation. Keep existing C-only applications
  unchanged. Audit SDK declarations for C linkage when included from C++; use
  compatible header guards/wrappers rather than changing exported symbol names.
- Use `-fno-exceptions -fno-rtti`. Explicitly construct/destroy player state inside
  application lifetime. Remove imported dynamic globals where practical; if
  unavoidable initialization arrays remain, implement and test opt-in CRT/linker
  support before using them. Never silently ignore constructors or use no-op
  destructor/guard stubs to satisfy the linker.
- Audit z8lua's error path across C++ stack frames. Protected cartridge execution
  must not longjmp past live nontrivial C++ objects. Keep resource ownership outside
  Lua error boundaries and guarantee explicit cleanup after failures.
- Audit STL allocation failure and abort paths: disabling exceptions does not
  make `std::string`/`vector` growth recoverable. Bound allocations and replace
  fatal paths where necessary; do not claim OOM recovery until injected failures
  return to the shell on the actual target profile.
- Start with an 8 MiB heap and 128 KiB stack metadata request, a 4 MiB z8lua
  allocation ceiling, and 1 MiB input-cartridge cap. Count decode buffers, STL,
  VM state, framebuffer, PCM, and libc overhead separately. Check arithmetic and
  decoded lengths before allocation. Measure and revise budgets with evidence.
- Implement a TabOS backend for the core's [Host interface](https://github.com/jtothebell/fake-08/blob/master/source/host.h)
  using public SDK graphics, input, audio, files, and monotonic time. No SDL,
  ESP-IDF, host threads, dynamic libraries, or private transport in application code.
  Keep upstream desktop backends out of the shipped artifact.

## Player Behavior

### CLI and cartridge loading

```text
pico8 --help
pico8 --version
pico8 T:/games/demo.p8
pico8 --mute T:/games/demo.p8.png
pico8 -- T:/games/-example.p8
```

No arguments prints usage and returns success; no browser is required initially.
Support one cartridge path and the options above. Invalid usage returns 2;
load/runtime errors return 1; normal exit or user cancellation returns 0.
Restore the terminal before printing final errors. Version identifies both the
TabOS player and pinned FAKE-08 revision.

Resolve the initial path using normal TabOS drive/cwd semantics without changing
process cwd. Use bounded file reads and the upstream cartridge decoder, with
checks for malformed sections, compression, PNG dimensions, decompressed sizes,
and unsupported cartridge versions. Preserve code, graphics, map/flag data,
music and sound sections. Treat `#include` as an unsupported development-time
feature initially, with a clear error; users supply self-contained cartridges.

Support runtime cartridge loads/reloads within the initial cartridge directory:
resolve and normalize relative names there, reject escape via `..`, absolute or
drive-qualified cartridge-requested paths, and release the previous VM/resources
deliberately. User-supplied CLI paths remain unrestricted normal TabOS paths.
Disable unneeded desktop libraries, file-writing/export helpers, shell execution,
network downloads and dynamic loading. Cartridge-visible unsupported operations
must fail explicitly and be listed in documentation. This is not a security sandbox.

### VM, drawing and controls

Preserve the pinned core's PICO-8 syntax, fixed-point arithmetic, RAM mapping,
palette/remapping, sprite/map/font rendering, clipping, camera, and callback
semantics. Avoid rewriting those in TabOS adapters. Maintain a compatibility
matrix for language features, memory APIs and runtime extensions; record inherited
limitations separately from port bugs.

Convert the final indexed framebuffer through the active palette into a reusable
128×128 RGB565 SDK logical canvas. Honor core screen draw modes before presentation.
TabOS integer scaling gives a centered 640×640 image on 1280×720 with black bars.
No per-pixel SDK calls or tile-engine integration are needed for presentation.

One input consumer owns raw keyboard events and tracks presses/releases. Map
arrows and WASD to directions, J/Z to button O, and K/X to button X. Combine aliases
so releasing one key does not clear another held alias. Let the core implement
`btnp` timing from sampled button state rather than OS key-repeat events.
Escape opens a minimal resume/restart/quit menu, navigated with arrows and
Enter/O; Escape resumes. Ctrl-C/Ctrl-D always request exit, including stuck Lua
loops. Do not reserve Q or P as global shortcuts that consume cartridge input.
Initially expose only the six game buttons, not optional keyboard/mouse extensions.

Keep raw-mode changes process-local; restore inherited mode and clear held state
on every exit. Restart rebuilds cartridge VM state while preserving saved cartdata.
Pausing stops game callbacks and flushes queued sound; resuming resets deadlines.

### Scheduling and audio

Drive 30/60 Hz core callbacks with absolute monotonic deadlines and fractional
remainders, independent of display refresh. Permit at most four catch-up updates
per pass, then discard accumulated wall-time debt and record an overrun. Service
input/audio between bounded work units; sleep cooperatively until the next due
work. Preserve upstream explicit flip/yield behavior through the same scheduler.

Install an internal instruction-count service/cancellation hook for every z8lua
thread, with service checks at most 10 ms apart when VM instructions execute.
Do not expose hooks that disable cancellation. Check long parser/decode work for
bounded execution too. Document that long native calls remain cooperatively
cancellable; measure actual hardware interruption latency.

Use the existing core [synthesizer](https://github.com/jtothebell/fake-08/blob/master/source/Audio.cpp)
at its native 22050 Hz, signed 16-bit mono, through one SDK speaker stream.
Generate in 256-frame blocks, initially maintaining 1024 queued frames. Preserve
unwritten tails across partial writes/EAGAIN; never regenerate or overwrite them.
Do not fill the entire kernel queue merely because space exists. Sample production
advances sound state independently of display presentation; in mute/no-device mode
advance equivalent synthesis into a bounded discard buffer.

Missing or failed audio is nonfatal: show one diagnostic and continue muted.
Flush on pause/restart/exit, close deterministically, and track underruns during
validation. Preserve upstream SFX/music semantics; do not reuse Snake melodies
or substitute simple tones. No application audio thread or callback is needed.

### Saves and cleanup

Implement `cartdata`/`dget`/`dset` using the pinned core's data representation under
`T:/data/pico8/saves/`. Use a stable SHA-256 encoding of the complete cartdata key
as the filename, with the original key stored/verified in the record; never use
the key as a raw path. Create directories lazily. Persist dirty data on pause,
cart switch, normal exit, and at most once every five seconds during play.
Write a temporary sibling then rename; failure keeps the previous good save and
produces one diagnostic. Missing/corrupt saves start with defaults. Storage failure
must not prevent play, and installation must not overwrite saves.

All normal, error, restart and cancellation paths converge on one cleanup sequence:
finish a bounded save attempt, stop/close audio, destroy VM/core state, close canvas,
restore console mode, then report status. Teardown also relies on process-owned
SDK resources being reclaimed; test forced termination independently of normal exit.

## Validation and Delivery Checklist

- [ ] Pin/import FAKE-08, matching z8lua and required dependencies; complete
  provenance, patch and notice inventory; select licensed fixtures.
- [ ] Pass C++/CRT/allocator/error-unwinding feasibility gate through the real RV32
  loader; add isolated regression tests for any shared build/SDK changes.
- [ ] Implement CLI, bounded text loading and minimal cartridge execution.
- [ ] Implement RGB565 presentation, keyboard/menu, callbacks and cancellation.
- [ ] Add bounded PNG/compressed cartridge loading and cartridge-switch policy.
- [ ] Integrate SFX/music, backpressure, pause/mute and nonfatal audio failures.
- [ ] Implement cartridge persistence, restart and all cleanup paths.
- [ ] Run applicable upstream tests plus target-profile ASan/UBSan tests for numeric
  boundaries, RAM access, malformed carts, decoder limits, OOM, and repeat VM setup.
- [ ] Add deterministic framebuffer/palette/sprite/map/text fixtures, input alias
  and `btnp` checks, fake-clock 30/60 Hz tests, PCM golden/tolerance checks,
  partial-write/underrun tests, and save corruption/write/rename failures.
- [ ] Add a real RV32 harness launching `pico8` from the persistent shell: text/PNG
  equivalents, rendered pixels, input, sound, restart, save/reload, errors,
  Ctrl-C/Ctrl-D during loops/coroutines, repeated launch and forced teardown.
- [ ] Build through `./apps/build.sh`; verify metadata, dependency rebuilds,
  extensionless install, offline build, assets and license installation.
  Pass macOS Debug/Release tests and Tab5 Debug/Release builds using `tools/tabos`.
- [ ] Ship an original small playable cartridge plus diagnostics through flat
  runtime assets into `T:/data/pico8/`; retain game source and redistribution notices.
- [ ] On physical Tab5, validate controls, microSD saves, sound/pause/exit, memory
  high-water usage, service responsiveness and interruption latency. Run a documented
  representative set including 30 Hz, 60 Hz, sprite/map-heavy and audio-heavy carts.
  Record achieved frame time, underruns and failures; do not infer native performance
  from the host's RV32 interpreter or promise full-speed universal compatibility.
- [ ] Publish `docs/pico8.md`, compatibility matrix and dated validation evidence;
  link from docs/application indexes, update roadmap/testing context, and record
  any accepted shared architecture changes. Keep hardware acceptance open until measured.

Acceptance requires usable games with graphics, input, sound and persistence on
Tab5, plus reliable return to the existing shell. A compilation proof alone is
only the first gate. Expand compatibility after this baseline; do not silently
replace the player with a partial library inside `apps/lua`.
