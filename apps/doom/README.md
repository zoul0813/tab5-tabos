# DOOM for TabOS

Optional TabOS port of [ozkl/doomgeneric](https://github.com/ozkl/doomgeneric), pinned
to commit `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`. It is GPL-2.0-only; upstream
source is checked out under ignored `apps/doom/doomgeneric/` and is never vendored here.

TabOS provides graphics, timing, and keyboard integration. DOOM renders its 320x200 RGB
framebuffer as RGB565, then scales it to a centered 960x720 4:3 image with black side
bars. Controls use W/S to move, A/D to strafe, arrows to turn, J or Control to fire,
E or Space to use, Shift to run, R to toggle always-run, number keys for weapons, and
standard Escape/Enter/arrow/Y/N/F-key menu controls. This is not a normal TabOS release
artifact.

## Install game data

DOOM uses `T:/data/doom` as its private working directory. Put one or more legally
obtained IWAD files there. With no `-iwad` argument, it uses the first readable file in
this order:

1. `doom1.wad`
2. `doom.wad`
3. `doom2.wad`
4. `freedoom1.wad`
5. `freedoom2.wad`

For another location or filename, pass a drive-qualified path, for example:

```text
doom -iwad S:/doom/custom.wad
```

DOOM creates `T:/data/doom` when needed. Configuration, screenshots, recorded demos,
and savegames are written beneath that directory. Its process-local working-directory
change does not alter the parent shell's directory. If no readable regular IWAD exists,
DOOM prints installation guidance and exits nonzero.

## Build

```sh
make -C apps/doom fetch
make -C apps/doom build
make -C apps/doom install
```

`fetch` clones only the pinned commit, verifies `HEAD`, copies it to
`build/apps/doom/source`, then applies repository patches there. It never modifies a
user-provided source directory. For offline or pre-fetched sources, point
`DOOMGENERIC_SOURCE_DIR` at a Git checkout whose `HEAD` is that exact commit:

```sh
make -C apps/doom build DOOMGENERIC_SOURCE_DIR=/path/to/doomgeneric-checkout
```

Only upstream engine files listed in `apps/doom/Makefile` compile. SDL, X11, Windows,
Emscripten, Allegro, Linux-VT, and SOSO platform files remain excluded.

The built program requests 8 MiB heap and 64 KiB stack. It uses RV32I (`-march=rv32i`),
never compressed instructions, and links newlib plus `libm`. Upstream compilation uses
`-w`; TabOS-owned adapter compilation remains `-Wall -Wextra -Werror`.

## License and data

Upstream `LICENSE` is GNU GPL version 2. TabOS distributes neither upstream source nor
any DOOM executable in standard release artifacts. WAD files are never downloaded or
committed. Users must provide shareware, Freedoom, or legally owned commercial game
data.

## Patches

`patches/series` records source compatibility patches. Phase 6 makes upstream normal
quit and error statuses explicit. Patches apply only to `build/apps/doom/source`, never
the ignored upstream checkout or an external `DOOMGENERIC_SOURCE_DIR` checkout.
