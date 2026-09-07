# Work with Tiled

Tiled is the visual authoring tool for TabOS tile worlds. It stores the map in a `.tmj`
file and an external tileset in a `.tsj` file. The TabOS converter reads those JSON files;
the application does not load them at runtime.

This page uses Tiled's standard editor terms. Menu placement can vary slightly between
Tiled releases, but the workflow and saved data are the same.

## Understand the Source Files

A typical source asset set contains:

```text
assets/
├── manifest.json   TabOS asset-set configuration
├── tiles.png       RGBA source image
├── tiles.tsj       Tiled tileset and per-tile metadata
└── level.tmj       Tiled map, layers, cells, and objects
```

The references flow in one direction:

```text
manifest.json -> level.tmj -> tiles.tsj -> tiles.png
```

Keep these files together or preserve their relative paths when moving them. Commit all
four. The generated `.tsp` and `.tmap` files belong in the build directory.

## Create an External Tileset

In Tiled:

1. Choose **File > New > New Tileset**.
2. Select **Based on Tileset Image**.
3. Browse to the RGBA sprite sheet, such as `tiles.png`.
4. Enter the tile width and height used by its grid.
5. Enter the image margin and spacing, normally zero.
6. Save as a JSON tileset, such as `tiles.tsj`.

Use an external tileset instead of embedding it in one map when several maps share the
same art and tile metadata.

The source image may contain unnamed scenery and named game sprites together. The
converter creates a sprite descriptor for every atlas tile. Add meaningful names where
game code needs a stable, readable generated constant.

## Name a Tile for Game Code

Select a tile in the tileset editor. In its **Properties** panel, add a custom string
property:

```text
name = player
```

After conversion, a manifest named `mygame` produces:

```c
MYGAME_SPRITE_PLAYER
```

Names should describe game meaning instead of sheet position. Prefer `player`, `wall`,
and `blue_gem` over `tile_17`. Renaming an authored item intentionally changes its C
macro, allowing compiler errors to show every place game code must be updated.

The generated C name is uppercase and sanitized. Names that sanitize to the same C
identifier are rejected instead of silently colliding.

## Set a Sprite Pivot

Add integer properties to a named tile:

```text
pivot_x = 8
pivot_y = 16
```

For a 16x16 character, this places the pivot at the bottom center. Calling:

```c
tabos_sprite_draw(&graphics, &sprites, MYGAME_SPRITE_PLAYER, 100, 80);
```

places the character's feet at world `(100, 80)`.

Use `(0, 0)` for a top-left origin. Pivots can lie on an edge or outside a sprite, which
is useful for attachment points. Tilemap cells ignore sprite pivots and always fill their
map cell.

## Mark Tiles with Game Flags

First register each flag bit in `manifest.json`:

```json
{
  "version": 1,
  "name": "mygame",
  "flags": {
    "solid": 1,
    "water": 2,
    "damage": 4
  },
  "maps": [
    {"name": "level", "source": "level.tmj"}
  ]
}
```

Then select a tile in Tiled and add a Boolean property whose name matches the manifest:

```text
solid = true
```

An integer property with the same name also works: zero clears the flag and a nonzero
value sets it. Boolean properties are clearest for simple categories.

The converter produces `MYGAME_FLAG_SOLID`. The application decides what the bit means:

```c
uint32_t flags = tabos_sprite_flags(&sprites, sprite_id);
if ((flags & MYGAME_FLAG_SOLID) != 0U) {
    /* Reject movement into this tile. */
}
```

## Create a Tile Animation

Open the tileset editor and select the tile that represents the animation. In the **Tile
Animation** panel:

1. Add frame tiles in playback order.
2. Set each frame duration in milliseconds.
3. Add a string property named `animation_name` to the selected trigger tile.
4. Give the trigger tile a `name` when game code also needs its sprite ID.
5. Add an integer `repeat_count` only for finite playback.

Example properties:

```text
name = player_walk_0
animation_name = player_walk
repeat_count = 0
```

This creates:

```c
MYGAME_SPRITE_PLAYER_WALK_0
MYGAME_ANIMATION_PLAYER_WALK
```

`repeat_count = 0` loops forever. `repeat_count = 1` plays once and holds the final frame.
Every frame duration must be greater than zero. TabOS preserves Tiled animation durations
exactly.

For separate walk, run, idle, and jump clips, create a separate animated trigger tile for
each clip and give each one a unique `animation_name`.

## Create the Map and Layers

Choose **File > New > New Map**, then configure:

- orientation: **Orthogonal**;
- map size: finite width and height; and
- tile size: the same logical cell size used by the tileset.

Save the map as JSON, such as `level.tmj`. Add `tiles.tsj` through the **Tilesets** panel.

Create layers in visual order. A common structure is:

```text
foreground    tile layer: roofs, branches, and objects above actors
markers       object layer: spawns, triggers, and pickups
ground        tile layer: floor, water, walls, and scenery
```

Tiled may display the top layer first in its layer panel. TabOS preserves the authored
layer order, but the application still chooses each tile layer's draw order:

