#include "equivalence.h"

#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            fprintf(stderr, "asset equivalence line %d: %s\n", __LINE__, #condition); \
            exit(1);                                                                  \
        }                                                                             \
    } while (0)
#define SAME(a, b, field) CHECK((a)->field == (b)->field)

enum {
    WIDTH  = 32,
    HEIGHT = 24,
    PIXELS = WIDTH * HEIGHT
};

static int graphics_open(uint32_t* width, uint32_t* height)
{
    *width  = WIDTH;
    *height = HEIGHT;
    return 0;
}

static int graphics_close(void)
{
    return 0;
}

static const tabos_elf_api_t api = {
    .abi_version    = TABOS_ELF_API_VERSION,
    .graphics_open  = graphics_open,
    .graphics_close = graphics_close,
};
const tabos_elf_api_t* tabos_runtime_api = &api;

static void compare_sprites(const tabos_sprite_set_t* a, const tabos_sprite_set_t* b)
{
    SAME(a, b, image_count);
    SAME(a, b, sprite_count);
    SAME(a, b, animation_count);
    SAME(a, b, metasprite_count);
    CHECK(a->_storage == NULL && b->_storage != NULL);
    for (uint32_t i = 0U; i < a->image_count; ++i) {
        const tabos_sprite_image_t* x = &a->images[i];
        const tabos_sprite_image_t* y = &b->images[i];
        SAME(x, y, width);
        SAME(x, y, height);
        SAME(x, y, color_key);
        SAME(x, y, color_key_enabled);
        CHECK(x->pixels != y->pixels);
        CHECK(memcmp(x->pixels, y->pixels, (size_t) x->width * x->height * sizeof(*x->pixels)) == 0);
    }
    for (uint32_t i = 0U; i < a->sprite_count; ++i) {
        const tabos_sprite_t* x = &a->sprites[i];
        const tabos_sprite_t* y = &b->sprites[i];
        SAME(x, y, image);
        SAME(x, y, x);
        SAME(x, y, y);
        SAME(x, y, width);
        SAME(x, y, height);
        SAME(x, y, pivot_x);
        SAME(x, y, pivot_y);
        SAME(x, y, flags);
        CHECK(tabos_sprite_flags(a, i) == tabos_sprite_flags(b, i));
    }
    for (uint32_t i = 0U; i < a->animation_count; ++i) {
        const tabos_sprite_animation_t* x = &a->animations[i];
        const tabos_sprite_animation_t* y = &b->animations[i];
        SAME(x, y, frame_count);
        SAME(x, y, repeat_count);
        SAME(x, y, trigger_sprite);
        for (uint32_t j = 0U; j < x->frame_count; ++j) {
            SAME(&x->frames[j], &y->frames[j], sprite);
            SAME(&x->frames[j], &y->frames[j], duration_ms);
        }
    }
    for (uint32_t i = 0U; i < a->metasprite_count; ++i) {
        const tabos_metasprite_t* x = &a->metasprites[i];
        const tabos_metasprite_t* y = &b->metasprites[i];
        SAME(x, y, part_count);
        for (uint32_t j = 0U; j < x->part_count; ++j) {
            const tabos_metasprite_part_t* p = &x->parts[j];
            const tabos_metasprite_part_t* q = &y->parts[j];
            SAME(p, q, sprite);
            SAME(p, q, x);
            SAME(p, q, y);
            SAME(p, q, rotation);
            SAME(p, q, mirror_x);
            SAME(p, q, mirror_y);
            SAME(p, q, opacity);
        }
    }
}

