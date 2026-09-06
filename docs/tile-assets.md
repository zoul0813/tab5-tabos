# Sprite and Tile Assets

TabOS sprite/tile support is a portable SDK layer over RGB565 blits. It owns no
textures, scene graph, entities, collision solver, or physics state. Keep asset memory
unchanged until `tabos_graphics_present()` completes.

## Runtime API

`<tabos/sprite.h>` provides zero-based sprite regions, pivots, 32-bit application flags,
explicit-time animation, ordered metasprites, and transformed/clipped drawing.
`TABOS_SPRITE_NONE` is invalid. Repeat count zero loops forever; positive counts hold the
last frame after that many cycles.
Each animation descriptor carries a `trigger_sprite`; tile rendering uses it to
associate Tiled's animated tile with its frame sequence even when first frame is a
different tile.

`tabos_sprite_animation_sprite()` returns the sprite ID selected for an animation at an
explicit elapsed time. It returns `TABOS_SPRITE_NONE` with `errno` set for invalid input.
Use it when game code needs the selected sprite without drawing it.

`tabos_sprite_animation_draw()` selects and draws a frame in one call, at natural size
and full opacity. `tabos_sprite_animation_draw_ex()` accepts the existing sprite draw
options for size, rotation, mirrors, opacity, and clipping. Initialize extended options
with `TABOS_SPRITE_DRAW_OPTIONS_DEFAULT`: natural size, full opacity, no rotation or
mirroring, and no clipping. Override only fields needed by the draw:

```c
tabos_sprite_draw_options_t options = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
options.mirror_x = true;
tabos_sprite_draw_ex(&graphics, &sprites, PLAYER, player.x, player.y, &options);
```

This initializer works with both sprite and animated `_ex` calls. Setting `opacity` to
zero afterward remains an explicit fully transparent draw. A plain `{0}` initializer
has zero opacity; use the named default whenever default visible drawing is intended.

Sprite pivots are local geometric origins measured from the source rectangle's top-left
edge. `(0, 0)` is its top-left corner and `(width / 2, height)` is its bottom-center edge.
Origins may lie outside the sprite for attachment points. Drawing places the transformed
origin at the requested world coordinate. Mirroring and quarter-turn rotation transform
the origin with the image. Scaling multiplies it by destination size divided by natural
size, with integer division truncating toward zero.

Pass elapsed milliseconds since this actor started its current clip, not a frame delta.
`tabos_sprite_animation_finished()` returns `0` on success and writes a boolean result:
looping clips never finish; finite clips finish after the final frame's full duration in
the final cycle. Drawing a completed clip holds its final frame. Completion remains true
on later queries; it is not a one-time event. Failures return `-1` with `errno` and leave
the output boolean unchanged. Missing clips, empty clips, zero-duration frames, and
invalid frame sprite IDs are rejected, including manually constructed C descriptors.

These helpers allocate no playback state. Each actor owns its clip ID and start time;
game code chooses transitions. For example, inside a game draw function returning `0`/`-1`,
called between camera begin/end:

```c
uint64_t elapsed_ms = now_ms - player.animation_started_ms;
bool finished = false;
if (tabos_sprite_animation_finished(&sprites, player.animation, elapsed_ms, &finished) != 0) {
    return -1;
}
if (finished) {
    player.animation = PLAYER_IDLE; /* Game's generated fallback animation ID. */
    player.animation_started_ms = now_ms;
    elapsed_ms = 0;
}
return tabos_sprite_animation_draw(
    &graphics, &sprites, player.animation, player.x, player.y, elapsed_ms);
```

Pause by freezing elapsed time; restart by resetting the actor's start time.
`tabos_sprite_animation_sprite()` remains available when the game needs a sprite ID
without drawing.

`<tabos/tilemap.h>` uses 32-bit `tabos_tile_t`. Zero is empty; low 28 bits hold a
one-based sprite ID; high bits preserve Tiled horizontal, vertical, and diagonal flips.
Use `TABOS_TILE()` and `TABOS_TILE_ID()` with normal zero-based IDs. Maps retain ordered
tile/object layers. Cells are writable in memory. Applications draw layers individually
and own collision/gameplay. Object layers expose point, rectangle, and tile markers plus
named integer properties; they never draw automatically.

Set the shared graphics camera once for the world pass. All sprite variants, metasprites,
map layers, and primitive object markers then use world coordinates without manual subtraction:

