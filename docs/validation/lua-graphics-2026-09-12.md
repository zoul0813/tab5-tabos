# Lua graphics validation — 2026-09-12

## Scope

Basic Lua graphics and keyboard game input, authorized independently of the tile
engine and before final CLI physical acceptance. Games run as ordinary Lua source;
`snake.lua` includes all rendering, input, rules, and score digits in one file.
No new public SDK API, private ELF ABI, or firmware service is introduced.

## Automated evidence

- `./apps/build.sh`: passed; all standard RV32 applications build/install. Installed
  and staged `snake.lua` compare byte-for-byte with source.
- `./tools/tabos macos debug build`: passed.
- `./tools/tabos macos release build`: passed.
- `ctest --test-dir build/macos-debug -R 'lua|sdk_graphics|naming|application_api' --output-on-failure`:
  8/8 passed, including native Lua and SDK drawing under ASan/UBSan.
- Equivalent macOS Release CTest selection: 8/8 passed.
- `build/macos-release/tests/tabos_lua_rv32 build/apps/shell/shell build/apps/lua/lua apps/lua/examples/snake.lua`:
  passed using the real RV32 executable, shell, loader, input and display services.
- `git diff --check`: passed.

Native checks compare independent expected RGB565 pixels for clipped rectangles,
lines, outlines, and packed blits. They exercise numeric/buffer limits, duplicate
opens, stale userdata, garbage collection, `<close>` error unwinding, allocation
failure during drawing, failed open/present/close and TTY transitions, held keys,
short press/release pairs, queue overflow, console-read exclusion, and cancellation.
The SDK now returns immediately for an empty logical fill intersection, before
forming a potentially invalid destination pointer for an offscreen rectangle.

RV32 checks verify actual rectangle/upscale and packed-blit pixels, raw short key
taps, the shipped Snake script, normal return, uncaught error with retained screen,
`os.exit(7)`, Ctrl-C/Ctrl-D, repeated launch, and forced teardown with graphics
still active. A subsequent runtime boot and graphics launch succeed.

Final Lua ELF size: text 377856, data 604, bss 396; total 378856 bytes. Metadata
remains 4 MiB heap and 64 KiB stack; Lua-managed allocations remain capped at 3 MiB.
The one SDK canvas is separately capped at 512 KiB. Packed-blit scratch allocations
count against Lua's limit. Existing RWX ELF and duplicate-static-library linker
warnings remain; no new compiler warnings were emitted.

## Pending physical acceptance

Linux validation remains excluded by user direction. The same RV32 application
was cross-built for Tab5; firmware was not changed or flashed for this slice.
Physical Tab5 graphics/input acceptance and performance were not performed.

- [ ] Run Snake from microSD; verify arrows/WASD, pause, restart, score, and quit.
- [ ] Verify canvas scaling, colors, packed blits, clipping, and display restoration.
- [ ] Verify Ctrl-C/Ctrl-D and error cleanup, repeated launches, and usable shell input.
- [ ] Measure frame construction/presentation, interruption latency, and stack/heap
  high-water usage under representative Lua game load.

Host RV32 execution timing does not predict native Tab5 frame rate. CLI acceptance
items already checked by the operator remain preserved in `agents/apps/lua.md`.