static void compare_maps(const tabos_tilemap_t* a, const tabos_tilemap_t* b)
{
    SAME(a, b, width);
    SAME(a, b, height);
    SAME(a, b, tile_width);
    SAME(a, b, tile_height);
    SAME(a, b, layer_count);
    for (uint32_t i = 0U; i < a->layer_count; ++i) {
        const tabos_tilemap_layer_t* x = &a->layers[i];
        const tabos_tilemap_layer_t* y = &b->layers[i];
        SAME(x, y, type);
        SAME(x, y, object_count);
        CHECK(strcmp(x->name, y->name) == 0);
        if (x->type == TABOS_TILEMAP_LAYER_TILES) {
            CHECK(x->cells != y->cells);
            CHECK(memcmp(x->cells, y->cells, (size_t) a->width * a->height * sizeof(*x->cells)) == 0);
        }
        for (uint32_t j = 0U; j < x->object_count; ++j) {
            const tabos_tilemap_object_t* p = &x->objects[j];
            const tabos_tilemap_object_t* q = &y->objects[j];
            SAME(p, q, id);
            SAME(p, q, shape);
            SAME(p, q, x);
            SAME(p, q, y);
            SAME(p, q, width);
            SAME(p, q, height);
            SAME(p, q, tile);
            SAME(p, q, property_count);
            CHECK(strcmp(p->name, q->name) == 0 && strcmp(p->type, q->type) == 0);
            for (uint32_t k = 0U; k < p->property_count; ++k) {
                CHECK(strcmp(p->properties[k].name, q->properties[k].name) == 0);
                SAME(&p->properties[k], &q->properties[k], value);
                int32_t value = 0;
                CHECK(tabos_tilemap_object_property(q, p->properties[k].name, &value) == 0);
                CHECK(value == p->properties[k].value);
            }
        }
    }
}

static void check_ids(const tabos_sprite_set_t* sprites, const tabos_tilemap_t* map)
{
    /* Values come from fixture authoring, not from the binary serializer. */
    CHECK(EQUIVALENCE_SPRITE_QUAD == 0U && EQUIVALENCE_SPRITE_KEYED == 1U && EQUIVALENCE_SPRITE_ACCENT == 2U);
    CHECK(EQUIVALENCE_ANIMATION_CYCLE == 0U && EQUIVALENCE_ANIMATION_ONCE == 1U && EQUIVALENCE_ANIMATION_HOLD == 2U);
    CHECK(EQUIVALENCE_METASPRITE_ACTOR == 0U && EQUIVALENCE_MAP_WORLD == 0U);
    CHECK(EQUIVALENCE_LAYER_WORLD_GROUND == 0U && EQUIVALENCE_LAYER_WORLD_MARKERS == 1U &&
          EQUIVALENCE_LAYER_WORLD_FRONT == 2U && EQUIVALENCE_LAYER_WORLD_ITEMS == 3U);
    CHECK(EQUIVALENCE_OBJECT_WORLD_SPAWN == 7U && EQUIVALENCE_OBJECT_WORLD_ZONE == 11U &&
          EQUIVALENCE_OBJECT_WORLD_ITEM == 29U);
    CHECK(EQUIVALENCE_FLAG_SOLID == UINT32_C(0x80000000) && EQUIVALENCE_FLAG_WATER == 2U);
    CHECK(sprites->image_count == 2U && sprites->sprite_count == 3U && sprites->animation_count == 3U &&
          sprites->metasprite_count == 1U && map->layer_count == 4U);
    CHECK(sprites->sprites[EQUIVALENCE_SPRITE_QUAD].flags == EQUIVALENCE_FLAG_SOLID);
    CHECK(sprites->sprites[EQUIVALENCE_SPRITE_ACCENT].flags == EQUIVALENCE_FLAG_WATER);
    CHECK(map->layers[EQUIVALENCE_LAYER_WORLD_MARKERS].objects[0].id == EQUIVALENCE_OBJECT_WORLD_SPAWN);
    CHECK(map->layers[EQUIVALENCE_LAYER_WORLD_MARKERS].objects[1].id == EQUIVALENCE_OBJECT_WORLD_ZONE);
    CHECK(map->layers[EQUIVALENCE_LAYER_WORLD_ITEMS].objects[0].id == EQUIVALENCE_OBJECT_WORLD_ITEM);
    CHECK(map->layers[EQUIVALENCE_LAYER_WORLD_ITEMS].objects[0].tile ==
          (TABOS_TILE(EQUIVALENCE_SPRITE_ACCENT) | UINT32_C(0xa0000000)));
    CHECK(sprites->animations[EQUIVALENCE_ANIMATION_CYCLE].trigger_sprite == EQUIVALENCE_SPRITE_KEYED);
    CHECK(sprites->animations[EQUIVALENCE_ANIMATION_CYCLE].frames[0].sprite == EQUIVALENCE_SPRITE_QUAD);
}

