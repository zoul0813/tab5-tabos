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

`<tabos/tilemap.h>` uses 32-bit `tabos_tile_t`. Zero is empty; low 28 bits hold a
one-based sprite ID; high bits preserve Tiled horizontal, vertical, and diagonal flips.
Use `TABOS_TILE()` and `TABOS_TILE_ID()` with normal zero-based IDs. Maps retain ordered
tile/object layers. Cells are writable in memory. Applications draw layers individually
and own collision/gameplay. Object layers expose point, rectangle, and tile markers plus
named integer properties; they never draw automatically.

Load `.tsp` and `.tmap` files with `tabos_sprite_set_load()` and
`tabos_tilemap_load()`. Matching unload functions free owned data and zero the object.

## Authoring

Run:

```sh
./tools/tabos assets build path/to/manifest.json --output build/assets
```

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
- Manifest flag names as boolean/integer tile properties: sprite flag bits.
- Standard tileset `transparentcolor`, plus optional integer tileset property
  `transparent_tolerance` from 0 through 255: color-key preparation.

Keep manifest content for orchestration and metadata Tiled cannot represent, such as
asset-set name, flag-bit registry, standalone PNG/GIF imports, and metasprite composition.

Output contains deterministic generated `.c`/`.h`, one `TSP1` `.tsp`, and a `TMP1`
`.tmap` per map. Binary tables are little-endian, four-byte aligned, offset/count based,
and uncompressed.

## Installation

Declare only runtime outputs before including `application.mk`:

```make
TABOS_RUNTIME_ASSETS := $(CURDIR)/assets/build/game.tsp \
                        $(CURDIR)/assets/build/level.tmap
```

Normal installation and `./apps/build.sh --msc` copy those files to
`T:/data/<app-name>/`; PNG, GIF, Tiled, and manifest sources stay out of runtime media.
