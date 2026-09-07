# Draw the Game

Once the graphics context, sprite set, and map are loaded, a frame has four steps: clear,
draw the world, draw screen-space content, and present.

## Draw a Map Layer

The simplest map draw uses the default options:

```c
tabos_tilemap_draw_options_t map_draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;

if (tabos_tilemap_draw_layer(
        &graphics,
        &map,
        MYGAME_LAYER_LEVEL_GROUND,
        &sprites,
        &map_draw) != 0) {
    /* Handle the draw error. */
}
```

The default viewport is the full logical graphics canvas. Animation time is zero. The
layer constant is an index into this loaded map's ordered layers.

## Draw Layers Around an Actor

Set one shared camera for all world content. Draw an actor after the ground and before the
foreground:

```c
uint64_t elapsed_ms = tabos_monotonic_ms() - started_ms;
tabos_tilemap_draw_options_t map_draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
map_draw.animation_ms = elapsed_ms;

tabos_graphics_clear(&graphics, TABOS_RGB565(8, 18, 30));
tabos_graphics_begin_camera(&graphics, camera_x, camera_y);

tabos_tilemap_draw_layer(
    &graphics, &map, MYGAME_LAYER_LEVEL_GROUND, &sprites, &map_draw);

tabos_sprite_animation_draw(
    &graphics, &sprites, MYGAME_ANIMATION_PLAYER_WALK,
    player_x, player_y, elapsed_ms);

tabos_tilemap_draw_layer(
    &graphics, &map, MYGAME_LAYER_LEVEL_FOREGROUND, &sprites, &map_draw);

tabos_graphics_end_camera(&graphics);

/* Draw HUD elements here. They use screen coordinates. */
tabos_graphics_present(&graphics);
```

Do not subtract `camera_x` or `camera_y` from the player or object positions. Map layers,
sprites, metasprites, and graphics primitives all use the active camera.

`animation_ms` advances every animated tile in the map layer. The elapsed time passed to
`tabos_sprite_animation_draw()` advances the actor clip. These are absolute elapsed times
from a chosen start, not frame deltas.

## Draw a Static Sprite

The basic call draws at natural size, full opacity, with no transform or clip:

```c
tabos_sprite_draw(
    &graphics, &sprites, MYGAME_SPRITE_CRATE, crate_x, crate_y);
```

The coordinate places the sprite pivot. Use extended options when a draw needs a change:

```c
tabos_sprite_draw_options_t sprite_draw = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
sprite_draw.mirror_x = facing_left;
sprite_draw.opacity = 192U;

tabos_sprite_draw_ex(
    &graphics, &sprites, MYGAME_SPRITE_PLAYER_0,
    player_x, player_y, &sprite_draw);
```

Always begin visible extended draws with `TABOS_SPRITE_DRAW_OPTIONS_DEFAULT`. A plain `{0}`
sets opacity to zero and therefore draws nothing. A zero width or height in the named
default means natural size.

## Switch and Finish Animations

The game stores an animation ID and start time for each actor:

```c
typedef struct {
    int32_t x;
    int32_t y;
    uint32_t animation;
    uint64_t animation_started_ms;
} actor_t;
```

Switch clips by changing the ID and resetting the start time:

```c
player.animation = MYGAME_ANIMATION_PLAYER_JUMP;
player.animation_started_ms = now_ms;
```

Test a one-shot and choose the follow-up in game code:

```c
uint64_t elapsed_ms = now_ms - player.animation_started_ms;
bool finished = false;

if (tabos_sprite_animation_finished(
        &sprites, player.animation, elapsed_ms, &finished) != 0) {
    /* Handle invalid animation data. */
}

if (finished) {
    player.animation = MYGAME_ANIMATION_PLAYER_IDLE;
    player.animation_started_ms = now_ms;
    elapsed_ms = 0;
}

tabos_sprite_animation_draw(
    &graphics, &sprites, player.animation,
    player.x, player.y, elapsed_ms);
```

Looping animations never report finished. Finite animations hold their final frame after
completion until the game changes the clip.

## Clip a World View

To draw a map only inside part of the screen, replace the default viewport:

```c
tabos_tilemap_draw_options_t map_draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
map_draw.viewport = (tabos_graphics_rect_t) {
    .x = 8,
    .y = 8,
    .width = 304U,
    .height = 164U,
};
```

The viewport clips in screen coordinates. It does not move the map. An explicit zero-width
or zero-height viewport draws nothing.

See the manual's [Runtime API](../../tile-assets.md#runtime-api) and
[Game Recipes](../../tile-assets.md#game-recipes) for complete draw contracts.