```c
tabos_tilemap_draw_options_t draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
draw.animation_ms = elapsed_ms;
tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
tabos_tilemap_draw_layer(&graphics, &map, GROUND_LAYER, &sprites, &draw);
tabos_sprite_animation_draw(&graphics, &sprites, player.animation, player.x, player.y, elapsed_ms);
tabos_tilemap_draw_layer(&graphics, &map, FRONT_LAYER, &sprites, &draw);
tabos_graphics_end_camera(&graphics);
/* Draw HUD in screen coordinates, then present. */
```

`TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT` uses the full graphics canvas at animation time zero.
Copy it and override `animation_ms` for the common full-canvas animated case. `viewport`
is a screen-space clip, intersected with canvas bounds before visible-cell selection.
An explicitly supplied empty viewport draws nothing. Maps remain anchored at world
`(0, 0)`; sprites and maps share the same projection. See
[graphics camera semantics](graphics-api.md).

Pre-release API migration: `tabos_tilemap_draw_options_t.camera_x` and `.camera_y` were
removed. Move those values to `tabos_graphics_begin_camera()` and remove camera subtraction
from sprite/object drawing. An old nonzero viewport origin also relocated the map;
the new viewport only clips. To preserve that old map placement, begin the camera at
`old_camera_x - viewport.x`, `old_camera_y - viewport.y`. Rebuild applications against
the updated SDK; asset formats and the private ELF call table are unchanged.

Load `.tsp` and `.tmap` files with `tabos_sprite_set_load()` and
`tabos_tilemap_load()`. Matching unload functions free owned data and zero the object.
Initialize the destination to zero and unload a successfully loaded asset before loading
another into it. Failed loads leave the destination unchanged and release temporary
allocations. Malformed files return `-1` with `EINVAL`; allocation failures use `ENOMEM`.
Unload only after the final present completes or graphics closes.

Loaders validate file size, aligned table bounds, non-overlapping descriptor sections,
references, and terminated strings. Pixel data and writable map cells cannot overlap
headers or descriptor tables; map cells also cannot overlap strings. Sprite regions and
animation clips must be nonempty, frame durations positive, and map/tile dimensions fit
positive signed 32-bit coordinates. The map loader rejects the reserved GID bit; checking
whether a nonempty cell's sprite ID exists in a particular sprite set happens at draw time.

## Game Recipes

Generated names let game code avoid numeric IDs. If the manifest is named `mygame`, a
Tiled tile named `player`, map named `level`, and layers named `ground` and `foreground`
produce names like:

```c
MYGAME_SPRITE_PLAYER
MYGAME_ANIMATION_PLAYER_WALK
MYGAME_METASPRITE_PLAYER_SHADOW
MYGAME_LAYER_LEVEL_GROUND
MYGAME_LAYER_LEVEL_FOREGROUND
MYGAME_OBJECT_LEVEL_SPAWN
MYGAME_FLAG_SOLID
```

The constants-only app header is safe to include with binary-backed assets. This complete
one-frame program shows loading, world drawing, HUD placement, presentation, and cleanup:

```c
#include <mygame.h>
#include <tabos/graphics.h>
#include <tabos/runtime_time.h>
#include <tabos/sprite.h>
#include <tabos/tilemap.h>

int main(void)
{
    tabos_graphics_t graphics = {.width = 320U, .height = 180U};
    tabos_sprite_set_t sprites = {0};
    tabos_tilemap_t map = {0};
    if (tabos_graphics_open(&graphics) != 0 ||
        tabos_sprite_set_load("T:/data/mygame/mygame.tsp", &sprites) != 0 ||
        tabos_tilemap_load("T:/data/mygame/level.tmap", &map) != 0) {
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        if (graphics.open) {
            (void) tabos_graphics_close(&graphics);
        }
        return 1;
    }

    const int32_t camera_x = 0;
    const int32_t camera_y = 0;
    const int32_t player_x = 80;
    const int32_t player_y = 68;
    const uint64_t animation_started_ms = tabos_monotonic_ms();
    const uint64_t elapsed_ms = tabos_monotonic_ms() - animation_started_ms;
    tabos_tilemap_draw_options_t map_draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
    map_draw.animation_ms                  = elapsed_ms;

    (void) tabos_graphics_clear(&graphics, TABOS_RGB565(8, 18, 30));
    (void) tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
    (void) tabos_tilemap_draw_layer(
        &graphics, &map, MYGAME_LAYER_LEVEL_GROUND, &sprites, &map_draw);
    (void) tabos_sprite_animation_draw(
        &graphics, &sprites, MYGAME_ANIMATION_PLAYER_WALK, player_x, player_y, elapsed_ms);
    (void) tabos_tilemap_draw_layer(
        &graphics, &map, MYGAME_LAYER_LEVEL_FOREGROUND, &sprites, &map_draw);
    (void) tabos_graphics_end_camera(&graphics);
    /* Draw screen-space HUD here. */
    (void) tabos_graphics_present(&graphics);

    /* After the final present, normally during shutdown: */
    tabos_tilemap_unload(&map);
    tabos_sprite_set_unload(&sprites);
    return tabos_graphics_close(&graphics) == 0 ? 0 : 1;
}
```

