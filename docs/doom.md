# DOOM

TabOS has an optional work-in-progress port of `ozkl/doomgeneric`, pinned to
`dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`. It is GPLv2 upstream code and stays
outside normal application builds and release artifacts.

Build it explicitly after activating or installing the project toolchain:

```sh
./apps/build.sh --with-doom
```

This performs pinned fetch, build, and installation to `.local/rootfs/T/bin/doom`.
DOOM remains excluded from plain `./apps/build.sh`. Individual targets remain available:

```sh
make -C apps/doom fetch
make -C apps/doom build
make -C apps/doom install
```

To copy an existing or newly built executable to Tab5 MSC storage with other apps:

```sh
./apps/build.sh build --msc --with-doom
```

This writes extensionless `bin/doom`. DOOM binary and WAD files remain absent from
published TabOS artifacts.

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

Current stripped executable is 902,264 bytes on disk. ELF section totals are 421,636
bytes text, 60,084 bytes data, and 241,072 bytes BSS. `.note.tabos` metadata version 1
has a 32-byte descriptor, requires application ABI 1 and capability bits 1 (console), and
requests an 8 MiB heap plus 64 KiB stack.