static void compare_pixels(const tabos_color_t* actual, const tabos_color_t* expected, const char* scene)
{
    for (size_t i = 0U; i < PIXELS; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "%s pixel %zu,%zu: got %04x expected %04x\n", scene, i % WIDTH, i / WIDTH, actual[i],
                    expected[i]);
            exit(1);
        }
    }
}

static void check_default_options(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites)
{
    const tabos_sprite_draw_options_t defaults = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
    CHECK(defaults.width == 0U && defaults.height == 0U && defaults.rotation == TABOS_GRAPHICS_ROTATE_0);
    CHECK(!defaults.mirror_x && !defaults.mirror_y && defaults.opacity == UINT8_MAX);
    CHECK(!defaults.clip_enabled && defaults.clip.x == 0 && defaults.clip.y == 0 && defaults.clip.width == 0U &&
          defaults.clip.height == 0U);

    tabos_color_t expected[PIXELS];
    CHECK(tabos_graphics_clear(graphics, 0x1234U) == 0);
    CHECK(tabos_sprite_draw(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 5, 5) == 0);
    memcpy(expected, graphics->pixels, sizeof(expected));
    CHECK(tabos_graphics_clear(graphics, 0x1234U) == 0);
    CHECK(tabos_sprite_draw_ex(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 5, 5, &defaults) == 0);
    compare_pixels(graphics->pixels, expected, "default sprite options match normal draw");

    CHECK(tabos_graphics_clear(graphics, 0x1234U) == 0);
    CHECK(tabos_sprite_animation_draw(graphics, sprites, EQUIVALENCE_ANIMATION_CYCLE, 5, 5, 10U) == 0);
    memcpy(expected, graphics->pixels, sizeof(expected));
    CHECK(tabos_graphics_clear(graphics, 0x1234U) == 0);
    CHECK(tabos_sprite_animation_draw_ex(graphics, sprites, EQUIVALENCE_ANIMATION_CYCLE, 5, 5, 10U, &defaults) == 0);
    compare_pixels(graphics->pixels, expected, "default animation options match normal draw");

    tabos_sprite_draw_options_t transparent = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
    transparent.opacity                     = 0U;
    CHECK(tabos_graphics_clear(graphics, 0x1234U) == 0);
    memcpy(expected, graphics->pixels, sizeof(expected));
    CHECK(tabos_sprite_draw_ex(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 5, 5, &transparent) == 0);
    compare_pixels(graphics->pixels, expected, "explicit zero opacity remains transparent");
}

static void known_pixels(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites)
{
    tabos_color_t expected[PIXELS] = {0};
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    CHECK(tabos_sprite_draw(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 2, 2) == 0);
    expected[1U * WIDTH + 1U] = 0xf800U;
    expected[1U * WIDTH + 2U] = 0x07e0U;
    expected[2U * WIDTH + 1U] = 0x001fU;
    expected[2U * WIDTH + 2U] = 0xffffU;
    CHECK(tabos_sprite_animation_draw(graphics, sprites, EQUIVALENCE_ANIMATION_CYCLE, 5, 1, 10U) == 0);
    expected[1U * WIDTH + 6U]                 = 0xffe0U;
    expected[2U * WIDTH + 5U]                 = 0xffe0U;
    expected[2U * WIDTH + 6U]                 = 0xffe0U;
    const tabos_sprite_draw_options_t clipped = {
        .width        = 4U,
        .height       = 4U,
        .opacity      = 255U,
        .clip_enabled = true,
        .clip         = {.x = 8, .y = 6, .width = 2U, .height = 2U},
    };
    CHECK(tabos_sprite_animation_draw_ex(graphics, sprites, EQUIVALENCE_ANIMATION_CYCLE, 9, 7, 0U, &clipped) == 0);
    expected[6U * WIDTH + 8U]                 = 0xf800U;
    expected[6U * WIDTH + 9U]                 = 0x07e0U;
    expected[7U * WIDTH + 8U]                 = 0x001fU;
    expected[7U * WIDTH + 9U]                 = 0xffffU;
    const tabos_sprite_draw_options_t opacity = {.opacity = 128U};
    CHECK(tabos_sprite_draw_ex(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 13, 2, &opacity) == 0);
    expected[1U * WIDTH + 12U] = 0x8000U;
    expected[1U * WIDTH + 13U] = 0x0400U;
    expected[2U * WIDTH + 12U] = 0x0010U;
    expected[2U * WIDTH + 13U] = 0x8410U;
    compare_pixels(graphics->pixels, expected, "known sprite colors/pivot/clip/opacity");
}