Check return values in production code. Draw operations may queue source pointers, so
loaded assets must remain alive and unchanged until `tabos_graphics_present()` completes.
The [`tdemo` source](../apps/tile-demo/src/main.c) adds a real input/render loop, camera
movement, a metasprite, object markers, and a writable cell using the same API.

### Draw a Sprite

The position passed to a sprite draw is its pivot position in world coordinates:

```c
tabos_sprite_draw(&graphics, &sprites, MYGAME_SPRITE_CRATE, x, y);

tabos_sprite_draw_options_t draw = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
draw.width = 32U;
draw.height = 32U;
draw.mirror_x = facing_left;
draw.opacity = 192U;
draw.clip_enabled = true;
draw.clip = playfield; /* Screen coordinates. */
tabos_sprite_draw_ex(&graphics, &sprites, MYGAME_SPRITE_PLAYER, x, y, &draw);
```

Width and height zero independently select natural post-rotation dimensions. Opacity
zero draws nothing; 255 is fully opaque. Clips stay in screen coordinates when a camera
is active.

### Draw a Metasprite

A metasprite combines ordered sprites, useful for shadows, equipment, vehicles, and
multi-part actors. Define parts in the manifest:

```json
{
  "name": "player_shadow",
  "parts": [
    {"sprite": "shadow", "x": 2, "y": 0, "opacity": 128},
    {"sprite": "player", "x": 0, "y": 0},
    {"sprite": "sword", "x": 7, "y": -6, "rotation": 1}
  ]
}
```

Parts draw in listed order, so the sword appears over the player and both appear over the
shadow. Offsets are signed world offsets from the metasprite origin. Rotation values are
the `tabos_graphics_rotation_t` quarter turns `0` through `3`; `mirror_x`, `mirror_y`, and
`opacity` default to false, false, and 255.

```c
tabos_metasprite_draw(
    &graphics,
    &sprites,
    MYGAME_METASPRITE_PLAYER_SHADOW,
    player_x,
    player_y,
    facing_left,
    false,
    255U);
```

Overall mirroring reverses part offsets and combines with each part's own mirrors.
Overall opacity multiplies part opacity. Metasprites intentionally have no overall scale,
clip, animation state, or arbitrary-angle rotation.

### Read Flags and Edit Cells

Flags are game-defined bits. TabOS stores and returns them but provides no collision
solver:

```c
tabos_tile_t tile = TABOS_TILE_EMPTY;
if (tabos_tilemap_get(&map, MYGAME_LAYER_LEVEL_GROUND, column, row, &tile) == 0 &&
    tile != TABOS_TILE_EMPTY) {
    uint32_t sprite = TABOS_TILE_ID(tile);
    if ((tabos_sprite_flags(&sprites, sprite) & MYGAME_FLAG_SOLID) != 0U) {
        /* Block movement. */
    }
}

tabos_tilemap_set(
    &map,
    MYGAME_LAYER_LEVEL_FOREGROUND,
    column,
    row,
    TABOS_TILE(MYGAME_SPRITE_OPEN_DOOR));
```

`TABOS_TILE()` encodes a normal zero-based sprite ID. OR it with
`TABOS_TILE_FLIP_HORIZONTAL`, `TABOS_TILE_FLIP_VERTICAL`, or
`TABOS_TILE_FLIP_DIAGONAL` when needed. `TABOS_TILE_ID()` returns the sprite ID and
ignores transform bits; for an empty cell it returns `TABOS_SPRITE_NONE`. Cell edits live
only in memory and are never saved automatically.

### Read Object Markers

