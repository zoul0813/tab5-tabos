# Author Assets in Tiled

TabOS treats the Tiled JSON files and source images as the editable source of the game
world. This page creates a conventional `mygame` asset set.

## Create the Application Layout

Create these directories inside the TabOS repository:

```text
apps/mygame/
├── Makefile
├── assets/
│   ├── manifest.json
│   ├── tiles.png
│   ├── tiles.tsj
│   └── level.tmj
├── include/
│   └── mygame.h       generated later
└── src/
    └── main.c
```

Use `apps/tile-demo` as a working example when a field or Tiled setting is unclear.

## Prepare the Sprite Sheet

Create `assets/tiles.png` as an RGBA PNG arranged on a regular grid. A 16x16 tileset can
contain terrain tiles, character frames, pickups, and other images in the same sheet.

TabOS accepts only fully transparent or fully opaque source pixels. Every alpha value must
be either 0 or 255. Paint shadows with an opaque color shade instead of partial alpha.
This avoids color fringes when the source becomes RGB565.

## Create the Tileset

In Tiled:

1. Create a new tileset using `tiles.png`.
2. Select **Based on Tileset Image**.
3. Enter the correct tile width, tile height, margin, and spacing.
4. Save the tileset as the external JSON file `assets/tiles.tsj`.

Select an important tile and add these custom properties as needed:

| Property | Tiled type | Meaning |
| --- | --- | --- |
| `name` | string | Generates a sprite macro, such as `MYGAME_SPRITE_PLAYER`. |
| `pivot_x` | int | Horizontal pivot measured from the tile's top-left. |
| `pivot_y` | int | Vertical pivot measured from the tile's top-left. |
| `solid` | bool or int | Sets the flag registered as `solid` in the manifest. |
| `water` | bool or int | Sets the flag registered as `water` in the manifest. |

Property names are case-sensitive. Give every sprite that game code references a unique
`name`. Tiles used only as anonymous map scenery do not require names.

For a 16x16 player, a pivot of `pivot_x = 8` and `pivot_y = 16` places the player's feet
at its draw position. Terrain tiles ignore sprite pivots when drawn as map cells because
they must fill their cells exactly.

## Add a Tiled Animation

Select the tile that will trigger the animation. In Tiled's Tile Animation editor, add
the frame tiles in order and enter each duration in milliseconds. On the same tile add:

- `name` as a string, such as `player_0`;
- `animation_name` as a string, such as `player_walk`; and
- `repeat_count` as an integer when the animation should be finite.

Omit `repeat_count`, or set it to `0`, for a looping animation. Set it to `1` for a
one-shot. The generated names will resemble:

```c
MYGAME_SPRITE_PLAYER_0
MYGAME_ANIMATION_PLAYER_WALK
```

Define walk, run, jump, and other clips on separate animated tiles when the game needs to
select them independently.

## Create the Map

In Tiled:

1. Create a **finite orthogonal** map.
2. Choose the tile width and height used by the tileset.
3. Add `tiles.tsj` as an external tileset.
4. Create a tile layer named `ground`.
5. Paint the ground.
6. Create a tile layer named `foreground` for content drawn over actors.
7. Create an object layer named `markers`.
8. Save the map as JSON at `assets/level.tmj`.

Layer order is preserved. TabOS imports visible and hidden supported layers alike; the
application chooses which tile layers to draw and in what order. Tile-layer dimensions
must match the map, and tile-layer offsets must be zero.

## Add Object Markers

On the `markers` object layer, create and name objects such as:

- a point named `spawn`;
- a rectangle named `damage_zone`; and
- a tile object named `gem`.

Give `damage_zone` an integer property named `damage`. Give objects a Tiled class when the
game needs a category shared by several markers.

Dragging an object can produce fractional coordinates. The converter rounds them to the
nearest logical pixel and prints a warning containing the old and new values. Tiled tile
objects use alignment anchors; the converter normalizes every supported alignment to the
top-left object geometry exposed by TabOS.

Object layers are data. A point, rectangle, or tile object appears only when the game
chooses to draw something for it.

## Create the Manifest

Save this as `assets/manifest.json`:

```json
{
  "version": 1,
  "name": "mygame",
  "flags": {
    "solid": 1,
    "water": 2
  },
  "metasprites": [
    {
      "name": "player_shadow",
      "parts": [
        {"sprite": "shadow", "x": 2, "y": 0, "opacity": 128},
        {"sprite": "player_0", "x": 0, "y": 0}
      ]
    }
  ],
  "maps": [
    {"name": "level", "source": "level.tmj"}
  ]
}
```

Remove the example metasprite until the Tiled tiles named `shadow` and `player_0` exist.
Flag values must be unique, nonzero bits. Use `1`, `2`, `4`, `8`, and so on.

The map imports its referenced tileset image, sprites, and animations. Do not also list
the same `tiles.png` under a manifest `images` array. The manifest remains the home for
the asset-set name, flag registry, metasprites, standalone PNG/GIF sources, and map list.

The complete field rules are in the manual's [Authoring section](../../tile-assets.md#authoring).