static void scene(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites, const tabos_tilemap_t* map,
                  uint64_t time, int32_t camera)
{
    CHECK(tabos_graphics_clear(graphics, 0x4208U) == 0);
    CHECK(tabos_graphics_begin_camera(graphics, camera - 2, -3) == 0);
    const tabos_tilemap_draw_options_t draw = {
        .viewport     = {.x = 2, .y = 2, .width = 15U, .height = 7U},
        .animation_ms = time,
    };
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &draw) == 0);
    CHECK(tabos_sprite_animation_draw(graphics, sprites, EQUIVALENCE_ANIMATION_HOLD, 5, 4, time) == 0);
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_FRONT, sprites, &draw) == 0);
    for (uint32_t rotation = 0U; rotation < 4U; ++rotation) {
        for (uint32_t mirror = 0U; mirror < 4U; ++mirror) {
            const tabos_sprite_draw_options_t options = {
                .width        = 3U,
                .height       = 4U,
                .rotation     = (tabos_graphics_rotation_t) rotation,
                .mirror_x     = (mirror & 1U) != 0U,
                .mirror_y     = (mirror & 2U) != 0U,
                .opacity      = 192U,
                .clip_enabled = true,
                .clip         = {.x = 1, .y = 10, .width = 29U, .height = 13U},
            };
            CHECK(tabos_sprite_draw_ex(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 2 + (int32_t) mirror * 7,
                                       11 + (int32_t) rotation * 3, &options) == 0);
        }
    }
    CHECK(tabos_metasprite_draw(graphics, sprites, EQUIVALENCE_METASPRITE_ACTOR, 25, 4, camera < 0, camera > 0, 200U) ==
          0);
    CHECK(tabos_graphics_end_camera(graphics) == 0);
}

static void known_tiles(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites, const tabos_tilemap_t* map)
{
    /* Independently specified output of all eight Tiled H/V/D combinations. */
    const tabos_color_t rows[32] = {
        0xf800, 0x07e0, 0x07e0, 0xf800, 0x001f, 0xffff, 0xffff, 0x001f, 0xf800, 0x001f, 0x001f,
        0xf800, 0x07e0, 0xffff, 0xffff, 0x07e0, 0x001f, 0xffff, 0xffff, 0x001f, 0xf800, 0x07e0,
        0x07e0, 0xf800, 0x07e0, 0xffff, 0xffff, 0x07e0, 0xf800, 0x001f, 0x001f, 0xf800,
    };
    tabos_color_t expected[PIXELS] = {0};
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    const tabos_tilemap_draw_options_t draw = {
        .viewport = {.width = 16U, .height = 2U}
    };
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &draw) == 0);
    memcpy(expected, rows, 16U * sizeof(rows[0]));
    memcpy(expected + WIDTH, rows + 16U, 16U * sizeof(rows[0]));
    compare_pixels(graphics->pixels, expected, "known tile transforms and viewport bounds");
}

static void known_compositing(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites, const tabos_tilemap_t* map)
{
    tabos_color_t expected[PIXELS] = {0};
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    const tabos_tilemap_draw_options_t draw = {
        .viewport     = {.width = 4U, .height = 2U},
        .animation_ms = 10U,
    };
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &draw) == 0);
    CHECK(tabos_sprite_draw(graphics, sprites, EQUIVALENCE_SPRITE_ACCENT, 2, 0) == 0);
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_FRONT, sprites, &draw) == 0);
    expected[0]          = 0xf800U;
    expected[1]          = 0x07e0U;
    expected[2]          = 0x07ffU; /* Foreground color key preserves the interleaved sprite. */
    expected[3]          = 0xffe0U;
    expected[WIDTH]      = 0x001fU;
    expected[WIDTH + 1U] = 0xffffU;
    expected[WIDTH + 2U] = 0xffe0U;
    expected[WIDTH + 3U] = 0xffe0U;
    CHECK(tabos_metasprite_draw(graphics, sprites, EQUIVALENCE_METASPRITE_ACTOR, 10, 10, false, false, 255U) == 0);
    /* The quarter-turn plus mirror transposes the first part; the second covers its right column. */
    expected[10U * WIDTH + 8U]  = 0x8000U;
    expected[11U * WIDTH + 8U]  = 0x0400U;
    expected[10U * WIDTH + 9U]  = 0x07ffU;
    expected[10U * WIDTH + 10U] = 0x07ffU;
    expected[11U * WIDTH + 9U]  = 0x07ffU;
    expected[11U * WIDTH + 10U] = 0x07ffU;
    compare_pixels(graphics->pixels, expected, "known layer and metasprite order");
}

