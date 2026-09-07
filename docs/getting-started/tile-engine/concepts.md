# Core Tile Engine Concepts

This page explains the terms used by the TabOS Tile Engine. Read it before creating the
first map.

## Pixels, Tiles, and Maps

A **pixel** is one color on the logical TabOS canvas. Game positions and camera positions
are measured in logical pixels.

A **tile** is a reusable rectangular image, commonly 8x8, 16x16, or 32x32 pixels. A
**tilemap** is a grid whose cells select tiles. A 20x12 map made from 16x16 tiles is
320x192 world pixels.

TabOS supports finite, orthogonal Tiled maps. The map begins at world `(0, 0)`. Its
positive X direction is right and its positive Y direction is down.

## Images and Sprites

A **sprite image** stores RGB565 pixels imported from a PNG or GIF. A **sprite** is a
rectangular region of one image plus these descriptors:

- width and height;
- a pivot;
- application-defined flags; and
- the source image and source rectangle.

The generated sprite ID identifies that descriptor. Game code should use a generated
name such as `MYGAME_SPRITE_PLAYER`, never a handwritten number.

The position passed to `tabos_sprite_draw()` is the sprite's **pivot position**. A pivot
of `(0, 0)` places the sprite's top-left corner at the draw position. A 16x16 character
with pivot `(8, 16)` uses its bottom center as the position. That is often convenient for
placing a character's feet in the world.

See [Draw a Sprite](../../tile-assets.md#draw-a-sprite) for scaling, mirrors, rotation,
opacity, and clipping.

## Animations

An **animation** is an ordered list of sprite IDs and frame durations. TabOS animations
are stateless. The application stores when an actor started its current animation and
passes elapsed milliseconds on every draw:

```c
uint64_t elapsed_ms = now_ms - actor.animation_started_ms;
tabos_sprite_animation_draw(
    &graphics, &sprites, MYGAME_ANIMATION_PLAYER_WALK,
    actor.x, actor.y, elapsed_ms);
```

A repeat count of zero loops forever. A positive repeat count plays that many cycles and
then holds the final frame. `tabos_sprite_animation_finished()` tells the game when a
finite animation has completed. The game decides whether to switch from jump to idle,
remove an effect, or take another action.

## Metasprites

A **metasprite** draws several sprites as one ordered object. Typical uses include a
character over a shadow, equipment attached to an actor, or a vehicle made from several
parts. Each part has a signed offset, transform, and opacity. Later parts draw over earlier
parts.

Metasprites are defined in `manifest.json`, because Tiled does not define this TabOS
composition. See [Draw a Metasprite](../../tile-assets.md#draw-a-metasprite).

## Tile Layers and Object Layers

A **tile layer** is a grid of tile cells. TabOS draws one tile layer per call. Drawing
layers separately lets the game place actors between them:

```text
draw ground tile layer
draw actors
draw foreground tile layer
draw screen-space HUD
```

An **object layer** contains authored markers. Supported objects are points, rectangles,
and tile objects. TabOS loads them but never draws them automatically. The application
can interpret them as spawn points, collision zones, pickups, doors, enemy locations, or
debug outlines.

Each object keeps its stable Tiled ID, name, class/type, top-left world geometry, shape,
and integer properties. A tile object also carries an encoded tile value.

## Flags and Properties

A **sprite flag** is one bit chosen by the game, such as `solid`, `water`, or `damage`.
Register its bit value in the manifest, then set a matching Boolean or integer property on
a Tiled tile. The converter generates constants such as `MYGAME_FLAG_SOLID`.

TabOS preserves the bits but assigns no behavior to them. For example, the application
can call `tabos_sprite_flags()` and decide that any tile with the `solid` flag blocks the
player.

Object properties are named, signed 32-bit integers. The application reads them with
`tabos_tilemap_object_property()`. Other property types are not part of the runtime object
API.

## Generated Names

The asset converter turns authored names into C macros:

```c
MYGAME_SPRITE_PLAYER
MYGAME_ANIMATION_PLAYER_WALK
MYGAME_METASPRITE_PLAYER_SHADOW
MYGAME_LAYER_WORLD_GROUND
MYGAME_OBJECT_WORLD_SPAWN
MYGAME_FLAG_SOLID
```

The prefix comes from the manifest's `name`. Sprite and animation names come from Tiled
tile properties. Layer and object names come from the Tiled map. Metasprite and flag names
come from the manifest.

## World and Screen Coordinates

Map cells, sprites, metasprites, and objects use world coordinates. Start a world pass
with one camera:

```c
tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
/* Draw world content. */
tabos_graphics_end_camera(&graphics);
/* Draw screen-space HUD content. */
```

Do not subtract the camera from individual sprite or object positions. The graphics
camera performs that conversion. A tilemap viewport and sprite clip are screen-space
rectangles.

Tiled tile-object anchors are normalized during conversion. At runtime, every object
reports the top-left corner of its bounding box in `x` and `y`, regardless of the
tileset's Tiled object alignment.

See the [graphics camera contract](../../graphics-api.md) and the
[tilemap runtime API](../../tile-assets.md#runtime-api) for exact behavior.

