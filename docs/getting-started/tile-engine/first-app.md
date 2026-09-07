# Complete First Application

This page combines the earlier steps into a small, complete map viewer. It loads the
binary assets, draws two tile layers around one animated sprite, and exits when the user
presses Q or Escape.

The example expects the names created in [Author Assets in Tiled](authoring.md):

```c
MYGAME_ANIMATION_PLAYER_WALK
MYGAME_LAYER_LEVEL_GROUND
MYGAME_LAYER_LEVEL_FOREGROUND
```

If your authored names differ, use the corresponding macros from
`apps/mygame/include/mygame.h`.

## Create `src/main.c`

```c
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mygame.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/sprite.h>
#include <tabos/tilemap.h>
#include <tabos/tty.h>

enum {
    GAME_WIDTH = 320,
    GAME_HEIGHT = 180,
};

int main(void)
{
    uint32_t tty_mode = 0U;
    (void) ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty_mode);
    (void) ioctl(
        STDIN_FILENO,
        TABOS_TTY_SET_MODE,
        (tty_mode & ~(uint32_t) TABOS_TTY_MODE_SCROLL_KEYS) |
            (uint32_t) TABOS_TTY_MODE_RAW_INPUT);

    tabos_graphics_t graphics = {
        .width = GAME_WIDTH,
        .height = GAME_HEIGHT,
    };
    tabos_sprite_set_t sprites = {0};
    tabos_tilemap_t map = {0};

    if (tabos_graphics_open(&graphics) != 0 ||
        tabos_sprite_set_load("T:/data/mygame/mygame.tsp", &sprites) != 0 ||
        tabos_tilemap_load("T:/data/mygame/level.tmap", &map) != 0) {
        fprintf(stderr, "mygame: startup failed (errno %d)\n", errno);
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        if (graphics.open) {
            (void) tabos_graphics_close(&graphics);
        }
        return 1;
    }

    bool running = true;
    int32_t player_x = 160;
    int32_t player_y = 90;
    int32_t camera_x = 0;
    int32_t camera_y = 0;
    uint64_t started_ms = tabos_monotonic_ms();

    while (running) {
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type == TABOS_INPUT_KEY_DOWN &&
                (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE)) {
                running = false;
            }
        }

        uint64_t elapsed_ms = tabos_monotonic_ms() - started_ms;
        tabos_tilemap_draw_options_t map_draw =
            TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
        map_draw.animation_ms = elapsed_ms;

        (void) tabos_graphics_clear(
            &graphics, TABOS_RGB565(8, 18, 30));
        (void) tabos_graphics_begin_camera(
            &graphics, camera_x, camera_y);
        (void) tabos_tilemap_draw_layer(
            &graphics,
            &map,
            MYGAME_LAYER_LEVEL_GROUND,
            &sprites,
            &map_draw);
        (void) tabos_sprite_animation_draw(
            &graphics,
            &sprites,
            MYGAME_ANIMATION_PLAYER_WALK,
            player_x,
            player_y,
            elapsed_ms);
        (void) tabos_tilemap_draw_layer(
            &graphics,
            &map,
            MYGAME_LAYER_LEVEL_FOREGROUND,
            &sprites,
            &map_draw);
        (void) tabos_graphics_end_camera(&graphics);
        (void) tabos_graphics_present(&graphics);
        (void) tabos_sleep_ms(16U);
    }

    (void) tabos_graphics_present(&graphics);
    tabos_tilemap_unload(&map);
    tabos_sprite_set_unload(&sprites);
    int result = tabos_graphics_close(&graphics);
    return result == 0 ? 0 : 1;
}
```

The extra present before unloading completes any queued use of asset pixels. The loaded
sprite set and map stay alive until then.

The example uses fixed player and camera coordinates. Add movement after the map renders
correctly; [Add Gameplay](gameplay.md) shows collision and object patterns, while the
[`tdemo` source](../../../apps/tile-demo/src/main.c) shows queued key-event processing and
camera controls.

## Build It

Use the Makefile from [Build and Load Assets](build-assets.md), then run:

```sh
make -C apps/mygame
```

This generates assets, compiles the application, and installs both under the local TabOS
root filesystem.

## Run It

Start the macOS host simulator through the normal TabOS workflow:

```sh
./tools/tabos macos debug run
```

At the TabOS shell, run:

```text
mygame
```

Press Q or Escape to return to the shell. See [Application Lifecycle](../../applications.md)
for the full application build and installation workflow.