static void camera_content(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites, const tabos_tilemap_t* map,
                           tabos_graphics_rect_t viewport)
{
    const tabos_tilemap_draw_options_t draw = {.viewport = viewport, .animation_ms = 10U};
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &draw) == 0);
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_FRONT, sprites, &draw) == 0);
    CHECK(tabos_sprite_draw(graphics, sprites, EQUIVALENCE_SPRITE_QUAD, 5, 8) == 0);
    CHECK(tabos_sprite_animation_draw(graphics, sprites, EQUIVALENCE_ANIMATION_HOLD, 10, 8, 0U) == 0);
    CHECK(tabos_metasprite_draw(graphics, sprites, EQUIVALENCE_METASPRITE_ACTOR, 20, 8, false, false, 255U) == 0);
}

static void check_camera_pixels(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites,
                                const tabos_tilemap_t* map)
{
    const tabos_graphics_rect_t full = {.width = WIDTH, .height = HEIGHT};
    tabos_color_t reference[PIXELS];
    tabos_color_t expected[PIXELS];
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    camera_content(graphics, sprites, map, full);
    memcpy(reference, graphics->pixels, sizeof(reference));
    const int32_t offsets[][2] = {
        {-3, -2},
        {-1,  1},
        { 0,  0},
        { 1, -1},
        { 3,  2},
        {30, 20}
    };
    for (size_t i = 0U; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        CHECK(tabos_graphics_begin_camera(graphics, offsets[i][0], offsets[i][1]) == 0);
        CHECK(tabos_graphics_clear(graphics, 0U) == 0);
        camera_content(graphics, sprites, map, full);
        memset(expected, 0, sizeof(expected));
        for (int32_t y = 0; y < HEIGHT; ++y) {
            for (int32_t x = 0; x < WIDTH; ++x) {
                const int32_t world_x = x + offsets[i][0];
                const int32_t world_y = y + offsets[i][1];
                if (world_x >= 0 && world_x < WIDTH && world_y >= 0 && world_y < HEIGHT) {
                    expected[y * WIDTH + x] = reference[world_y * WIDTH + world_x];
                }
            }
        }
        CHECK(tabos_graphics_end_camera(graphics) == 0);
        CHECK(tabos_graphics_pixel(graphics, 0, HEIGHT - 1, 0xffffU) == 0);
        expected[(HEIGHT - 1) * WIDTH] = 0xffffU;
        compare_pixels(graphics->pixels, expected, "shared camera moves world and leaves HUD fixed");
    }
    /* A screen viewport clips the map without relocating its world origin. */
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    CHECK(tabos_graphics_begin_camera(graphics, -1, -1) == 0);
    const tabos_tilemap_draw_options_t clipped = {
        .viewport = {.x = 3, .y = 1, .width = 7U, .height = 2U},
          .animation_ms = 10U
    };
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &clipped) == 0);
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_FRONT, sprites, &clipped) == 0);
    memset(expected, 0, sizeof(expected));
    for (int32_t y = 1; y < 3; ++y) {
        for (int32_t x = 3; x < 10; ++x) {
            expected[y * WIDTH + x] = reference[(y - 1) * WIDTH + x - 1];
        }
    }
    compare_pixels(graphics->pixels, expected, "camera viewport clips without shifting map origin");
    CHECK(tabos_graphics_begin_camera(graphics, INT32_MAX, INT32_MIN) == 0);
    CHECK(tabos_graphics_clear(graphics, 0U) == 0);
    CHECK(tabos_tilemap_draw_layer(graphics, map, EQUIVALENCE_LAYER_WORLD_GROUND, sprites, &clipped) == 0);
    memset(expected, 0, sizeof(expected));
    compare_pixels(graphics->pixels, expected, "far outside map remains empty");
    CHECK(tabos_graphics_end_camera(graphics) == 0);
}

