# Build and Load Tile Assets

The recommended workflow keeps editable PNG and Tiled files in the application source,
then generates compact binary files for the TabOS filesystem.

## Add the Application Makefile

Create `apps/mygame/Makefile`:

```make
APP_NAME := mygame
SOURCES := src/main.c
ASSET_BUILD := ../../build/apps/$(APP_NAME)/generated
ASSET_HEADER := include/mygame.h
TABOS_RUNTIME_ASSETS := $(ASSET_BUILD)/mygame.tsp $(ASSET_BUILD)/level.tmap
TABOS_BUILD_PREREQUISITES := $(ASSET_HEADER)
TABOS_HOST_PYTHON ?= python3

.PHONY: mygame-assets
mygame-assets: assets/manifest.json assets/level.tmj assets/tiles.tsj assets/tiles.png
	"$(TABOS_HOST_PYTHON)" "$(PROJECT_ROOT)/tools/tabos" assets build assets/manifest.json --output "$(ASSET_BUILD)" --header-output "$(ASSET_HEADER)"

$(ASSET_HEADER) $(TABOS_RUNTIME_ASSETS): mygame-assets
	@test -f $@

include ../../sdk/make/application.mk
```

`TABOS_BUILD_PREREQUISITES` generates the public ID header before compiling `main.c`.
`TABOS_RUNTIME_ASSETS` tells the application build which binary files to install under
`T:/data/mygame/`.

## Generate the Assets

Run:

```sh
make -C apps/mygame mygame-assets
```

The converter writes:

| File | Purpose |
| --- | --- |
| `apps/mygame/include/mygame.h` | Constants included by game source and committed with it. |
| `build/apps/mygame/generated/mygame.tsp` | Images, sprites, animations, metasprites, and flags. |
| `build/apps/mygame/generated/level.tmap` | Map dimensions, cells, layers, objects, and properties. |
| generated `.c` and `.h` files | Optional compiled-in representation and converter output. |

The developer-facing `include/mygame.h` contains constants only. It may look like:

```c
#define MYGAME_FLAG_SOLID 1U
#define MYGAME_SPRITE_PLAYER_0 4U
#define MYGAME_ANIMATION_PLAYER_WALK 0U
#define MYGAME_METASPRITE_PLAYER_SHADOW 0U
#define MYGAME_LAYER_LEVEL_GROUND 0U
#define MYGAME_LAYER_LEVEL_FOREGROUND 1U
#define MYGAME_LAYER_LEVEL_MARKERS 2U
#define MYGAME_OBJECT_LEVEL_SPAWN 1U
```

Include these names from C. Do not copy their numeric values into game code. Regenerate
the header whenever names or IDs change in Tiled or the manifest. Commit the generated
header so editors and clean source checkouts can resolve the names.

## Build and Install the Application

Build and install the application into the local TabOS root filesystem:

```sh
make -C apps/mygame
```

The executable is installed as `T:/bin/mygame`. Its runtime assets are installed as:

```text
T:/data/mygame/mygame.tsp
T:/data/mygame/level.tmap
```

To build all maintained applications, use `./apps/build.sh`. To copy built applications
and declared runtime assets to a mounted Tab5 MSC volume, use the documented
[`./apps/build.sh --msc` workflow](../../applications.md#building-applications).

## Load the Runtime Files

Include the generated names and public SDK headers:

```c
#include <mygame.h>
#include <tabos/graphics.h>
#include <tabos/sprite.h>
#include <tabos/tilemap.h>
```

Initialize every destination to zero, then load it:

```c
tabos_sprite_set_t sprites = {0};
tabos_tilemap_t map = {0};

if (tabos_sprite_set_load("T:/data/mygame/mygame.tsp", &sprites) != 0 ||
    tabos_tilemap_load("T:/data/mygame/level.tmap", &map) != 0) {
    /* Report errno and leave the application. */
}
```

Keep both assets alive while any draw using them may still be queued. After the final
`tabos_graphics_present()`, release them:

```c
tabos_tilemap_unload(&map);
tabos_sprite_set_unload(&sprites);
```

Unload a successfully loaded value before loading a replacement into it. Failed loads
leave the destination unchanged. The exact loader errors and validation guarantees are
listed under [Runtime API](../../tile-assets.md#runtime-api).