```c
tabos_tilemap_draw_layer(
    &graphics, &map, MYGAME_LAYER_LEVEL_GROUND, &sprites, &draw);
draw_actors();
tabos_tilemap_draw_layer(
    &graphics, &map, MYGAME_LAYER_LEVEL_FOREGROUND, &sprites, &draw);
```

Layer names generate constants. Keep them unique within the map. Do not use tile-layer
offsets; supported tile layers begin at the map origin and match the map dimensions.

## Paint and Transform Tiles

Paint map cells with Tiled's stamp, bucket, terrain, and selection tools. Horizontal,
vertical, and diagonal tile flips are stored in the cell GID and preserved by TabOS.
Animated tiles automatically use their current frame when
`tabos_tilemap_draw_options_t.animation_ms` advances.

The map is finite. Its world pixel dimensions are:

```text
map width in cells  * tile width in pixels
map height in cells * tile height in pixels
```

Changing the Tiled map dimensions does not change the application's logical graphics
canvas. Choose both sizes deliberately.

## Add a Spawn Point

Select the `markers` object layer, choose Tiled's point tool, click the desired position,
and name the object `spawn`.

The generated macro contains both map and object names:

```c
MYGAME_OBJECT_LEVEL_SPAWN
```

Game code reads it from the generated layer:

```c
const tabos_tilemap_object_t* spawn = tabos_tilemap_object(
    &map,
    MYGAME_LAYER_LEVEL_MARKERS,
    MYGAME_OBJECT_LEVEL_SPAWN);
```

The macro value is the stable Tiled object ID. It is not the object's array position.

## Add a Rectangle Trigger

On the object layer, draw a rectangle named `lava_zone`. Add:

```text
Class: hazard
damage: 10   (int)
```

At runtime, the object provides top-left `x` and `y`, `width`, `height`, `name`, and
`type`. `type` contains the Tiled class, or the legacy type when class is absent.

Read its property with:

```c
int32_t damage = 0;
if (tabos_tilemap_object_property(lava_zone, "damage", &damage) == 0) {
    /* Apply damage after the game's overlap test. */
}
```

Only signed integer object properties enter the runtime package.

## Add a Tile Object

Choose the tile-object tool, select a named tile such as `gem`, and place it on the object
layer. Name the object `gem_pickup`.

Tiled positions tile objects by the tileset's object alignment. TabOS normalizes that
alignment during conversion. At runtime, `object->x` and `object->y` always mean the
top-left of the object's bounding box.

The object will not draw itself. The game may use its rectangle as a pickup trigger and
draw its encoded sprite after collision:

```c
uint32_t gem_sprite = TABOS_TILE_ID(gem_pickup->tile);
const tabos_sprite_t* descriptor = &sprites.sprites[gem_sprite];

tabos_sprite_draw(
    &graphics,
    &sprites,
    gem_sprite,
    gem_pickup->x + descriptor->pivot_x,
    gem_pickup->y + descriptor->pivot_y);
```

Adding the pivot converts the object's top-left position to the pivot position expected
by `tabos_sprite_draw()`.

## Use Classes and Repeated Objects

Generated constants are convenient for unique named objects. Iterate an object layer for
repeated objects such as enemy spawns, doors, or harvest points. Give them a shared Tiled
class, then compare `object->type` in game code.

Names, classes, and integer properties serve different purposes:

| Tiled field | Good use |
| --- | --- |
| Name | One specific object, such as `boss_door`. |
| Class | A category, such as `enemy_spawn`. |
| Integer property | Per-object data, such as `enemy_kind = 3` or `count = 4`. |

## Rebuild After Editing

The everyday edit loop is:

1. Edit `tiles.png`, `tiles.tsj`, or `level.tmj`.
2. Save in Tiled.
3. Run `make -C apps/mygame`.
4. Review converter warnings, especially coordinate rounding.
5. Run the application and verify the authored result.

The Makefile tracks the manifest, map, tileset, and image as prerequisites. Saving any of
them causes the asset converter to regenerate the binaries and public ID header.

If an object is dragged to `200.5`, the converter may report that it rounded the value to
`201`. This warning informs the developer about the pixel adjustment; it does not reject
normal visual placement.

## Study the `tdemo` Tiled Sources

The complete example pairs each source file with its result:

| Source | What to inspect |
| --- | --- |
| [`world.tmj`](../../../apps/tile-demo/assets/world.tmj) | Painted layers, transforms, spawn/grove/gem objects, and the `trees` property. |
| [`demo.tsj`](../../../apps/tile-demo/assets/demo.tsj) | Tile names, pivots, solid/water flags, and robot animation. |
| [`manifest.json`](../../../apps/tile-demo/assets/manifest.json) | Flag-bit registry, `tree_shadow` metasprite, and map entry. |
| [`tdemo.h`](../../../apps/tile-demo/include/tdemo.h) | Generated macros resulting from authored names. |
| [`main.c`](../../../apps/tile-demo/src/main.c) | Runtime use of layers, objects, properties, flags, animation, and cells. |

Open `world.tmj` in Tiled. Select the three object markers and named tiles, then compare
their Properties panels with `tdemo.h` and `main.c`. This is the fastest way to see the
full author-to-code path.

For every supported JSON field and converter rule, use the manual's complete
[Authoring reference](../../tile-assets.md#authoring).
