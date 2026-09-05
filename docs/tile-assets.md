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

Pause by freezing elapsed time; restart by resetting the actor's start time. The existing
frame-selection function remains available when the game needs a sprite ID without drawing.

`<tabos/tilemap.h>` uses 32-bit `tabos_tile_t`. Zero is empty; low 28 bits hold a
one-based sprite ID; high bits preserve Tiled horizontal, vertical, and diagonal flips.
Use `TABOS_TILE()` and `TABOS_TILE_ID()` with normal zero-based IDs. Maps retain ordered
tile/object layers. Cells are writable in memory. Applications draw layers individually
and own collision/gameplay. Object layers expose point, rectangle, and tile markers plus
named integer properties; they never draw automatically.

Set the shared graphics camera once for the world pass. All sprite variants, metasprites,
map layers, and primitive object markers then use world coordinates without manual subtraction:

```c
const tabos_tilemap_draw_options_t draw = {
    .viewport = {.width = graphics.width, .height = graphics.height},
    .animation_ms = elapsed_ms,
};
tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
tabos_tilemap_draw_layer(&graphics, &map, GROUND_LAYER, &sprites, &draw);
tabos_sprite_animation_draw(&graphics, &sprites, player.animation, player.x, player.y, elapsed_ms);
tabos_tilemap_draw_layer(&graphics, &map, FRONT_LAYER, &sprites, &draw);
tabos_graphics_end_camera(&graphics);
/* Draw HUD in screen coordinates, then present. */
```

`viewport` is a screen-space clip, intersected with canvas bounds before visible-cell
selection. Empty viewports draw nothing. Maps remain anchored at world `(0, 0)`; sprites
and maps share the same projection. See [graphics camera semantics](graphics-api.md).

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

## Authoring

Run:

```sh
./tools/tabos assets build path/to/manifest.json --output build/assets
```

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

Version-1 JSON manifests contain `name`, optional numeric `flags`, and arrays named
`images`, `animations`, `metasprites`, and `maps`. Example:

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

GIF frames honor transparency, disposal, delay, and loop metadata. Delays below 10 ms
become 10 ms unless `durations_ms` overrides them. Alpha must be exactly 0 or 255.
Transparency gets a deterministic unused RGB565 key; explicit `color_key` must not
collide with opaque pixels. `resize` performs nearest-neighbor import resizing.
`transparent_rgb` and optional `transparent_tolerance` key deliberately opaque pixel
art before strict alpha validation.

Maps may be finite orthogonal Tiled JSON with multiple atlas tilesets, tile/object
layers, transforms, animations, integer/boolean tile properties, and integer object
properties. Only integral point, rectangle, and tile objects with zero rotation are
accepted. Unsupported map modes, object geometry, partial alpha, unknown GIDs, and the
reserved GID bit fail with diagnostics.

When a manifest imports a Tiled map, its TMJ/TSJ files are authoritative for atlas
regions and tile animations; do not list the same tileset image under manifest `images`.
The converter loads maps before manifest animations and metasprites, allowing those
entries to reference sprites named in TSJ tile properties. Supported TSJ metadata is:

- `name` string tile property: generated sprite name.
- `pivot_x` and `pivot_y` integer tile properties: local source-pixel anchor. Sprite
  drawing places this anchor at the requested destination coordinate; `(0, 0)` anchors
  the top-left, while `(width / 2, height - 1)` anchors near the bottom-center.
- `animation_name` string tile property: name of that tile's Tiled animation.
- `repeat_count` integer property on an animated tile: `0` loops forever (default),
  `1` plays once, and larger values play that many complete cycles. Values must fit an
  unsigned 32-bit integer; wrong types, negative values, and use on a tile without an
  animation are rejected. This TabOS property survives both generated C and binary output
  and applies to animated map tiles as well as explicitly drawn sprites. Define each named
  walk/run/jump clip on its own animated tile.
- Manifest flag names as boolean/integer tile properties: sprite flag bits.
- Standard tileset `transparentcolor`, plus optional integer tileset property
  `transparent_tolerance` from 0 through 255: color-key preparation.

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

Declare only runtime outputs before including `application.mk`:

```make
TABOS_RUNTIME_ASSETS := $(CURDIR)/assets/build/game.tsp \
                        $(CURDIR)/assets/build/level.tmap
```

Normal installation and `./apps/build.sh --msc` copy those files to
`T:/data/<app-name>/`; PNG, GIF, Tiled, and manifest sources stay out of runtime media.
