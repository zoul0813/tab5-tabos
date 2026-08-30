# DOOM

TabOS has an optional work-in-progress port of `ozkl/doomgeneric`, pinned to
`dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`. It is GPLv2 upstream code and stays
outside normal application builds and release artifacts.

Build it explicitly after activating or installing the project toolchain:

```sh
make -C apps/doom fetch
make -C apps/doom build
make -C apps/doom install
```

For offline use, `DOOMGENERIC_SOURCE_DIR` may point to a pre-fetched Git checkout, but
its checked-out `HEAD` must exactly equal the pinned commit. TabOS verifies this before
compiling. Fetching uses ignored `apps/doom/doomgeneric/` storage; patches apply only to
a disposable build copy under `build/`.

No WAD files are supplied, committed, or downloaded. Users must supply shareware,
Freedoom, or legally owned commercial game data themselves. Copy IWADs to
`T:/data/doom`; with no arguments, DOOM searches there in this order:

1. `doom1.wad`
2. `doom.wad`
3. `doom2.wad`
4. `freedoom1.wad`
5. `freedoom2.wad`

DOOM creates the directory when needed. A drive-qualified path can override discovery,
for example `doom -iwad S:/doom/custom.wad`. Configuration, screenshots, demos, and
savegames also stay beneath `T:/data/doom`. The working-directory change belongs only
to the DOOM process and does not change the parent shell directory.

Current runtime support converts DOOM's 320x200 RGB framebuffer to RGB565 and presents
it as a centered 960x720 4:3 image. Keyboard controls are W/S movement, A/D strafing,
arrow turning, J or Control fire, E or Space use, Shift run, R always-run toggle, number
weapon selection, and standard menu keys.
