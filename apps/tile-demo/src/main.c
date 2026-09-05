#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tilemap.h>
#include <tabos/tty.h>
#include <unistd.h>

enum {
    DEMO_WIDTH             = 160,
    DEMO_HEIGHT            = 120,
    DEMO_SPRITE_ROBOT_WALK = 0,
    DEMO_METASPRITE_TREE   = 0,
};

static void restore_tty(uint32_t mode)
{
    (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, mode);
}

int main(void)
{
    uint32_t tty_mode = 0U;
    (void) ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty_mode);
    (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE,
                 (tty_mode & ~(uint32_t) TABOS_TTY_MODE_SCROLL_KEYS) | (uint32_t) TABOS_TTY_MODE_RAW_INPUT);
    tabos_graphics_t graphics  = {.width = DEMO_WIDTH, .height = DEMO_HEIGHT};
    tabos_sprite_set_t sprites = {0};
    tabos_tilemap_t map        = {0};
    if (tabos_graphics_open(&graphics) != 0 ||
        tabos_sprite_set_load("T:/data/tile-demo/tile-demo.tsp", &sprites) != 0 ||
        tabos_tilemap_load("T:/data/tile-demo/world.tmap", &map) != 0) {
        fprintf(stderr, "tile-demo: load failed (errno %d)\n", errno);
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        if (graphics.open) {
            (void) tabos_graphics_close(&graphics);
        }
        restore_tty(tty_mode);
        return 1;
    }
    bool running     = true;
    int32_t camera_x = 0;
    int32_t camera_y = 0;
    uint64_t started = tabos_monotonic_ms();
    while (running) {
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type != TABOS_INPUT_KEY_DOWN) {
                continue;
            }
            if (event.key == TABOS_KEY_Q) {
                running = false;
            } else if (event.key == TABOS_KEY_LEFT) {
                camera_x -= 2;
            } else if (event.key == TABOS_KEY_RIGHT) {
                camera_x += 2;
            } else if (event.key == TABOS_KEY_UP) {
                camera_y -= 2;
            } else if (event.key == TABOS_KEY_DOWN) {
                camera_y += 2;
            } else if (event.key == TABOS_KEY_E) {
                tabos_tile_t tile = 0U;
                if (tabos_tilemap_get(&map, 1U, 5U, 3U, &tile) == 0) {
                    (void) tabos_tilemap_set(&map, 1U, 5U, 3U, tile == 0U ? TABOS_TILE(2U) : 0U);
                }
            }
        }
        const uint64_t elapsed                  = tabos_monotonic_ms() - started;
        const tabos_tilemap_draw_options_t draw = {
            .viewport     = {.width = DEMO_WIDTH, .height = DEMO_HEIGHT},
            .animation_ms = elapsed,
        };
        (void) tabos_graphics_clear(&graphics, TABOS_RGB565(8, 18, 30));
        (void) tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
        (void) tabos_tilemap_draw_layer(&graphics, &map, 0U, &sprites, &draw);
        (void) tabos_sprite_animation_draw(&graphics, &sprites, DEMO_SPRITE_ROBOT_WALK, 80, 68, elapsed);
        (void) tabos_metasprite_draw(&graphics, &sprites, DEMO_METASPRITE_TREE, 120, 72, false, false, 255U);
        (void) tabos_tilemap_draw_layer(&graphics, &map, 1U, &sprites, &draw);
        const tabos_tilemap_layer_t* markers = &map.layers[2];
        for (uint32_t index = 0U; index < markers->object_count; ++index) {
            const tabos_tilemap_object_t* object = &markers->objects[index];
            (void) tabos_graphics_rect(&graphics, object->x, object->y, object->width == 0U ? 2U : object->width,
                                       object->height == 0U ? 2U : object->height, TABOS_RGB565(255, 255, 0));
        }
        (void) tabos_graphics_end_camera(&graphics);
        (void) tabos_graphics_present(&graphics);
        (void) tabos_sleep_ms(16U);
    }
    (void) tabos_graphics_present(&graphics);
    tabos_tilemap_unload(&map);
    tabos_sprite_set_unload(&sprites);
    const int result = tabos_graphics_close(&graphics);
    restore_tty(tty_mode);
    return result == 0 ? 0 : 1;
}
