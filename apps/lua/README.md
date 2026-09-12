# Lua for TabOS

Independent PUC Lua 5.5.1 application. Build and install with `./apps/build.sh`
from the repository root. On TabOS, run `lua --help` or `lua` for the prompt.

See [user documentation](../../docs/lua.md), [upstream provenance](UPSTREAM.md)
and [license](LICENSE). Sources and the selected upstream tests are vendored for
offline builds. Lua canvas drawing and keyboard input support games written entirely
in Lua; run `lua T:/data/lua/snake.lua` for the bundled example. Snake includes Lua-generated audio matching native Snake. PCM playback is available;
Tile/sprite and child-process bindings remain follow-on work. Physical CLI and
graphics acceptance is still pending.

Run `lua T:/data/lua/starfall.lua` for the single-file port of `apps/starfall`.
It includes all artwork and its font: A/S move, K starts/fires/restarts, P pauses,
and Q or Escape quits. High scores use `T:/data/lua/starfall-highscore.dat`;
storage failure is nonfatal. Starfall has no audio, matching the native game.

Run `lua T:/data/lua/touch.lua` for a pointer/touch demo. Touch or click to move
the square, then drag; Q or Escape quits. Screen-owned pointer streams expose
down/move/up/cancel, contact IDs, optional pressure, and canvas coordinates.
The same file runs with Tab5 touch or host mouse/touch input.