Object-layer constants identify a layer index. Object constants preserve Tiled object
IDs; they are not array indexes. Look up one named marker directly:

```c
const tabos_tilemap_object_t* spawn = tabos_tilemap_object(
    &map,
    MYGAME_LAYER_LEVEL_MARKERS,
    MYGAME_OBJECT_LEVEL_SPAWN);
if (spawn != NULL) {
    player_x = spawn->x;
    player_y = spawn->y;
}
```

An invalid map or non-object layer returns `NULL` with `EINVAL`; a missing object ID
returns `NULL` with `ENOENT`. Iterate the selected layer when processing every marker:

```c
const tabos_tilemap_layer_t* layer = &map.layers[MYGAME_LAYER_LEVEL_MARKERS];
if (layer->type != TABOS_TILEMAP_LAYER_OBJECTS) {
    /* Wrong generated layer constant for this operation. */
}

for (uint32_t index = 0U; index < layer->object_count; ++index) {
    const tabos_tilemap_object_t* object = &layer->objects[index];
    if (object->id == MYGAME_OBJECT_LEVEL_SPAWN) {
        player_x = object->x;
        player_y = object->y;
    }
    if (object->shape == TABOS_TILEMAP_OBJECT_RECTANGLE) {
        int32_t damage = 0;
        if (tabos_tilemap_object_property(object, "damage", &damage) == 0) {
            /* Create a rectangular damage trigger. */
        }
    }
}
```

Point, rectangle, and tile objects expose integer positions and dimensions. The converter
normalizes tile-object positions to the same top-left convention used by rectangles, then
rounds fractional Tiled geometry as described below and reports each rounding adjustment.
Tile objects also expose an encoded `tile` value. `name` is the Tiled object name; `type`
is its class, falling back to the legacy Tiled type. Missing properties return `ENOENT`.
Generated object constants work with `tabos_tilemap_object()` and during iteration.
Object layers do not render themselves; game code may create entities, triggers, or debug
markers from them.

## Authoring

The normal workflow is:

1. Create a finite orthogonal JSON map (`.tmj`) in Tiled.
2. Use atlas tilesets embedded in the map or stored as JSON `.tsj` files.
3. Name tiles and animations with the TabOS properties below.
4. Add tile layers in desired draw order and object layers for markers.
5. Reference the map from the version-1 manifest.
6. Run `tabos assets build`, then include its generated app header.

Use integral map dimensions and zero tile-layer offsets. Object placement may be fractional;
conversion rounds it to logical pixels with warnings. Layer visibility and opacity are
authoring aids; the runtime imports supported layers and game code chooses which tile layers
to draw.

Run:

```sh
./tools/tabos assets build path/to/manifest.json --output build/assets
```

For a manifest named `mygame` with a map named `level`, this produces:

| Output | Application use |
| --- | --- |
| `mygame.tsp` | Load all images, sprites, animations, metasprites, and flags. |
| `level.tmap` | Load the writable map and its ordered tile/object layers. |
| `mygame.c` | Optional compiled-in descriptors and pixels. |
| `mygame.h` | Generated-C declarations plus every named ID constant. |

The separate `--header-output` file described below contains only named constants used by
binary-backed applications. Binary maps are loaded individually by their `.tmap` path;
layer and object constants identify data inside the loaded map. No map-ID constant is
generated because the runtime has no map collection for such a value to index.

For application code that uses generated IDs, also place a public header in the app's
include directory:

```sh
./tools/tabos assets build path/to/manifest.json \
    --output build/assets \
    --header-output apps/my-game/include/my-game-assets.h
```

The output directory still receives matching generated C, header, and binary assets.
`--header-output` writes a constants-only header at the requested developer-facing path.
It omits the generated descriptor declarations used only when compiling the generated C
form. Commit that public header with the application so constants remain available to
editors and clean source checkouts. It carries a generated-file warning and must be
regenerated from the manifest/Tiled sources rather than edited by hand.

Generated constants use the asset-set name as collision-safe prefix. Tiled tile `name`
and `animation_name` string properties, native map/layer/object names, manifest
metasprite names, and flag names produce constants such as:

```c
GAME_SPRITE_PLAYER
GAME_ANIMATION_PLAYER_WALK
GAME_METASPRITE_PLAYER
GAME_LAYER_WORLD_FOREGROUND
GAME_OBJECT_WORLD_SPAWN
GAME_FLAG_SOLID
```

