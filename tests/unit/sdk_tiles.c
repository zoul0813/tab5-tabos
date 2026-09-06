#include <tabos/internal/elf_api.h>
#include <tabos/tilemap.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned int platform_blit_count;
static unsigned int platform_present_count;
static tabos_graphics_blit_options_t platform_blit;
static tabos_graphics_blit_options_t platform_blits[128];

static int graphics_open(uint32_t* width, uint32_t* height)
{
    *width  = 32U;
    *height = 32U;
    return 0;
}

static int graphics_close(void)
{
    return 0;
}

static int graphics_present(void)
{
    ++platform_present_count;
    return 0;
}

static int graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
    platform_blit = *options;
    if (platform_blit_count < sizeof(platform_blits) / sizeof(platform_blits[0])) {
        platform_blits[platform_blit_count] = *options;
    }
    ++platform_blit_count;
    return 0;
}

static const tabos_elf_api_t api = {
    .abi_version      = TABOS_ELF_API_VERSION,
    .graphics_open    = graphics_open,
    .graphics_close   = graphics_close,
    .graphics_present = graphics_present,
    .graphics_blit_ex = graphics_blit_ex,
};

const tabos_elf_api_t* tabos_runtime_api = &api;

static bool write_bad_asset(const char* source_path, const char* destination_path, size_t bytes)
{
    FILE* source      = fopen(source_path, "rb");
    FILE* destination = fopen(destination_path, "wb");
    if (source == NULL || destination == NULL) {
        if (source != NULL) {
            fclose(source);
        }
        if (destination != NULL) {
            fclose(destination);
        }
        return false;
    }
    uint8_t buffer[64];
    const size_t count = fread(buffer, 1U, bytes, source);
    if (count != bytes) {
        fclose(source);
        fclose(destination);
        return false;
    }
    buffer[0]                     = 'X';
    const bool written            = fwrite(buffer, 1U, count, destination) == count;
    const bool source_closed      = fclose(source) == 0;
    const bool destination_closed = fclose(destination) == 0;
    return source_closed && destination_closed && written;
}

static bool rectangle_matches(const tabos_graphics_t* graphics, int32_t x, int32_t y, uint32_t width, uint32_t height,
                              tabos_color_t color)
{
    for (uint32_t row = 0U; row < graphics->height; ++row) {
        for (uint32_t column = 0U; column < graphics->width; ++column) {
            const bool inside = column >= (uint32_t) x && column < (uint32_t) x + width && row >= (uint32_t) y &&
                                row < (uint32_t) y + height;
            if (graphics->pixels[(size_t) row * graphics->width + column] != (inside ? color : 0U)) {
                return false;
            }
        }
    }
    return true;
}

