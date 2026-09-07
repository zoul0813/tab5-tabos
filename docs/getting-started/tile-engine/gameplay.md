# Add Gameplay to a Tile World

TabOS exposes authored tile and object data. The application turns that data into game
rules. This page shows common patterns.

## Block Movement with Tile Flags

Convert a nonnegative world point to a map cell, read its tile, and inspect that sprite's
flags:

```c
static bool point_blocks(
    const tabos_tilemap_t* map,
    const tabos_sprite_set_t* sprites,
    uint32_t layer,
    int32_t x,
    int32_t y)
{
    if (x < 0 || y < 0 ||
        (uint64_t) (uint32_t) x >= (uint64_t) map->width * map->tile_width ||
        (uint64_t) (uint32_t) y >= (uint64_t) map->height * map->tile_height) {
        return true;
    }

    tabos_tile_t tile = TABOS_TILE_EMPTY;
    if (tabos_tilemap_get(
            map, layer,
            (uint32_t) x / map->tile_width,
            (uint32_t) y / map->tile_height,
            &tile) != 0) {
        return true;
    }

    if (tile == TABOS_TILE_EMPTY) {
        return false;
    }

    uint32_t flags = tabos_sprite_flags(sprites, TABOS_TILE_ID(tile));
    return (flags & (MYGAME_FLAG_SOLID | MYGAME_FLAG_WATER)) != 0U;
}
```

This example treats points outside the map as blocked. A real character usually checks
multiple points around its collision bounds. The Tile Engine does not choose the actor
shape, collision response, or meaning of a flag.

`TABOS_TILE_ID(tile)` removes Tiled transform bits before looking up the sprite descriptor.
Never call it on an empty tile unless the resulting `TABOS_SPRITE_NONE` is expected.

## Find Named Objects

Generated object constants are stable Tiled object IDs. Find a named spawn marker inside
its object layer:

```c
const tabos_tilemap_object_t* spawn = tabos_tilemap_object(
    &map,
    MYGAME_LAYER_LEVEL_MARKERS,
    MYGAME_OBJECT_LEVEL_SPAWN);

if (spawn == NULL) {
    /* errno is EINVAL for a wrong layer or ENOENT for a missing object. */
} else {
    player_x = spawn->x;
    player_y = spawn->y;
}
```

An object constant is an object ID, not an array index. A layer constant is an index in
the loaded map.

## Iterate an Object Layer

Use iteration when several objects share a class or should all create game entities:

```c
const tabos_tilemap_layer_t* markers =
    &map.layers[MYGAME_LAYER_LEVEL_MARKERS];

if (markers->type == TABOS_TILEMAP_LAYER_OBJECTS) {
    for (uint32_t index = 0U; index < markers->object_count; ++index) {
        const tabos_tilemap_object_t* object = &markers->objects[index];

        if (object->shape == TABOS_TILEMAP_OBJECT_RECTANGLE) {
            /* Create or test a rectangular trigger at object->x/y/width/height. */
        }
    }
}
```

The runtime geometry always uses a top-left origin. Point objects may have zero width and
height. Tile objects provide `object->tile`; extract their sprite with
`TABOS_TILE_ID(object->tile)` and their transforms with `TABOS_TILE_TRANSFORMS(object->tile)`.

Object layers never draw automatically. Draw a debug rectangle, spawn an entity, or draw
the tile object's sprite according to the game's rules.

## Read an Object Property

Read a named signed integer without searching the property array yourself:

```c
int32_t damage = 0;

if (tabos_tilemap_object_property(zone, "damage", &damage) == 0) {
    player_health -= damage;
}
```

A missing property returns `-1` with `errno = ENOENT`. The output value stays unchanged
on failure.

## Draw a Metasprite

After defining `player_shadow` in the manifest, draw all of its ordered parts with one
call:

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

Overall mirror values reverse the part offsets and combine with per-part mirrors. Overall
opacity multiplies each part's opacity. A metasprite has no playback state; animate a
composite actor by selecting or drawing its parts according to game state.

## Change a Map Cell

Map cells loaded from `.tmap` are writable in memory:

```c
tabos_tile_t old_tile = TABOS_TILE_EMPTY;

if (tabos_tilemap_get(
        &map, MYGAME_LAYER_LEVEL_FOREGROUND,
        door_column, door_row, &old_tile) == 0) {
    tabos_tilemap_set(
        &map, MYGAME_LAYER_LEVEL_FOREGROUND,
        door_column, door_row,
        TABOS_TILE(MYGAME_SPRITE_OPEN_DOOR));
}
```

Use `TABOS_TILE_EMPTY` to erase a cell. Add transform bits with bitwise OR when needed:

```c
tabos_tile_t mirrored =
    TABOS_TILE(MYGAME_SPRITE_OPEN_DOOR) |
    TABOS_TILE_FLIP_HORIZONTAL;
```

Changes last until the map is unloaded. TabOS does not save them back to the `.tmap` file.
The [flags and cells recipe](../../tile-assets.md#read-flags-and-edit-cells) documents all
transform bits and errors.
