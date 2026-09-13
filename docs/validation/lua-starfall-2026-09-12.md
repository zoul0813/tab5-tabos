# Lua Starfall validation — 2026-09-12

The single-file `apps/lua/examples/starfall.lua` ports native Starfall's 640×360
artwork, embedded font, controls, fixed-step gameplay, enemy patterns, scoring,
particles and game states using the existing Lua graphics/input/file APIs.
Scores use `T:/data/lua/starfall-highscore.dat`; storage errors are nonfatal.
There are no runtime binding, SDK or firmware changes.

## Automated results

- `./apps/build.sh`: passed. Staged and installed `starfall.lua` both match the
  15167-byte source. Lua ELF remains 383256 total text/data/BSS bytes.
- `./tools/tabos macos debug build`: passed.
- `./tools/tabos macos release build`: passed.
- Focused CTest selection below: 11/11 passed in Debug (8.59 seconds, including
  ASan/UBSan) and Release (1.68 seconds).
- Real RV32 integration below: passed, including Starfall title/player/pause
  framebuffer pixels, Q exit, Escape after relaunch and terminal restoration.
- `git diff --check` and C formatting check: passed.

```sh
ctest --test-dir build/macos-debug \
  -R 'lua|starfall|sdk_graphics|naming|application_api' --output-on-failure
ctest --test-dir build/macos-release \
  -R 'lua|starfall|sdk_graphics|naming|application_api' --output-on-failure
build/macos-release/tests/tabos_lua_rv32 \
  build/apps/shell/shell build/apps/lua/lua \
  apps/lua/examples/snake.lua apps/lua/examples/starfall.lua
```

`component.lua_starfall` checks the shipped game logic against expected RNG,
movement, firing, collision, enemy, wave, immunity, pause/death/restart and pool
behavior. It uses real host file I/O for score persistence, malformed/missing
data and injected replacement failure. A second run executes the complete script
with scheduled keyboard input wrapping a real SDK canvas, checking held movement
and firing, repeat suppression, pause/resume and close. RV32 validation uses the
same SDK-built Lua artifact as Tab5, through the host interpreter and runtime.

## Physical acceptance remaining

- [ ] Play on Tab5: A/S movement, held K firing, P pause/resume, restart, Q/Escape exit.
- [ ] Check artwork, frame rate, input latency and memory use during busy waves.
- [ ] Verify microSD score save/reload and nonfatal unavailable/read-only storage.
- [ ] Verify Ctrl-C/Ctrl-D interruption and a usable terminal after repeated launches.

Linux builds/tests were excluded by user direction. No device was flashed.
Automated fixed-step checks do not establish a measured 60 FPS presentation rate.
