# Starfall

Starfall is a standalone 640x360 arcade shooter demonstrating TabOS scaled RGB565
graphics, raw keyboard input, fixed-step timing, and persistent filesystem data.

## Controls

- A/S: move left/right
- K: start, fire, or restart
- P: pause
- Q or Escape: exit

Build and install it into the host root filesystem with:

```sh
make -C apps/starfall
```

Run `starfall` from the TabOS shell. High score is stored at
`T:/data/starfall/highscore.dat`. Storage failure is nonfatal. Starfall currently has
no audio. Saves write and close a temporary file before renaming it over the score;
a reported rename failure removes only the temporary file and preserves the prior score.

A single-file Lua port is installed as `T:/data/lua/starfall.lua` by the Lua app.
Run `lua T:/data/lua/starfall.lua` with the same controls. It includes the artwork
and font in Lua and keeps a separate high score. See [Lua games](../../docs/lua.md#games-written-in-lua).
