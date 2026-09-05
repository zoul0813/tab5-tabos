#include <tabos/internal/elf_api.h>
#include <tabos/tilemap.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int graphics_open(uint32_t* width, uint32_t* height)
{
    *width  = 16U;
    *height = 16U;
    return 0;
}

static int graphics_close(void)
{
    return 0;
}

static int graphics_present(void)
{
    return 0;
}

static int graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
    (void) options;
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
        tabos_sprite_animation_frame(&sprites, 0U, 0U) != 0U || tabos_sprite_animation_frame(&sprites, 0U, 10U) != 1U ||
        tabos_sprite_animation_frame(&sprites, 0U, 60U) != 1U || !transformed_pivots(&graphics, red) ||
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
        errno != ERANGE || tabos_tilemap_draw_layer(&graphics, &map, 0U, &sprites, &draw) != 0) {
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
    if (tabos_graphics_close(&graphics) != 0) {
        return 1;
    }
    if (argc == 3) {
        tabos_sprite_set_t loaded_sprites = {0};
        tabos_tilemap_t loaded_map        = {0};
        if (tabos_sprite_set_load(argv[1], &loaded_sprites) != 0 || loaded_sprites.image_count != 1U ||
            loaded_sprites.sprite_count != 16U || tabos_tilemap_load(argv[2], &loaded_map) != 0 ||
            loaded_map.layer_count != 3U || tabos_tilemap_get(&loaded_map, 0U, 0U, 0U, &tile) != 0 ||
            tile == TABOS_TILE_EMPTY) {
            return 1;
        }
        int32_t property = 0;
        if (loaded_map.layers[2].object_count != 3U ||
            tabos_tilemap_object_property(&loaded_map.layers[2].objects[1], "trees", &property) != 0 || property != 3) {
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