Application makefiles can list the public header in `TABOS_BUILD_PREREQUISITES`, ensuring
asset generation completes before any source that includes it is compiled. Identifier
collisions after C-name sanitization are rejected.

Converter requires Pillow. Install pinned host dependency with
`python3 -m pip install -r tools/requirements.txt` when it is not already available.
Application builds preserve this host Python when activating ESP-IDF, so IDF's
isolated Python environment does not need a second Pillow installation.

Version-1 JSON manifests contain `name`, an optional `flags` object, and arrays named
`images`, `animations`, `metasprites`, and `maps`. Each named flag must be one unique,
nonzero bit in a 32-bit value. Combine generated flag macros in game code rather than
assigning multiple bits to one manifest name. Paths are relative to the manifest. Example:

```json
{
  "version": 1,
  "name": "game",
  "flags": {"solid": 1},
  "metasprites": [
    {"name": "actor", "parts": [{"sprite": "wall", "x": 0, "y": 0}]}
  ],
  "maps": [{"name": "level", "source": "level.tmj"}]
}
```

`version` must be the integer `1`; a boolean or string is not accepted. The manifest root,
each entry, and each nested frame, part, or region must have the documented JSON object or
array form. Standalone image entries accept PNG or GIF files. Tiled atlas images accept PNG
files. Other formats fail during conversion instead of depending on whichever decoders a
local Pillow installation happens to provide.

Standalone PNGs may become one full-image sprite or several named regions:

```json
{
  "name": "actors",
  "source": "actors.png",
  "transparent_rgb": [255, 0, 255],
  "transparent_tolerance": 8,
  "sprites": [
    {
      "name": "player",
      "x": 0,
      "y": 0,
      "width": 16,
      "height": 24,
      "pivot": [8, 24],
      "flags": ["solid"]
    }
  ]
}
```

Place this object in the manifest's `images` array. Omit `sprites` to create one sprite
covering the whole image. Optional `resize: [width, height]` uses nearest-neighbor scaling.
Optional `color_key` selects an explicit RGB565 transparency key; otherwise transparent
input receives a deterministic unused key. The explicit key is rejected if an opaque
source pixel converts to the same RGB565 value.

An asset set may contain any combination of standalone PNG/GIF entries and images imported
through Tiled tilesets. Each source remains an independent image descriptor with its own
dimensions, pixels, and transparency key; conversion does not require or perform physical
atlas repacking. Sprites keep the corresponding image index. Named animations and metasprites
may freely combine sprites from different source images. Each manifest image source must be
unique. A source already owned by an imported Tiled tileset must not also appear under manifest
`images`; the converter rejects either duplication with the conflicting path.

An animated GIF image entry generates one full-frame sprite per GIF frame and one
animation using the image entry's name. Animated GIF entries cannot define multiple
regions. `durations_ms` may override every frame duration, and `repeat_count` may override
GIF loop metadata.

Manual animations reference previously imported sprite names:

```json
{
  "name": "player_jump",
  "repeat_count": 1,
  "frames": [
    {"sprite": "player_jump_0", "duration_ms": 80},
    {"sprite": "player_jump_1", "duration_ms": 120}
  ]
}
```

GIF frames honor transparency, disposal, delay, and loop metadata. Delays below 10 ms
become 10 ms unless `durations_ms` overrides them. Alpha must be exactly 0 or 255.
Transparency gets a deterministic unused RGB565 key; explicit `color_key` must not
collide with opaque pixels. `resize` performs nearest-neighbor import resizing.
`transparent_rgb` and optional `transparent_tolerance` key deliberately opaque pixel
art before strict alpha validation. A pixel matches when the absolute difference of each
red, green, and blue component is at most the tolerance.

Maps may be finite orthogonal Tiled JSON with multiple atlas tilesets, tile/object
layers, transforms, animations, integer/boolean tile properties, and integer object
properties. Point, rectangle, and tile objects with zero rotation are accepted. Fractional
object positions and dimensions are rounded to the nearest logical pixel, with exact halves
rounded away from zero. The converter prints a warning naming the map, object, field,
alignment-normalized value, and rounded value for every adjustment. Unsupported map modes,
nonnumeric or out-of-range object geometry, partial alpha, unknown GIDs, and the reserved
GID bit fail with diagnostics.
Map and tile dimensions must be positive and fit the runtime's 32-bit fields; tile-layer
dimensions must match the map and offsets must be
zero. GIDs, object IDs, object sizes, and property values are range-checked before binary
generation. Diagnostics identify the input path, field, layer, tileset, object, or GID
where applicable.