static bool transformed_pivots(tabos_graphics_t* graphics, tabos_color_t color)
{
    const tabos_color_t pixels[]        = {color, color, color, color, color, color};
    const tabos_sprite_image_t images[] = {
        {.pixels = pixels, .width = 3U, .height = 2U}
    };
    const tabos_sprite_t descriptors[] = {
        {.width = 3U, .height = 2U, .pivot_x = 1, .pivot_y = 0}
    };
    const tabos_sprite_set_t sprites = {
        .images = images, .image_count = 1U, .sprites = descriptors, .sprite_count = 1U};
    const struct {
            tabos_graphics_rotation_t rotation;
            bool mirror_x;
            bool mirror_y;
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;
    } cases[] = {
        {  TABOS_GRAPHICS_ROTATE_0, false, false, 7, 8, 3U, 2U},
        {  TABOS_GRAPHICS_ROTATE_0, false,  true, 7, 6, 3U, 2U},
        {  TABOS_GRAPHICS_ROTATE_0,  true, false, 6, 8, 3U, 2U},
        {  TABOS_GRAPHICS_ROTATE_0,  true,  true, 6, 6, 3U, 2U},
        { TABOS_GRAPHICS_ROTATE_90, false, false, 8, 6, 2U, 3U},
        { TABOS_GRAPHICS_ROTATE_90, false,  true, 6, 6, 2U, 3U},
        { TABOS_GRAPHICS_ROTATE_90,  true, false, 8, 7, 2U, 3U},
        { TABOS_GRAPHICS_ROTATE_90,  true,  true, 6, 7, 2U, 3U},
        {TABOS_GRAPHICS_ROTATE_180, false, false, 6, 6, 3U, 2U},
        {TABOS_GRAPHICS_ROTATE_180, false,  true, 6, 8, 3U, 2U},
        {TABOS_GRAPHICS_ROTATE_180,  true, false, 7, 6, 3U, 2U},
        {TABOS_GRAPHICS_ROTATE_180,  true,  true, 7, 8, 3U, 2U},
        {TABOS_GRAPHICS_ROTATE_270, false, false, 6, 7, 2U, 3U},
        {TABOS_GRAPHICS_ROTATE_270, false,  true, 8, 7, 2U, 3U},
        {TABOS_GRAPHICS_ROTATE_270,  true, false, 6, 6, 2U, 3U},
        {TABOS_GRAPHICS_ROTATE_270,  true,  true, 8, 6, 2U, 3U},
    };
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        tabos_sprite_draw_options_t options = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
        options.rotation                    = cases[index].rotation;
        options.mirror_x                    = cases[index].mirror_x;
        options.mirror_y                    = cases[index].mirror_y;
        if (tabos_graphics_clear(graphics, 0U) != 0 ||
            tabos_sprite_draw_ex(graphics, &sprites, 0U, 8, 8, &options) != 0 ||
            !rectangle_matches(graphics, cases[index].x, cases[index].y, cases[index].width, cases[index].height,
                               color)) {
            return false;
        }
    }

    tabos_sprite_t outside_descriptor  = descriptors[0];
    outside_descriptor.pivot_x         = -1;
    outside_descriptor.pivot_y         = 3;
    tabos_sprite_set_t outside         = sprites;
    outside.sprites                    = &outside_descriptor;
    tabos_sprite_draw_options_t scaled = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
    scaled.width                       = 6U;
    scaled.height                      = 4U;
    if (tabos_graphics_clear(graphics, 0U) != 0 || tabos_sprite_draw_ex(graphics, &outside, 0U, 4, 8, &scaled) != 0 ||
        !rectangle_matches(graphics, 6, 2, 6U, 4U, color)) {
        return false;
    }
    outside_descriptor.pivot_x = INT32_MIN;
    scaled.width               = UINT32_MAX;
    return tabos_sprite_draw_ex(graphics, &outside, 0U, 4, 8, &scaled) == -1 && errno == EOVERFLOW;
}

static bool invalid_descriptors(tabos_graphics_t* graphics, const tabos_sprite_set_t* sprites,
                                const tabos_tilemap_t* map, const tabos_tilemap_draw_options_t* draw)
{
    tabos_sprite_set_t invalid_sprites = *sprites;
    invalid_sprites.sprites            = NULL;
    errno                              = 0;
    if (tabos_sprite_draw(graphics, &invalid_sprites, 0U, 0, 0) != -1 || errno != EINVAL ||
        tabos_sprite_flags(&invalid_sprites, 0U) != 0U || errno != EINVAL) {
        return false;
    }

    invalid_sprites        = *sprites;
    invalid_sprites.images = NULL;
    if (tabos_sprite_draw(graphics, &invalid_sprites, 0U, 0, 0) != -1 || errno != EINVAL) {
        return false;
    }

    invalid_sprites             = *sprites;
    invalid_sprites.metasprites = NULL;
    if (tabos_metasprite_draw(graphics, &invalid_sprites, 0U, 0, 0, false, false, 255U) != -1 || errno != EINVAL) {
        return false;
    }
    const tabos_metasprite_t missing_parts = {.part_count = 1U};
    invalid_sprites                        = *sprites;
    invalid_sprites.metasprites            = &missing_parts;
    invalid_sprites.metasprite_count       = 1U;
    if (tabos_metasprite_draw(graphics, &invalid_sprites, 0U, 0, 0, false, false, 255U) != -1 || errno != EINVAL) {
        return false;
    }

    const tabos_metasprite_part_t overflow_part  = {.sprite = 0U, .x = 1, .opacity = 255U};
    const tabos_metasprite_t overflow_metasprite = {.parts = &overflow_part, .part_count = 1U};
    invalid_sprites                              = *sprites;
    invalid_sprites.metasprites                  = &overflow_metasprite;
    invalid_sprites.metasprite_count             = 1U;
    if (tabos_metasprite_draw(graphics, &invalid_sprites, 0U, INT32_MAX, 0, false, false, 255U) != -1 ||
        errno != EOVERFLOW) {
        return false;
    }

    invalid_sprites            = *sprites;
    invalid_sprites.animations = NULL;
    if (tabos_tilemap_draw_layer(graphics, map, 0U, &invalid_sprites, draw) != -1 || errno != EINVAL) {
        return false;
    }

    tabos_tilemap_layer_t invalid_layer = map->layers[0];
    tabos_tilemap_t invalid_map         = *map;
    invalid_map.layers                  = &invalid_layer;
    invalid_layer.cells                 = NULL;
    tabos_tile_t unchanged              = 123U;
    if (tabos_tilemap_get(&invalid_map, 0U, 0U, 0U, &unchanged) != -1 || errno != EINVAL || unchanged != 123U) {
        return false;
    }
    tabos_tile_t reserved = TABOS_TILE(0U) | TABOS_TILE_RESERVED;
    invalid_layer         = map->layers[0];
    invalid_layer.cells   = &reserved;
    invalid_map.width     = 1U;
    invalid_map.height    = 1U;
    if (tabos_tilemap_draw_layer(graphics, &invalid_map, 0U, sprites, draw) != -1 || errno != EINVAL) {
        return false;
    }
    return true;
}

