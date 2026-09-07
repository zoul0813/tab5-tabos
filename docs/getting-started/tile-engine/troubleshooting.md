# Tile Engine Troubleshooting

## A Generated Name Is Missing

Check where that name originates:

| Expected macro | Required authored name |
| --- | --- |
| `MYGAME_SPRITE_*` | String `name` property on a Tiled tile, or named manifest sprite. |
| `MYGAME_ANIMATION_*` | String `animation_name` property on an animated Tiled tile, or named manifest animation. |
| `MYGAME_METASPRITE_*` | Metasprite `name` in `manifest.json`. |
| `MYGAME_LAYER_*` | Map and layer names in Tiled. |
| `MYGAME_OBJECT_*` | Map and object names in Tiled. |
| `MYGAME_FLAG_*` | Flag name in `manifest.json`. |

Regenerate the assets and inspect the committed constants-only header in the application's
`include` directory. Do not add a handwritten replacement macro to `main.c`.

## The Converter Reports Rounded Object Coordinates

Tiled permits fractional object positions and sizes; TabOS world geometry uses integer
logical pixels. The converter rounds each fractional field to the nearest pixel and emits
a warning. This is expected after dragging objects freely. Review the reported movement
when exact placement matters.

## A Tile Object Is Vertically Shifted

Rebuild the asset after editing it. The converter reads the Tiled tileset's
`objectalignment`, converts that anchor to the top-left runtime convention, then rounds
the normalized result. Do not compensate by subtracting the tile height in game code.

## Object Markers Do Not Show Their Artwork

Object layers are never rendered automatically. The object is authored data. Look it up
or iterate its layer, then choose what the game should draw. A tile object exposes an
encoded tile through `object->tile`; its sprite ID is `TABOS_TILE_ID(object->tile)`.

## A Map Looks Small or Has Empty Space

Check all three sizes:

- logical canvas size: `graphics.width` by `graphics.height`;
- map grid size: `map.width` by `map.height`; and
- map pixel size: `map.width * map.tile_width` by `map.height * map.tile_height`.

The camera changes which world pixels are visible. It does not scale the map to fill the
canvas. A 27x15 map of 16x16 tiles is 432x240 pixels. Empty space is expected when the
camera shows world coordinates outside the finite map.

## Sprites Move Differently from the Map

Use one `tabos_graphics_begin_camera()` call around the complete world pass. Pass world
positions directly to sprites, metasprites, object debug shapes, and map drawing. Remove
manual camera subtraction from individual draw positions.

## An Extended Sprite Draw Is Invisible

Initialize options with:

```c
tabos_sprite_draw_options_t draw = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
```

A `{0}` initializer sets opacity to zero. The named default uses natural size, full
opacity, no transforms, and no clipping.

## Transparent Edges Show Magenta or Noise

Use an RGBA PNG whose alpha values are only 0 or 255. Do not place translucent antialiasing
or shadows over a magenta background. Paint opaque shadow colors explicitly and make the
background fully transparent. TabOS uses exact RGB565 color-key transparency at runtime;
general partial alpha is unsupported for imported sprite assets.

## An Animation Does Not Advance

Pass elapsed milliseconds since the clip started. Do not pass the current frame's delta
or always pass zero. For animated map tiles, also set
`tabos_tilemap_draw_options_t.animation_ms` before drawing the layer.

## A One-Shot Animation Never Finishes

Set the animated tile's integer `repeat_count` property to a positive value, normally `1`.
A missing or zero repeat count loops forever. Call `tabos_sprite_animation_finished()`
with the same elapsed time used for drawing, then let the game choose the next animation.

## A Tile Does Not Block Movement

Confirm that:

1. the flag exists as a unique bit in `manifest.json`;
2. the Tiled tile has a matching Boolean true or nonzero integer property;
3. the generated flag macro is used in the collision mask;
4. the cell is read from the intended tile layer; and
5. transform bits are removed with `TABOS_TILE_ID()` before calling `tabos_sprite_flags()`.

The Tile Engine stores flags but does not apply collision automatically.

## A Runtime Asset Does Not Load

Confirm that the Makefile lists the `.tsp` and every `.tmap` under
`TABOS_RUNTIME_ASSETS`, and that the game path matches `T:/data/<app-name>/`. Print `errno`
on failure. Rebuild source and runtime together; the application ABI and asset formats are
still pre-release.

For converter field rules and runtime errors, consult the complete
[Sprite and Tile Assets manual](../../tile-assets.md).