static void check_completion(const tabos_sprite_set_t* sprites)
{
    const uint64_t times[] = {0U, 14U, 15U, 29U, 39U, 40U, 41U, UINT64_MAX};
    for (size_t index = 0U; index < sizeof(times) / sizeof(times[0]); ++index) {
        bool finished = true;
        CHECK(tabos_sprite_animation_finished(sprites, EQUIVALENCE_ANIMATION_CYCLE, times[index], &finished) == 0);
        CHECK(!finished);
        CHECK(tabos_sprite_animation_finished(sprites, EQUIVALENCE_ANIMATION_ONCE, times[index], &finished) == 0);
        CHECK(finished == (times[index] >= 15U));
        CHECK(tabos_sprite_animation_finished(sprites, EQUIVALENCE_ANIMATION_HOLD, times[index], &finished) == 0);
        CHECK(finished == (times[index] >= 40U));
    }
    /* A new elapsed time restarts playback; querying/drawing never mutates clip state. */
    bool finished = true;
    CHECK(tabos_sprite_animation_finished(sprites, EQUIVALENCE_ANIMATION_ONCE, 0U, &finished) == 0 && !finished);

    const tabos_sprite_frame_t long_frames[] = {
        {.sprite = 0U, .duration_ms = UINT32_MAX},
        {.sprite = 1U, .duration_ms = UINT32_MAX}
    };
    const tabos_sprite_animation_t long_animation = {
        .frames       = long_frames,
        .frame_count  = 2U,
        .repeat_count = UINT32_MAX,
    };
    tabos_sprite_set_t long_set = *sprites;
    long_set.animations         = &long_animation;
    long_set.animation_count    = 1U;
    CHECK(tabos_sprite_animation_finished(&long_set, 0U, UINT64_MAX, &finished) == 0 && !finished);
}

static void check_animation_errors(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites)
{
    tabos_color_t before[PIXELS];
    memcpy(before, graphics->pixels, sizeof(before));
    bool finished = true;
    CHECK(tabos_sprite_animation_finished(NULL, 0U, 0U, &finished) == -1 && errno == EINVAL && finished);
    CHECK(tabos_sprite_animation_finished(sprites, UINT32_MAX, 0U, &finished) == -1 && errno == EINVAL && finished);
    CHECK(tabos_sprite_animation_finished(sprites, 0U, 0U, NULL) == -1 && errno == EINVAL);
    CHECK(tabos_sprite_animation_draw(graphics, sprites, UINT32_MAX, 0, 0, 0U) == -1 && errno == EINVAL);
    CHECK(tabos_sprite_animation_draw(NULL, sprites, 0U, 0, 0, 0U) == -1 && errno == EINVAL);
    CHECK(tabos_sprite_animation_draw_ex(graphics, sprites, 0U, 0, 0, 0U, NULL) == -1 && errno == EINVAL);
    tabos_sprite_set_t invalid = *sprites;
    invalid.animations         = NULL;
    CHECK(tabos_sprite_animation_finished(&invalid, 0U, 0U, &finished) == -1 && errno == EINVAL && finished);
    CHECK(tabos_sprite_animation_draw(graphics, &invalid, 0U, 0, 0, 0U) == -1 && errno == EINVAL);
    tabos_sprite_frame_t frame    = {.sprite = 0U, .duration_ms = 10U};
    tabos_sprite_animation_t clip = {.frames = &frame, .frame_count = 1U, .repeat_count = 1U};
    invalid.animations            = &clip;
    invalid.animation_count       = 1U;
    for (uint32_t index = 0U; index < 4U; ++index) {
        clip.frames       = &frame;
        clip.frame_count  = 1U;
        frame.sprite      = 0U;
        frame.duration_ms = 10U;
        if (index == 0U) {
            clip.frames = NULL;
        } else if (index == 1U) {
            clip.frame_count = 0U;
        } else if (index == 2U) {
            frame.duration_ms = 0U;
        } else {
            frame.sprite = sprites->sprite_count;
        }
        CHECK(tabos_sprite_animation_finished(&invalid, 0U, 0U, &finished) == -1 && errno == EINVAL && finished);
        CHECK(tabos_sprite_animation_frame(&invalid, 0U, 0U) == TABOS_SPRITE_NONE && errno == EINVAL);
        CHECK(tabos_sprite_animation_draw(graphics, &invalid, 0U, 0, 0, 0U) == -1 && errno == EINVAL);
    }
    compare_pixels(graphics->pixels, before, "invalid animations leave canvas unchanged");
}