static bool native_submission(const tabos_sprite_set_t* sprites)
{
    tabos_graphics_t graphics = {0};
    platform_blit_count       = 0U;
    platform_present_count    = 0U;
    if (tabos_graphics_open(&graphics) != 0 || graphics.pixels != NULL || graphics.width != 32U ||
        graphics.height != 32U || tabos_graphics_begin_camera(&graphics, 3, 4) != 0) {
        return false;
    }

    tabos_sprite_draw_options_t draw = TABOS_SPRITE_DRAW_OPTIONS_DEFAULT;
    draw.width                       = 4U;
    draw.height                      = 6U;
    draw.rotation                    = TABOS_GRAPHICS_ROTATE_90;
    draw.mirror_x                    = true;
    draw.mirror_y                    = true;
    draw.opacity                     = 123U;
    draw.clip                        = (tabos_graphics_rect_t) {.x = 1, .y = 2, .width = 20U, .height = 18U};
    draw.clip_enabled                = true;
    if (tabos_sprite_draw_ex(&graphics, sprites, 0U, 10, 11, &draw) != 0 || platform_blit_count != 1U) {
        return false;
    }
    const tabos_graphics_blit_options_t* submitted = &platform_blits[0];
    if (submitted->pixels != sprites->images[0].pixels || submitted->bitmap_width != 2U ||
        submitted->bitmap_height != 2U || submitted->source.x != 0 || submitted->source.y != 0 ||
        submitted->source.width != 2U || submitted->source.height != 2U || submitted->destination.x != 5 ||
        submitted->destination.y != 4 || submitted->destination.width != 4U || submitted->destination.height != 6U ||
        submitted->rotation != TABOS_GRAPHICS_ROTATE_90 || !submitted->mirror_x || !submitted->mirror_y ||
        submitted->opacity != 123U || !submitted->color_key_enabled ||
        submitted->color_key_low != sprites->images[0].color_key ||
        submitted->color_key_high != sprites->images[0].color_key || !submitted->clip_enabled ||
        submitted->clip.x != 1 || submitted->clip.y != 2 || submitted->clip.width != 20U ||
        submitted->clip.height != 18U) {
        return false;
    }

    if (tabos_sprite_animation_draw(&graphics, sprites, 0U, 12, 13, 10U) != 0 ||
        tabos_metasprite_draw(&graphics, sprites, 0U, 20, 20, false, false, 200U) != 0 || platform_blit_count != 4U ||
        platform_blits[1].destination.x != 8 || platform_blits[1].destination.y != 8 ||
        platform_blits[2].opacity != 200U || platform_blits[3].destination.x != 18 ||
        platform_blits[3].destination.y != 15 || platform_blits[3].opacity != 100U ||
        tabos_graphics_end_camera(&graphics) != 0) {
        return false;
    }

    tabos_tile_t cells[96];
    for (size_t index = 0U; index < sizeof(cells) / sizeof(cells[0]); ++index) {
        cells[index] = TABOS_TILE(0U);
    }
    cells[1]                    |= TABOS_TILE_FLIP_HORIZONTAL;
    cells[2]                    |= TABOS_TILE_FLIP_VERTICAL;
    cells[3]                    |= TABOS_TILE_FLIP_HORIZONTAL | TABOS_TILE_FLIP_VERTICAL;
    cells[4]                    |= TABOS_TILE_FLIP_DIAGONAL;
    cells[5]                    |= TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_HORIZONTAL;
    cells[6]                    |= TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_VERTICAL;
    cells[7]                    |= TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_HORIZONTAL | TABOS_TILE_FLIP_VERTICAL;
    tabos_tilemap_layer_t layer  = {.name = "native", .type = TABOS_TILEMAP_LAYER_TILES, .cells = cells};
    const tabos_tilemap_t map    = {
           .width = 12U, .height = 8U, .tile_width = 1U, .tile_height = 1U, .layers = &layer, .layer_count = 1U};
    const tabos_tilemap_draw_options_t map_draw = {
        .viewport     = {.width = 12U, .height = 8U},
        .animation_ms = 10U,
    };
    if (tabos_tilemap_draw_layer(&graphics, &map, 0U, sprites, &map_draw) != 0 || platform_blit_count != 100U) {
        return false;
    }
    const tabos_graphics_blit_options_t* first_tile = &platform_blits[4];
    const tabos_graphics_blit_options_t* last_tile  = &platform_blits[99];
    if (first_tile->source.x != sprites->sprites[1].x || first_tile->source.y != sprites->sprites[1].y ||
        first_tile->destination.x != 0 || first_tile->destination.y != 0 || first_tile->destination.width != 1U ||
        first_tile->destination.height != 1U || !first_tile->clip_enabled || first_tile->clip.width != 12U ||
        first_tile->clip.height != 8U || !platform_blits[5].mirror_x || !platform_blits[6].mirror_y ||
        !platform_blits[7].mirror_x || !platform_blits[7].mirror_y ||
        platform_blits[8].rotation != TABOS_GRAPHICS_ROTATE_90 || !platform_blits[8].mirror_x ||
        platform_blits[9].rotation != TABOS_GRAPHICS_ROTATE_270 ||
        platform_blits[10].rotation != TABOS_GRAPHICS_ROTATE_90 ||
        platform_blits[11].rotation != TABOS_GRAPHICS_ROTATE_90 || !platform_blits[11].mirror_y ||
        last_tile->destination.x != 11 || last_tile->destination.y != 7 || tabos_graphics_present(&graphics) != 0 ||
        platform_blit_count != 100U || platform_present_count != 1U || tabos_graphics_close(&graphics) != 0) {
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    const tabos_color_t key             = TABOS_RGB565(255, 0, 255);
    const tabos_color_t red             = TABOS_RGB565(255, 0, 0);
    const tabos_color_t pixels[]        = {key, red, red, key};
    const tabos_sprite_image_t images[] = {
        {.pixels = pixels, .width = 2U, .height = 2U, .color_key = key, .color_key_enabled = true},
    };
    const tabos_sprite_t sprite_descriptors[] = {
        {.width = 2U, .height = 2U, .pivot_x = 1, .pivot_y = 1, .flags = 4U},
        {.width = 2U, .height = 2U, .pivot_x = 1, .pivot_y = 1},
    };
    const tabos_sprite_frame_t frames[] = {
        {.sprite = 0U, .duration_ms = 10U},
        {.sprite = 1U, .duration_ms = 20U}
    };
    const tabos_sprite_animation_t animations[] = {
        {.frames = frames, .frame_count = 2U, .repeat_count = 2U, .trigger_sprite = 0U}
    };
    const tabos_metasprite_part_t parts[] = {
        {.sprite = 0U, .opacity = 255U},
        {.sprite = 1U, .x = 2, .opacity = 128U}
    };
    const tabos_metasprite_t metasprites[] = {
        {.parts = parts, .part_count = 2U}
    };
    const tabos_sprite_set_t sprites = {.images           = images,
                                        .image_count      = 1U,
                                        .sprites          = sprite_descriptors,
                                        .sprite_count     = 2U,
                                        .animations       = animations,
                                        .animation_count  = 1U,
                                        .metasprites      = metasprites,
                                        .metasprite_count = 1U};
    tabos_graphics_t graphics        = {.width = 16U, .height = 16U};
    if (tabos_graphics_open(&graphics) != 0 || tabos_sprite_flags(&sprites, 0U) != 4U ||
        tabos_sprite_animation_sprite(&sprites, 0U, 0U) != 0U ||
        tabos_sprite_animation_sprite(&sprites, 0U, 10U) != 1U ||
        tabos_sprite_animation_sprite(&sprites, 0U, 60U) != 1U || !transformed_pivots(&graphics, red) ||
        tabos_sprite_draw(&graphics, &sprites, 0U, 4, 4) != 0 || graphics.pixels[3U * graphics.width + 4U] != red ||
        tabos_metasprite_draw(&graphics, &sprites, 0U, 8, 8, true, false, 255U) != 0) {
        return 1;
    }

    tabos_tile_t cells[]           = {TABOS_TILE(0U), TABOS_TILE_EMPTY, TABOS_TILE(1U) | TABOS_TILE_FLIP_HORIZONTAL,
                                      TABOS_TILE(0U) | TABOS_TILE_FLIP_DIAGONAL};
    tabos_tilemap_layer_t layers[] = {
        {.name = "ground", .type = TABOS_TILEMAP_LAYER_TILES, .cells = cells}
    };
    tabos_tilemap_t map = {
        .width = 2U, .height = 2U, .tile_width = 2U, .tile_height = 2U, .layers = layers, .layer_count = 1U};
    tabos_tile_t tile                       = 0U;
    const tabos_tilemap_draw_options_t draw = {
        .viewport = {.width = 5U, .height = 5U},
          .animation_ms = 10U
    };
    if (tabos_graphics_begin_camera(&graphics, -1, -1) != 0) {
        return 1;
    }
    if (tabos_tilemap_get(&map, 0U, 0U, 0U, &tile) != 0 || tile != TABOS_TILE(0U) ||
        tabos_tilemap_set(&map, 0U, 1U, 0U, TABOS_TILE(1U)) != 0 || tabos_tilemap_get(&map, 0U, 2U, 0U, &tile) != -1 ||
        errno != ERANGE || tabos_tilemap_draw_layer(&graphics, &map, 0U, &sprites, &draw) != 0 ||
        !invalid_descriptors(&graphics, &sprites, &map, &draw)) {
        return 1;
    }

    const tabos_color_t green                     = TABOS_RGB565(0, 255, 0);
    const tabos_color_t blue                      = TABOS_RGB565(0, 0, 255);
    const tabos_color_t yellow                    = TABOS_RGB565(255, 255, 0);
    const tabos_color_t transform_pixels[]        = {red, green, blue, yellow};
    const tabos_sprite_image_t transform_images[] = {
        {.pixels = transform_pixels, .width = 2U, .height = 2U}
    };
    const tabos_sprite_t transform_descriptors[] = {
        {.width = 2U, .height = 2U}
    };
    const tabos_sprite_set_t transform_sprites = {
        .images = transform_images, .image_count = 1U, .sprites = transform_descriptors, .sprite_count = 1U};
    tabos_tile_t transform_cells[] = {
        TABOS_TILE(0U),
        TABOS_TILE(0U) | TABOS_TILE_FLIP_HORIZONTAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_VERTICAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_HORIZONTAL | TABOS_TILE_FLIP_VERTICAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_DIAGONAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_HORIZONTAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_VERTICAL,
        TABOS_TILE(0U) | TABOS_TILE_FLIP_DIAGONAL | TABOS_TILE_FLIP_HORIZONTAL | TABOS_TILE_FLIP_VERTICAL,
    };
    tabos_tilemap_layer_t transform_layers[] = {
        {.name = "transforms", .type = TABOS_TILEMAP_LAYER_TILES, .cells = transform_cells}
    };
    const tabos_tilemap_t transform_map = {
        .width = 8U, .height = 1U, .tile_width = 2U, .tile_height = 2U, .layers = transform_layers, .layer_count = 1U};
    const tabos_tilemap_draw_options_t default_draw = TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT;
    if (default_draw.animation_ms != 0U || tabos_graphics_clear(&graphics, 0U) != 0 ||
        tabos_graphics_begin_camera(&graphics, 0, 0) != 0 ||
        tabos_tilemap_draw_layer(&graphics, &transform_map, 0U, &transform_sprites, &default_draw) != 0 ||
        graphics.pixels[0] != red) {
        return 1;
    }
    const tabos_tilemap_draw_options_t transform_draw = {
        .viewport = {.y = 8, .width = 16U, .height = 2U}
    };
    const tabos_color_t expected[] = {
        red,   green, green,  red,    blue,   yellow, yellow, blue,   red,  blue, blue,
        red,   green, yellow, yellow, green,  blue,   yellow, yellow, blue, red,  green,
        green, red,   green,  yellow, yellow, green,  red,    blue,   blue, red,
    };
    if (tabos_graphics_begin_camera(&graphics, 0, -8) != 0) {
        return 1;
    }
    if (tabos_tilemap_draw_layer(&graphics, &transform_map, 0U, &transform_sprites, &transform_draw) != 0) {
        return 1;
    }
    for (uint32_t index = 0U; index < 32U; ++index) {
        const uint32_t x = index % 16U;
        const uint32_t y = 8U + index / 16U;
        if (graphics.pixels[y * graphics.width + x] != expected[index]) {
            fprintf(stderr, "transform pixel %u,%u: got %04x expected %04x\n", x, y,
                    graphics.pixels[y * graphics.width + x], expected[index]);
            return 1;
        }
    }
    if (platform_blit_count != 0U || platform_present_count != 0U || tabos_graphics_present(&graphics) != 0 ||
        platform_blit_count != 1U || platform_present_count != 1U || platform_blit.pixels != graphics.pixels ||
        platform_blit.bitmap_width != 16U || platform_blit.bitmap_height != 16U || platform_blit.source.width != 16U ||
        platform_blit.source.height != 16U || platform_blit.destination.x != 0 || platform_blit.destination.y != 0 ||
        platform_blit.destination.width != 32U || platform_blit.destination.height != 32U ||
        platform_blit.opacity != 255U || tabos_graphics_close(&graphics) != 0) {
        return 1;
    }
    if (!native_submission(&sprites)) {
        return 1;
    }
    if (argc == 3) {
        tabos_sprite_set_t loaded_sprites = {0};
        tabos_tilemap_t loaded_map        = {0};
        if (tabos_sprite_set_load(argv[1], &loaded_sprites) != 0 || loaded_sprites.image_count != 1U ||
            loaded_sprites.sprite_count != 16U || tabos_tilemap_load(argv[2], &loaded_map) != 0 ||
            loaded_map.width == 0U || loaded_map.height == 0U || loaded_map.tile_width == 0U ||
            loaded_map.tile_height == 0U || loaded_map.layer_count != 3U ||
            loaded_map.layers[0].type != TABOS_TILEMAP_LAYER_TILES || loaded_map.layers[0].cells == NULL ||
            loaded_map.layers[2].type != TABOS_TILEMAP_LAYER_OBJECTS) {
            return 1;
        }
        const tabos_tilemap_object_t* grove = NULL;
        for (uint32_t index = 0U; index < loaded_map.layers[2].object_count; ++index) {
            if (loaded_map.layers[2].objects[index].id == 2U) {
                grove = &loaded_map.layers[2].objects[index];
                break;
            }
        }
        int32_t property = 0;
        if (grove == NULL || tabos_tilemap_object_property(grove, "trees", &property) != 0 || property != 3) {
            return 1;
        }
        tabos_tilemap_unload(&loaded_map);
        tabos_sprite_set_unload(&loaded_sprites);
        if (loaded_map._storage != NULL || loaded_sprites._storage != NULL) {
            return 1;
        }
        const char* bad_sprite_path = "/tmp/tabos-sdk-tiles-bad.tsp";
        const char* bad_map_path    = "/tmp/tabos-sdk-tiles-bad.tmap";
        if (!write_bad_asset(argv[1], bad_sprite_path, 16U) ||
            tabos_sprite_set_load(bad_sprite_path, &loaded_sprites) != -1 ||
            !write_bad_asset(argv[2], bad_map_path, 16U) || tabos_tilemap_load(bad_map_path, &loaded_map) != -1) {
            return 1;
        }
        (void) remove(bad_sprite_path);
        (void) remove(bad_map_path);
    }
    return 0;
}
