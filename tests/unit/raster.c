#include <tabos/internal/raster.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    platform_pixel_t* storage = malloc(18U * sizeof(*storage));
    assert(storage != NULL);
    platform_pixel_t* pixels           = storage + 1U;
    platform_framebuffer_t framebuffer = {
        .pixels        = pixels,
        .width         = 4U,
        .height        = 4U,
        .stride_pixels = 4U,
    };
    memset(pixels, 0, 16U * sizeof(*pixels));
    raster_fill(&framebuffer, -1, -1, 3U, 3U, 0xffffU);
    assert(pixels[0] == 0xffffU && pixels[1] == 0xffffU && pixels[4] == 0xffffU);
    assert(pixels[2] == 0U && pixels[8] == 0U);

    for (size_t index = 0U; index < 18U; ++index) {
        storage[index] = 0x5aa5U;
    }
    raster_fill(&framebuffer, -2, 0, 1U, 1U, 1U);
    raster_fill(&framebuffer, 5, 0, 1U, 1U, 2U);
    raster_fill(&framebuffer, 0, -2, 1U, 1U, 3U);
    raster_fill(&framebuffer, 0, 5, 1U, 1U, 4U);
    raster_fill(&framebuffer, INT32_MIN, INT32_MIN, 1U, 1U, 5U);
    raster_fill(&framebuffer, INT32_MAX, INT32_MAX, UINT32_MAX, UINT32_MAX, 6U);
    raster_fill(&framebuffer, 0, 0, 0U, 1U, 7U);
    raster_fill(&framebuffer, 0, 0, 1U, 0U, 8U);
    for (size_t index = 0U; index < 18U; ++index) {
        assert(storage[index] == 0x5aa5U);
    }

    memset(pixels, 0, 16U * sizeof(*pixels));
    raster_fill(&framebuffer, INT32_MIN, INT32_MIN, UINT32_MAX, UINT32_MAX, 0x7befU);
    assert(storage[0] == 0x5aa5U && storage[17] == 0x5aa5U);
    for (size_t index = 0U; index < 16U; ++index) {
        assert(pixels[index] == 0x7befU);
    }

    const tabos_color_t source[]                = {1U, 2U, 3U, 4U};
    const tabos_graphics_blit_options_t rotated = {
        .pixels        = source,
        .bitmap_width  = 2U,
        .bitmap_height = 2U,
        .source        = {.width = 2U, .height = 2U},
        .destination   = {.x = 0, .y = 0, .width = 2U, .height = 2U},
        .rotation      = TABOS_GRAPHICS_ROTATE_90,
        .opacity       = 255U,
    };
    memset(pixels, 0, 16U * sizeof(*pixels));
    assert(raster_blit(&framebuffer, &rotated));
    assert(pixels[0] == 2U && pixels[1] == 4U);
    assert(pixels[4] == 1U && pixels[5] == 3U);

    const tabos_graphics_blit_options_t keyed = {
        .pixels            = source,
        .bitmap_width      = 2U,
        .bitmap_height     = 2U,
        .source            = {.width = 2U, .height = 2U},
        .destination       = {.x = 1, .y = 1, .width = 2U, .height = 2U},
        .opacity           = 255U,
        .color_key_enabled = true,
        .color_key_low     = 2U,
        .color_key_high    = 2U,
    };
    for (size_t index = 0U; index < 16U; ++index) {
        pixels[index] = 9U;
    }
    assert(raster_blit(&framebuffer, &keyed));
    assert(pixels[5] == 1U && pixels[6] == 9U && pixels[9] == 3U && pixels[10] == 4U);

    const tabos_color_t white                   = 0xffffU;
    const tabos_graphics_blit_options_t blended = {
        .pixels        = &white,
        .bitmap_width  = 1U,
        .bitmap_height = 1U,
        .source        = {.width = 1U, .height = 1U},
        .destination   = {.width = 1U, .height = 1U},
        .opacity       = 128U,
    };
    pixels[0] = 0U;
    assert(raster_blit(&framebuffer, &blended));
    assert(pixels[0] == 0x8410U);

    platform_pixel_t spans[] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    raster_copy_span(spans + 1U, spans, 8U);
    const platform_pixel_t shifted[] = {1U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    assert(memcmp(spans, shifted, sizeof(spans)) == 0);
    raster_fill_span(spans + 1U, 7U, 0xa55aU);
    for (size_t index = 1U; index < 8U; ++index) {
        assert(spans[index] == 0xa55aU);
    }
    free(storage);
    return 0;
}