int main(int argc, char** argv)
{
    CHECK(argc == 3);
    tabos_sprite_set_t loaded = {0};
    tabos_tilemap_t map       = {0};
    CHECK(tabos_sprite_set_load(argv[1], &loaded) == 0);
    CHECK(tabos_tilemap_load(argv[2], &map) == 0);
    compare_sprites(&equivalence_sprites, &loaded);
    compare_maps(&equivalence_world, &map);
    check_ids(&equivalence_sprites, &equivalence_world);
    check_ids(&loaded, &map);
    tabos_graphics_t graphics = {.width = WIDTH, .height = HEIGHT};
    CHECK(tabos_graphics_open(&graphics) == 0);
    check_default_options(&graphics, &equivalence_sprites);
    check_default_options(&graphics, &loaded);
    check_camera_pixels(&graphics, &equivalence_sprites, &equivalence_world);
    check_camera_pixels(&graphics, &loaded, &map);
    check_completion(&equivalence_sprites);
    check_completion(&loaded);
    check_animation_errors(&graphics, &loaded);
    known_pixels(&graphics, &equivalence_sprites);
    known_pixels(&graphics, &loaded);
    known_tiles(&graphics, &equivalence_sprites, &equivalence_world);
    known_tiles(&graphics, &loaded, &map);
    known_compositing(&graphics, &equivalence_sprites, &equivalence_world);
    known_compositing(&graphics, &loaded, &map);
    const uint64_t times[]        = {0U, 6U, 7U, 9U, 10U, 19U, 20U, 29U, 30U, 39U, 40U, 60U, UINT64_MAX};
    const int32_t cameras[]       = {-3, -1, 0, 1, 3, 17};
    const uint32_t cycle_frames[] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1};
    const uint32_t hold_frames[]  = {2, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1};
    size_t scenes                 = 0U;
    for (size_t i = 0U; i < sizeof(times) / sizeof(times[0]); ++i) {
        CHECK(tabos_sprite_animation_frame(&loaded, EQUIVALENCE_ANIMATION_CYCLE, times[i]) == cycle_frames[i]);
        CHECK(tabos_sprite_animation_frame(&loaded, EQUIVALENCE_ANIMATION_HOLD, times[i]) == hold_frames[i]);
        CHECK(tabos_sprite_animation_frame(&equivalence_sprites, EQUIVALENCE_ANIMATION_CYCLE, times[i]) ==
              cycle_frames[i]);
        CHECK(tabos_sprite_animation_frame(&equivalence_sprites, EQUIVALENCE_ANIMATION_HOLD, times[i]) ==
              hold_frames[i]);
        for (size_t j = 0U; j < sizeof(cameras) / sizeof(cameras[0]); ++j) {
            tabos_color_t expected[PIXELS];
            scene(&graphics, &equivalence_sprites, &equivalence_world, times[i], cameras[j]);
            memcpy(expected, graphics.pixels, sizeof(expected));
            scene(&graphics, &loaded, &map, times[i], cameras[j]);
            compare_pixels(graphics.pixels, expected, "generated-C/binary scene");
            ++scenes;
        }
    }
    CHECK(tabos_tilemap_set(&equivalence_world, 0U, 1U, 1U, TABOS_TILE(2U)) == 0);
    CHECK(map.layers[0].cells[9] == 0U);
    CHECK(tabos_tilemap_set(&map, 0U, 1U, 1U, TABOS_TILE(2U)) == 0);
    compare_maps(&equivalence_world, &map);
    tabos_color_t edited[PIXELS];
    scene(&graphics, &equivalence_sprites, &equivalence_world, 0U, 0);
    memcpy(edited, graphics.pixels, sizeof(edited));
    scene(&graphics, &loaded, &map, 0U, 0);
    compare_pixels(graphics.pixels, edited, "edited generated-C/binary scene");
    ++scenes;
    CHECK(tabos_graphics_close(&graphics) == 0);
    tabos_tilemap_unload(&map);
    tabos_sprite_set_unload(&loaded);
    printf("asset equivalence: all metadata, %zu scene pairs, independent pixel and animation expectations passed\n",
           scenes);
    return 0;
}