Runtime object `x,y` always identifies the top-left of its bounding box. Tiled rectangle
objects already use that origin. For tile objects, the converter resolves the owning tileset's
`objectalignment` and translates its anchor to top-left. Tiled's `unspecified` alignment means
`bottomleft` for the supported orthogonal maps, so the converter subtracts the object height.
All nine explicit Tiled alignments are supported.

When a manifest imports a Tiled map, its TMJ/TSJ files are authoritative for atlas
image paths, regions, names, pivots, flags, transparency, and tile animations. The manifest
cannot override those fields, and the converter rejects listing the same tileset image under
manifest `images`.
The converter loads maps before manifest animations and metasprites, allowing those
entries to reference sprites named in TSJ tile properties. Supported TSJ metadata is:

- `name` string tile property: generated sprite name.
- `pivot_x` and `pivot_y` integer tile properties: local geometric origin. Sprite drawing
  places this origin at the requested destination coordinate; `(0, 0)` anchors the
  top-left corner, while `(width / 2, height)` anchors the bottom-center edge.
- `animation_name` string tile property: name of that tile's Tiled animation.
- `repeat_count` integer property on an animated tile: `0` loops forever (default),
  `1` plays once, and larger values play that many complete cycles. Values must fit an
  unsigned 32-bit integer; wrong types, negative values, and use on a tile without an
  animation are rejected. This TabOS property survives both generated C and binary output
  and applies to animated map tiles as well as explicitly drawn sprites. Define each named
  walk/run/jump clip on its own animated tile.
- Manifest flag names as boolean/integer tile properties: sprite flag bits.
- A registered Boolean property sets its flag when true. A registered integer property
  sets its flag when nonzero. False or zero leaves the flag clear. Other integer/Boolean
  tile properties may remain in Tiled for editor or game tooling, but the converter ignores
  them and they are unavailable through the runtime sprite descriptor.
- Standard tileset `transparentcolor`, plus optional integer tileset property
  `transparent_tolerance` from 0 through 255: color-key preparation.

Tiled tile-animation durations are preserved exactly in milliseconds and must be positive.
The 10-ms minimum applies only to GIF delays without a manifest duration override.

Keep manifest content for orchestration and metadata Tiled cannot represent, such as
asset-set name, flag-bit registry, standalone PNG/GIF imports, and metasprite composition.

Output contains deterministic generated `.c`/`.h`, one `TSP1` `.tsp`, and a `TMP1`
`.tmap` per map. Binary tables are little-endian, four-byte aligned, offset/count based,
and uncompressed.

## Validation

The host test suite compiles generated C and loads binary assets from the same synthetic
PNG/Tiled fixture. It compares metadata and exact logical-canvas pixels, including
independent expected colors and layer order. Run it through the normal host workflow:

```sh
./tools/tabos macos debug test
# After building, run only the equivalence check:
ctest --test-dir build/macos-debug -R '^unit.sdk_asset_equivalence$' --output-on-failure
```

Use the corresponding Linux target and build directory on Linux. This test validates
asset representation equivalence; native rendering parity and physical display behavior
require their separate checks.

## Installation

Generate the public ID header before compilation and declare only binary runtime outputs:

```make
APP_NAME := mygame
ASSET_BUILD := $(CURDIR)/../../build/apps/$(APP_NAME)/generated
ASSET_HEADER := include/mygame.h
TABOS_RUNTIME_ASSETS := $(ASSET_BUILD)/mygame.tsp $(ASSET_BUILD)/level.tmap
TABOS_BUILD_PREREQUISITES := $(ASSET_HEADER)

.PHONY: mygame-assets
mygame-assets: assets/manifest.json assets/level.tmj assets/tiles.tsj assets/tiles.png
	python3 ../../tools/tabos assets build assets/manifest.json \
	    --output "$(ASSET_BUILD)" --header-output "$(ASSET_HEADER)"

$(ASSET_HEADER) $(TABOS_RUNTIME_ASSETS): mygame-assets
	@test -f $@

include ../../sdk/make/application.mk
```

Normal installation and `./apps/build.sh --msc` copy those files to
`T:/data/<app-name>/`; PNG, GIF, Tiled, and manifest sources stay out of runtime media.
