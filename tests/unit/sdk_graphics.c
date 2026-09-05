#include <tabos/graphics.h>
#include <tabos/internal/elf_api.h>

#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                         \
    do {                                                                         \
        if (!(condition)) {                                                      \
            fprintf(stderr, "camera check line %d: %s\n", __LINE__, #condition); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

static unsigned int close_count;
static unsigned int present_count;
static unsigned int clear_count;
static tabos_color_t cleared_color;
static bool upscale_valid;
static uint32_t expected_width;
static uint32_t expected_height;
static int32_t expected_x;
static int32_t expected_y;
static uint32_t expected_output_width;
static uint32_t expected_output_height;
static uint32_t overlay_flags;
static bool capture_native;
static tabos_graphics_blit_options_t submitted;
static unsigned int submitted_count;
static int32_t submitted_x, submitted_y;

static int graphics_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    (void) width;
    (void) height;
    (void) color;
    submitted_x = x;
    submitted_y = y;
    ++submitted_count;
    return 0;
}

static int graphics_blit(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint16_t* pixels)
{
    (void) pixels;
    return graphics_fill_rect(x, y, width, height, 0U);
}

static int graphics_open(uint32_t* width, uint32_t* height)
{
    *width  = 1280U;
    *height = 720U;
    return 0;
}

static int graphics_close(void)
{
    ++close_count;
    return 0;
}

static int graphics_present(void)
{
    ++present_count;
    return 0;
}

static int graphics_clear(uint32_t color)
{
    ++clear_count;
    cleared_color = (tabos_color_t) color;
    return 0;
}

static uint32_t graphics_capabilities(void)
{
    return TABOS_GRAPHICS_CAP_QUEUED | TABOS_GRAPHICS_CAP_TRANSFORM;
}

static int graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
    if (capture_native) {
        submitted = *options;
        ++submitted_count;
        return 0;
    }
    upscale_valid = options != NULL && options->pixels != NULL && options->bitmap_width == expected_width &&
                    options->bitmap_height == expected_height && options->source.width == expected_width &&
                    options->source.height == expected_height && options->destination.x == expected_x &&
                    options->destination.y == expected_y && options->destination.width == expected_output_width &&
                    options->destination.height == expected_output_height && options->opacity == 255U;
    return upscale_valid ? 0 : -22;
}

static int graphics_set_overlays(uint32_t flags)
{
    overlay_flags = flags;
    return 0;
}

static const tabos_elf_api_t api = {
    .abi_version           = TABOS_ELF_API_VERSION,
    .graphics_open         = graphics_open,
    .graphics_fill_rect    = graphics_fill_rect,
    .graphics_blit         = graphics_blit,
    .graphics_clear        = graphics_clear,
    .graphics_present      = graphics_present,
    .graphics_close        = graphics_close,
    .graphics_capabilities = graphics_capabilities,
    .graphics_blit_ex      = graphics_blit_ex,
    .graphics_set_overlays = graphics_set_overlays,
};

const tabos_elf_api_t* tabos_runtime_api = &api;

static void check_camera(void)
{
    tabos_graphics_t graphics = {.width = 16U, .height = 12U};
    CHECK(tabos_graphics_begin_camera(NULL, 1, 2) == -1 && errno == EINVAL);
    CHECK(tabos_graphics_end_camera(&graphics) == -1 && errno == EINVAL);
    CHECK(tabos_graphics_open(&graphics) == 0);
    const tabos_color_t pixels[]      = {1U, 2U, 3U, 4U};
    tabos_color_t expected[16U * 12U] = {0};
    CHECK(tabos_graphics_begin_camera(&graphics, 10, -5) == 0);
    CHECK(tabos_graphics_fill_rect(&graphics, 11, -4, 2U, 2U, 7U) == 0);
    expected[17] = expected[18] = expected[33] = expected[34] = 7U;
    CHECK(tabos_graphics_pixel(&graphics, 13, -4, 8U) == 0);
    expected[19] = 8U;
    CHECK(tabos_graphics_line(&graphics, 14, -4, 15, -3, 9U) == 0);
    expected[20] = expected[37] = 9U;
    CHECK(tabos_graphics_rect(&graphics, 16, -4, 3U, 3U, 10U) == 0);
    expected[22] = expected[23] = expected[24] = expected[38] = expected[40] = 10U;
    expected[54] = expected[55] = expected[56] = 10U;
    CHECK(tabos_graphics_blit(&graphics, 10, 0, 2U, 2U, pixels) == 0);
    expected[80]                             = 1U;
    expected[81]                             = 2U;
    expected[96]                             = 3U;
    expected[97]                             = 4U;
    const tabos_graphics_blit_options_t blit = {
        .pixels        = pixels,
        .bitmap_width  = 2U,
        .bitmap_height = 2U,
        .source        = {.width = 2U, .height = 2U},
        .destination   = {.x = 13, .y = 0, .width = 2U, .height = 2U},
        .clip          = {.x = 4, .y = 5, .width = 1U, .height = 2U},
        .clip_enabled  = true,
        .opacity       = 255U,
    };
    CHECK(tabos_graphics_blit_ex(&graphics, &blit) == 0);
    CHECK(blit.destination.x == 13 && blit.destination.y == 0);
    expected[84]  = 2U;
    expected[100] = 4U;
    CHECK(tabos_graphics_end_camera(&graphics) == 0);
    CHECK(tabos_graphics_pixel(&graphics, 0, 0, 11U) == 0);
    expected[0] = 11U;
    CHECK(memcmp(expected, graphics.pixels, sizeof(expected)) == 0);
    CHECK(tabos_graphics_begin_camera(&graphics, INT32_MIN, INT32_MAX) == 0);
    CHECK(tabos_graphics_pixel(&graphics, INT32_MAX, 0, 1U) == -1 && errno == ERANGE);
    CHECK(tabos_graphics_blit_ex(&graphics, &blit) == -1 && errno == ERANGE);
    CHECK(memcmp(expected, graphics.pixels, sizeof(expected)) == 0);
    CHECK(tabos_graphics_begin_camera(&graphics, 2, 3) == 0);
    CHECK(graphics.camera_x == 2 && graphics.camera_y == 3);
    CHECK(tabos_graphics_clear(&graphics, 12U) == 0);
    for (size_t i = 0U; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        CHECK(graphics.pixels[i] == 12U);
    }
    expected_width         = 16U;
    expected_height        = 12U;
    expected_x             = 160;
    expected_y             = 0;
    expected_output_width  = 960U;
    expected_output_height = 720U;
    CHECK(tabos_graphics_present(&graphics) == 0 && upscale_valid);
    CHECK(graphics.camera_x == 2 && graphics.camera_y == 3);
    CHECK(tabos_graphics_close(&graphics) == 0);
    CHECK(graphics.camera_x == 0 && graphics.camera_y == 0);

    capture_native = true;
    CHECK(tabos_graphics_open(&graphics) == 0);
    CHECK(tabos_graphics_begin_camera(&graphics, 10, -5) == 0);
    CHECK(tabos_graphics_pixel(&graphics, 13, 0, 7U) == 0);
    CHECK(submitted_x == 3 && submitted_y == 5);
    CHECK(tabos_graphics_blit(&graphics, 13, 0, 2U, 2U, pixels) == 0);
    CHECK(submitted_x == 3 && submitted_y == 5);
    CHECK(tabos_graphics_blit_ex(&graphics, &blit) == 0);
    CHECK(submitted.destination.x == 3 && submitted.destination.y == 5);
    CHECK(submitted.clip.x == 4 && submitted.clip.y == 5 && submitted.source.x == 0);
    CHECK(tabos_graphics_end_camera(&graphics) == 0);
    CHECK(submitted.destination.x == 3 && submitted.destination.y == 5);
    CHECK(tabos_graphics_blit_ex(&graphics, &blit) == 0);
    CHECK(submitted.destination.x == 13 && submitted.destination.y == 0);
    CHECK(tabos_graphics_begin_camera(&graphics, INT32_MIN, 0) == 0);
    const unsigned int before = submitted_count;
    CHECK(tabos_graphics_blit(&graphics, INT32_MAX, 0, 2U, 2U, pixels) == -1 && errno == ERANGE);
    CHECK(tabos_graphics_blit_ex(&graphics, &blit) == -1 && errno == ERANGE);
    CHECK(submitted_count == before);
    CHECK(tabos_graphics_close(&graphics) == 0);
}

int main(void)
{
    tabos_graphics_t graphics = {.width = 320U, .height = 240U};
    const tabos_color_t red   = TABOS_RGB565(255, 0, 0);
    const tabos_color_t blue  = TABOS_RGB565(0, 0, 255);
    tabos_graphics_t invalid  = {.width = 320U};
    if (tabos_graphics_open(&invalid) != -1) {
        return 1;
    }
    if (tabos_graphics_open(&graphics) != 0 || graphics.scale != 3U || graphics.output_x != 160U ||
        graphics.output_y != 0U || graphics.output_width != 960U || graphics.output_height != 720U ||
        graphics.letterbox_color != 0U || tabos_graphics_set_letterbox_color(&graphics, red) != 0) {
        return 1;
    }
    if (tabos_graphics_set_overlays(&graphics, TABOS_GRAPHICS_OVERLAY_WIFI) != 0 ||
        overlay_flags != TABOS_GRAPHICS_OVERLAY_WIFI ||
        tabos_graphics_set_overlays(&graphics, TABOS_GRAPHICS_OVERLAY_ALL << 1U) != -1) {
        return 1;
    }
    expected_width         = 320U;
    expected_height        = 240U;
    expected_x             = 160;
    expected_y             = 0;
    expected_output_width  = 960U;
    expected_output_height = 720U;
    if (tabos_graphics_present(&graphics) != 0 || clear_count != 1U || cleared_color != red || !upscale_valid ||
        tabos_graphics_close(&graphics) != 0 || close_count != 1U) {
        return 1;
    }

    graphics = (tabos_graphics_t) {.width = 320U, .height = 180U};
    if (tabos_graphics_open(&graphics) != 0 || graphics.width != 320U || graphics.height != 180U ||
        graphics.scale != 4U || tabos_graphics_pixels(&graphics) == NULL ||
        (tabos_graphics_capabilities(&graphics) & TABOS_GRAPHICS_CAP_SCALED_CANVAS) == 0U) {
        return 1;
    }
    if (tabos_graphics_clear(&graphics, blue) != 0 || tabos_graphics_fill_rect(&graphics, -2, -2, 4U, 4U, red) != 0 ||
        graphics.pixels[0] != red || graphics.pixels[1] != red || graphics.pixels[2] != blue) {
        return 1;
    }

    const tabos_color_t sprite[]             = {red, blue, blue, red};
    const tabos_graphics_blit_options_t blit = {
        .pixels        = sprite,
        .bitmap_width  = 2U,
        .bitmap_height = 2U,
        .source        = {.width = 2U, .height = 2U},
        .destination   = {.x = 10, .y = 10, .width = 4U, .height = 4U},
        .rotation      = TABOS_GRAPHICS_ROTATE_90,
        .opacity       = 255U,
    };
    const tabos_graphics_blit_options_t clipped = {
        .pixels        = sprite,
        .bitmap_width  = 2U,
        .bitmap_height = 2U,
        .source        = {.width = 2U, .height = 2U},
        .destination   = {.x = 20, .y = 20, .width = 2U, .height = 2U},
        .opacity       = 255U,
        .clip          = {.x = 20, .y = 20, .width = 1U, .height = 1U},
        .clip_enabled  = true,
    };
    expected_width         = 320U;
    expected_height        = 180U;
    expected_x             = 0;
    expected_y             = 0;
    expected_output_width  = 1280U;
    expected_output_height = 720U;
    if (tabos_graphics_blit_ex(&graphics, &blit) != 0 || tabos_graphics_blit_ex(&graphics, &clipped) != 0 ||
        graphics.pixels[20U * graphics.width + 20U] != red || graphics.pixels[20U * graphics.width + 21U] != blue ||
        tabos_graphics_present(&graphics) != 0 || !upscale_valid || present_count != 2U || clear_count != 1U ||
        tabos_graphics_close(&graphics) != 0 || close_count != 2U || graphics.open) {
        return 1;
    }

    graphics = (tabos_graphics_t) {0};
    if (tabos_graphics_open(&graphics) != 0 || graphics.width != 1280U || graphics.height != 720U ||
        graphics.scale != 1U || graphics.pixels != NULL || tabos_graphics_close(&graphics) != 0 || close_count != 3U) {
        return 1;
    }
    check_camera();
    return 0;
}
