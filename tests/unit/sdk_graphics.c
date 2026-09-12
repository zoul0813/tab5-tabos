#include <tabos/graphics.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>

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
static unsigned int blit_count;
static unsigned int fill_count;
static bool fill_bounds_valid;
static int32_t first_fill_x;
static int32_t first_fill_y;
static int32_t last_fill_x;
static int32_t last_fill_y;

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

static int graphics_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    (void) color;
    if (fill_count == 0U) {
        first_fill_x = x;
        first_fill_y = y;
    }
    last_fill_x = x;
    last_fill_y = y;
    ++fill_count;
    if (x < 0 || y < 0 || width == 0U || height == 0U || (uint64_t) (uint32_t) x + width > 1280U ||
        (uint64_t) (uint32_t) y + height > 720U) {
        fill_bounds_valid = false;
    }
    return 0;
}

static uint32_t graphics_capabilities(void)
{
    return TABOS_GRAPHICS_CAP_QUEUED | TABOS_GRAPHICS_CAP_TRANSFORM;
}

static int graphics_blit(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint16_t* pixels)
{
    (void) x;
    (void) y;
    if (width == 1U && height == 1U && pixels != NULL) {
        ++blit_count;
        return 0;
    }
    return -EINVAL;
}

static int graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
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
    .graphics_clear        = graphics_clear,
    .graphics_fill_rect    = graphics_fill_rect,
    .graphics_present      = graphics_present,
    .graphics_close        = graphics_close,
    .graphics_capabilities = graphics_capabilities,
    .graphics_blit         = graphics_blit,
    .graphics_blit_ex      = graphics_blit_ex,
    .graphics_set_overlays = graphics_set_overlays,
};

const tabos_elf_api_t* tabos_runtime_api = &api;

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
    expected_width         = 320U;
    expected_height        = 180U;
    expected_x             = 0;
    expected_y             = 0;
    expected_output_width  = 1280U;
    expected_output_height = 720U;
    if (tabos_graphics_blit_ex(&graphics, &blit) != 0 || tabos_graphics_present(&graphics) != 0 || !upscale_valid ||
        present_count != 2U || clear_count != 1U || tabos_graphics_close(&graphics) != 0 || close_count != 2U ||
        graphics.open) {
        return 1;
    }

    graphics = (tabos_graphics_t) {0};
    if (tabos_graphics_open(&graphics) != 0 || graphics.width != 1280U || graphics.height != 720U ||
        graphics.scale != 1U || graphics.pixels != NULL || tabos_graphics_blit(&graphics, 0, 0, 0U, 1U, sprite) != -1 ||
        errno != EINVAL || blit_count != 0U || tabos_graphics_blit(&graphics, 0, 0, 1U, 0U, sprite) != -1 ||
        errno != EINVAL || blit_count != 0U || tabos_graphics_blit(&graphics, 0, 0, 1U, 1U, sprite) != 0 ||
        blit_count != 1U) {
        return 1;
    }

    fill_bounds_valid = true;
    fill_count        = 0U;
    if (tabos_graphics_line(&graphics, INT32_MIN, 100, INT32_MAX, 100, red) != 0 || !fill_bounds_valid ||
        fill_count != 1280U || first_fill_x != 0 || first_fill_y != 100 || last_fill_x != 1279 || last_fill_y != 100) {
        return 1;
    }
    fill_count = 0U;
    if (tabos_graphics_line(&graphics, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, red) != 0 || !fill_bounds_valid ||
        fill_count != 720U || first_fill_x != 0 || first_fill_y != 0 || last_fill_x != 719 || last_fill_y != 719) {
        return 1;
    }
    fill_count = 0U;
    if (tabos_graphics_line(&graphics, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MIN, red) != 0 || fill_count != 0U ||
        tabos_graphics_line(&graphics, INT32_MAX, 0, INT32_MAX, 719, red) != 0 || fill_count != 0U) {
        return 1;
    }
    if (tabos_graphics_rect(&graphics, INT32_MAX, INT32_MIN, UINT32_MAX, UINT32_MAX, red) != 0 || fill_count != 0U) {
        return 1;
    }
    if (tabos_graphics_rect(&graphics, 10, INT32_MIN, 2U, UINT32_MAX, red) != 0 || !fill_bounds_valid ||
        fill_count != 2U || first_fill_x != 10 || first_fill_y != 0 || last_fill_x != 11 || last_fill_y != 0) {
        return 1;
    }
    fill_count = 0U;
    if (tabos_graphics_rect(&graphics, INT32_MIN, 10, UINT32_MAX, 2U, red) != 0 || !fill_bounds_valid ||
        fill_count != 2U || first_fill_x != 0 || first_fill_y != 10 || last_fill_x != 0 || last_fill_y != 11) {
        return 1;
    }
    if (tabos_graphics_close(&graphics) != 0 || close_count != 3U) {
        return 1;
    }
    return 0;
}
