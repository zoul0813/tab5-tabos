#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tilemap.h>
#include <tabos/tty.h>
#include <unistd.h>

#include <tdemo.h>

enum {
    DEMO_WIDTH     = 426,
    DEMO_HEIGHT    = 240,
    DEMO_MOVE_STEP = 4,
};

static void restore_tty(uint32_t mode)
{
    (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, mode);
}

static bool tile_blocks(const tabos_tilemap_t* map, const tabos_sprite_set_t* sprites, uint32_t layer, int32_t x,
                        int32_t y)
{
    if (x < 0 || y < 0 || (uint64_t) (uint32_t) x >= (uint64_t) map->width * map->tile_width ||
        (uint64_t) (uint32_t) y >= (uint64_t) map->height * map->tile_height) {
        return true;
    }
    tabos_tile_t tile = TABOS_TILE_EMPTY;
    if (tabos_tilemap_get(map, layer, (uint32_t) x / map->tile_width, (uint32_t) y / map->tile_height, &tile) != 0) {
        return true;
    }
    if (tile == TABOS_TILE_EMPTY) {
        return false;
    }
    return (tabos_sprite_flags(sprites, TABOS_TILE_ID(tile)) & (TDEMO_FLAG_SOLID | TDEMO_FLAG_WATER)) != 0U;
}

static bool position_blocks(const tabos_tilemap_t* map, const tabos_sprite_set_t* sprites, int32_t x, int32_t y)
{
    return tile_blocks(map, sprites, TDEMO_LAYER_WORLD_GROUND, x, y) ||
           tile_blocks(map, sprites, TDEMO_LAYER_WORLD_FOREGROUND, x, y);
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
    if (tabos_graphics_open(&graphics) != 0 || tabos_sprite_set_load("T:/data/tdemo/tdemo.tsp", &sprites) != 0 ||
        tabos_tilemap_load("T:/data/tdemo/world.tmap", &map) != 0) {
        fprintf(stderr, "tdemo: load failed (errno %d)\n", errno);
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        if (graphics.open) {
            (void) tabos_graphics_close(&graphics);
        }
        restore_tty(tty_mode);
        return 1;
    }
    const tabos_tilemap_object_t* spawn =
        tabos_tilemap_object(&map, TDEMO_LAYER_WORLD_MARKERS, TDEMO_OBJECT_WORLD_SPAWN);
    const tabos_tilemap_object_t* grove =
        tabos_tilemap_object(&map, TDEMO_LAYER_WORLD_MARKERS, TDEMO_OBJECT_WORLD_GROVE);
    int32_t grove_trees = 0;
    const bool markers_valid =
        spawn != NULL && grove != NULL && tabos_tilemap_object_property(grove, "trees", &grove_trees) == 0;
    if (markers_valid && (grove_trees < 1 || grove_trees > 4)) {
        errno = EINVAL;
    }
    if (!markers_valid || grove_trees < 1 || grove_trees > 4) {
        fprintf(stderr, "tdemo: marker lookup failed (errno %d)\n", errno);
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        (void) tabos_graphics_close(&graphics);
        restore_tty(tty_mode);
        return 1;
    }
    int32_t robot_x   = spawn->x;
    int32_t robot_y   = spawn->y;
    bool running      = true;
    bool move_left    = false;
    bool move_right   = false;
    bool move_up      = false;
    bool move_down    = false;
    bool camera_left  = false;
    bool camera_right = false;
    bool camera_up    = false;
    bool camera_down  = false;
    int32_t camera_x  = 0;
    int32_t camera_y  = 0;
    uint64_t started  = tabos_monotonic_ms();
    while (running) {
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type != TABOS_INPUT_KEY_DOWN && event.type != TABOS_INPUT_KEY_UP) {
                continue;
            }
            const bool down = event.type == TABOS_INPUT_KEY_DOWN;
            if (down && (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE)) {
                running = false;
            } else if (event.key == TABOS_KEY_A) {
                move_left = down;
            } else if (event.key == TABOS_KEY_D) {
                move_right = down;
            } else if (event.key == TABOS_KEY_W) {
                move_up = down;
            } else if (event.key == TABOS_KEY_S) {
                move_down = down;
            } else if (event.key == TABOS_KEY_LEFT) {
                camera_left = down;
            } else if (event.key == TABOS_KEY_RIGHT) {
                camera_right = down;
            } else if (event.key == TABOS_KEY_UP) {
                camera_up = down;
            } else if (event.key == TABOS_KEY_DOWN) {
                camera_down = down;
            } else if (down && !event.repeat && event.key == TABOS_KEY_E) {
                tabos_tile_t tile = 0U;
                if (tabos_tilemap_get(&map, TDEMO_LAYER_WORLD_FOREGROUND, 5U, 3U, &tile) == 0) {
                    (void) tabos_tilemap_set(&map, TDEMO_LAYER_WORLD_FOREGROUND, 5U, 3U,
                                             tile == 0U ? TABOS_TILE(TDEMO_SPRITE_WALL) : 0U);
                }
            }
        }
        camera_x               += ((int32_t) camera_right - (int32_t) camera_left) * DEMO_MOVE_STEP;
        camera_y               += ((int32_t) camera_down - (int32_t) camera_up) * DEMO_MOVE_STEP;
        const int32_t robot_dx  = ((int32_t) move_right - (int32_t) move_left) * DEMO_MOVE_STEP;
        const int32_t robot_dy  = ((int32_t) move_down - (int32_t) move_up) * DEMO_MOVE_STEP;
        if (robot_dx != 0 && !position_blocks(&map, &sprites, robot_x + robot_dx, robot_y)) {
            robot_x += robot_dx;
        }
        if (robot_dy != 0 && !position_blocks(&map, &sprites, robot_x, robot_y + robot_dy)) {
            robot_y += robot_dy;
        }
        const uint64_t elapsed            = tabos_monotonic_ms() - started;
        tabos_tilemap_draw_options_t draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
        draw.animation_ms                 = elapsed;
        (void) tabos_graphics_clear(&graphics, TABOS_RGB565(8, 18, 30));
        (void) tabos_graphics_begin_camera(&graphics, camera_x, camera_y);
        (void) tabos_tilemap_draw_layer(&graphics, &map, TDEMO_LAYER_WORLD_GROUND, &sprites, &draw);
        (void) tabos_sprite_animation_draw(&graphics, &sprites, TDEMO_ANIMATION_ROBOT_WALK, robot_x, robot_y, elapsed);
        (void) tabos_tilemap_draw_layer(&graphics, &map, TDEMO_LAYER_WORLD_FOREGROUND, &sprites, &draw);
        for (int32_t tree = 0; tree < grove_trees; ++tree) {
            const int32_t tree_x = grove->x + 8 + tree % 2 * 16;
            const int32_t tree_y = grove->y + 15 + tree / 2 * 16;
            (void) tabos_metasprite_draw(&graphics, &sprites, TDEMO_METASPRITE_TREE_SHADOW, tree_x, tree_y, false,
                                         false, 255U);
        }
        const tabos_tilemap_layer_t* markers = &map.layers[TDEMO_LAYER_WORLD_MARKERS];
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
