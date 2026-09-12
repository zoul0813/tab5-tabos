# Lua for TabOS

Independent PUC Lua 5.5.1 application. Build and install with `./apps/build.sh`
from the repository root. On TabOS, run `lua --help` or `lua` for the prompt.

See [user documentation](../../docs/lua.md), [upstream provenance](UPSTREAM.md)
and [license](LICENSE). Sources and the selected upstream tests are vendored for
offline builds. Lua canvas drawing and keyboard input support games written entirely
in Lua; run `lua T:/data/lua/snake.lua` for the bundled example. Audio, pointer,
tile/sprite and child-process bindings remain follow-on work. Physical CLI and
graphics acceptance is still pending.
