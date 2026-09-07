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

static void move_robot(const tabos_tilemap_t* map, const tabos_sprite_set_t* sprites, int32_t* robot_x,
                       int32_t* robot_y, int32_t dx, int32_t dy)
{
    if (dx != 0 && !position_blocks(map, sprites, *robot_x + dx, *robot_y)) {
        *robot_x += dx;
    }
    if (dy != 0 && !position_blocks(map, sprites, *robot_x, *robot_y + dy)) {
        *robot_y += dy;
    }
}

static bool sprite_overlaps_object(const tabos_sprite_set_t* sprites, uint32_t sprite_id, int32_t sprite_x,
                                   int32_t sprite_y, const tabos_tilemap_object_t* object)
{
    if (sprite_id >= sprites->sprite_count || object->width == 0U || object->height == 0U) {
        return false;
    }
    const tabos_sprite_t* sprite = &sprites->sprites[sprite_id];
    const int64_t left           = (int64_t) sprite_x - sprite->pivot_x;
    const int64_t top            = (int64_t) sprite_y - sprite->pivot_y;
    const int64_t right          = left + sprite->width;
    const int64_t bottom         = top + sprite->height;
    const int64_t object_right   = (int64_t) object->x + object->width;
    const int64_t object_bottom  = (int64_t) object->y + object->height;
    return left < object_right && right > object->x && top < object_bottom && bottom > object->y;
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
        return 1;
    }
    const tabos_tilemap_object_t* spawn =
        tabos_tilemap_object(&map, TDEMO_LAYER_WORLD_MARKERS, TDEMO_OBJECT_WORLD_SPAWN);
    const tabos_tilemap_object_t* grove =
        tabos_tilemap_object(&map, TDEMO_LAYER_WORLD_MARKERS, TDEMO_OBJECT_WORLD_GROVE);
    const tabos_tilemap_object_t* gem = tabos_tilemap_object(&map, TDEMO_LAYER_WORLD_MARKERS, TDEMO_OBJECT_WORLD_GEM);
    const uint32_t gem_sprite         = gem == NULL ? TABOS_SPRITE_NONE : TABOS_TILE_ID(gem->tile);
    int32_t grove_trees               = 0;
    const bool markers_valid          = spawn != NULL && grove != NULL && gem != NULL &&
                               gem->shape == TABOS_TILEMAP_OBJECT_TILE && TABOS_TILE_TRANSFORMS(gem->tile) == 0U &&
                               gem_sprite < sprites.sprite_count &&
                               tabos_tilemap_object_property(grove, "trees", &grove_trees) == 0;
    if (markers_valid && (grove_trees < 1 || grove_trees > 4)) {
        errno = EINVAL;
    }
    if (!markers_valid || grove_trees < 1 || grove_trees > 4) {
        fprintf(stderr, "tdemo: marker lookup failed (errno %d)\n", errno);
        tabos_tilemap_unload(&map);
        tabos_sprite_set_unload(&sprites);
        (void) tabos_graphics_close(&graphics);
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
    bool gem_revealed = false;
    int32_t camera_x  = 0;
    int32_t camera_y  = 0;
    uint64_t started  = tabos_monotonic_ms();
    while (running) {
        bool robot_horizontal_press  = false;
        bool robot_vertical_press    = false;
        bool camera_horizontal_press = false;
        bool camera_vertical_press   = false;
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
                if (down && !event.repeat) {
                    move_robot(&map, &sprites, &robot_x, &robot_y, -DEMO_MOVE_STEP, 0);
                    robot_horizontal_press = true;
                }
            } else if (event.key == TABOS_KEY_D) {
                move_right = down;
                if (down && !event.repeat) {
                    move_robot(&map, &sprites, &robot_x, &robot_y, DEMO_MOVE_STEP, 0);
                    robot_horizontal_press = true;
                }
            } else if (event.key == TABOS_KEY_W) {
                move_up = down;
                if (down && !event.repeat) {
                    move_robot(&map, &sprites, &robot_x, &robot_y, 0, -DEMO_MOVE_STEP);
                    robot_vertical_press = true;
                }
            } else if (event.key == TABOS_KEY_S) {
                move_down = down;
                if (down && !event.repeat) {
                    move_robot(&map, &sprites, &robot_x, &robot_y, 0, DEMO_MOVE_STEP);
                    robot_vertical_press = true;
                }
            } else if (event.key == TABOS_KEY_LEFT) {
                camera_left = down;
                if (down && !event.repeat) {
                    camera_x                -= DEMO_MOVE_STEP;
                    camera_horizontal_press  = true;
                }
            } else if (event.key == TABOS_KEY_RIGHT) {
                camera_right = down;
                if (down && !event.repeat) {
                    camera_x                += DEMO_MOVE_STEP;
                    camera_horizontal_press  = true;
                }
            } else if (event.key == TABOS_KEY_UP) {
                camera_up = down;
                if (down && !event.repeat) {
                    camera_y              -= DEMO_MOVE_STEP;
                    camera_vertical_press  = true;
                }
            } else if (event.key == TABOS_KEY_DOWN) {
                camera_down = down;
                if (down && !event.repeat) {
                    camera_y              += DEMO_MOVE_STEP;
                    camera_vertical_press  = true;
                }
            } else if (down && !event.repeat && event.key == TABOS_KEY_E) {
                tabos_tile_t tile = 0U;
                if (tabos_tilemap_get(&map, TDEMO_LAYER_WORLD_FOREGROUND, 5U, 3U, &tile) == 0) {
                    (void) tabos_tilemap_set(&map, TDEMO_LAYER_WORLD_FOREGROUND, 5U, 3U,
                                             tile == 0U ? TABOS_TILE(TDEMO_SPRITE_WALL) : 0U);
                }
            }
        }
        if (!camera_horizontal_press) {
            camera_x += ((int32_t) camera_right - (int32_t) camera_left) * DEMO_MOVE_STEP;
        }
        if (!camera_vertical_press) {
            camera_y += ((int32_t) camera_down - (int32_t) camera_up) * DEMO_MOVE_STEP;
        }
        const int32_t robot_dx =
            robot_horizontal_press ? 0 : ((int32_t) move_right - (int32_t) move_left) * DEMO_MOVE_STEP;
        const int32_t robot_dy = robot_vertical_press ? 0 : ((int32_t) move_down - (int32_t) move_up) * DEMO_MOVE_STEP;
        move_robot(&map, &sprites, &robot_x, &robot_y, robot_dx, robot_dy);
        const uint64_t elapsed      = tabos_monotonic_ms() - started;
        const uint32_t robot_sprite = tabos_sprite_animation_sprite(&sprites, TDEMO_ANIMATION_ROBOT_WALK, elapsed);
        if (!gem_revealed && sprite_overlaps_object(&sprites, robot_sprite, robot_x, robot_y, gem)) {
            gem_revealed = true;
        }
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
        if (gem_revealed) {
            const tabos_sprite_t* sprite = &sprites.sprites[gem_sprite];
            (void) tabos_sprite_draw(&graphics, &sprites, gem_sprite, gem->x + sprite->pivot_x,
                                     gem->y + sprite->pivot_y);
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
    return result == 0 ? 0 : 1;
}
