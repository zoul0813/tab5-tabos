#include <tabos/internal/raster.h>

#include <stddef.h>

void raster_fill(platform_framebuffer_t *framebuffer, int32_t x, int32_t y,
                 uint32_t width, uint32_t height, tabos_color_t color)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL) return;
    const int64_t right = (int64_t)x + width;
    const int64_t bottom = (int64_t)y + height;
    const int32_t left = x < 0 ? 0 : x;
    const int32_t top = y < 0 ? 0 : y;
    const int32_t clipped_right = right > (int64_t)framebuffer->width
        ? (int32_t)framebuffer->width : (int32_t)right;
    const int32_t clipped_bottom = bottom > (int64_t)framebuffer->height
        ? (int32_t)framebuffer->height : (int32_t)bottom;
    for (int32_t row = top; row < clipped_bottom; ++row)
        for (int32_t column = left; column < clipped_right; ++column)
            framebuffer->pixels[(size_t)row * framebuffer->stride_pixels + (size_t)column] = color;
}

static bool keyed(tabos_color_t color, tabos_color_t low, tabos_color_t high)
{
    const unsigned int red = (color >> 11U) & 0x1fU;
    const unsigned int green = (color >> 5U) & 0x3fU;
    const unsigned int blue = color & 0x1fU;
    return red >= ((low >> 11U) & 0x1fU) && red <= ((high >> 11U) & 0x1fU) &&
        green >= ((low >> 5U) & 0x3fU) && green <= ((high >> 5U) & 0x3fU) &&
        blue >= (low & 0x1fU) && blue <= (high & 0x1fU);
}

static tabos_color_t blend(tabos_color_t source, tabos_color_t destination, uint8_t opacity)
{
    const unsigned int inverse = 255U - opacity;
    const unsigned int red = ((((source >> 11U) & 0x1fU) * opacity) +
        (((destination >> 11U) & 0x1fU) * inverse) + 127U) / 255U;
    const unsigned int green = ((((source >> 5U) & 0x3fU) * opacity) +
        (((destination >> 5U) & 0x3fU) * inverse) + 127U) / 255U;
    const unsigned int blue = (((source & 0x1fU) * opacity) +
        ((destination & 0x1fU) * inverse) + 127U) / 255U;
    return (tabos_color_t)((red << 11U) | (green << 5U) | blue);
}

bool raster_blit(platform_framebuffer_t *framebuffer,
                 const tabos_graphics_blit_options_t *options)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || options == NULL ||
        options->pixels == NULL || options->bitmap_width == 0U ||
        options->bitmap_height == 0U || options->source.width == 0U ||
        options->source.height == 0U || options->destination.width == 0U ||
        options->destination.height == 0U || options->source.x < 0 || options->source.y < 0 ||
        (uint64_t)(uint32_t)options->source.x + options->source.width > options->bitmap_width ||
        (uint64_t)(uint32_t)options->source.y + options->source.height > options->bitmap_height ||
        options->rotation > TABOS_GRAPHICS_ROTATE_270) return false;

    const uint32_t rotated_width = options->rotation == TABOS_GRAPHICS_ROTATE_90 ||
        options->rotation == TABOS_GRAPHICS_ROTATE_270
        ? options->source.height : options->source.width;
    const uint32_t rotated_height = options->rotation == TABOS_GRAPHICS_ROTATE_90 ||
        options->rotation == TABOS_GRAPHICS_ROTATE_270
        ? options->source.width : options->source.height;
    for (uint32_t dy = 0U; dy < options->destination.height; ++dy) {
        const int64_t output_y = (int64_t)options->destination.y + dy;
        if (output_y < 0 || output_y >= (int64_t)framebuffer->height) continue;
        const uint32_t ry = (uint32_t)((uint64_t)dy * rotated_height /
                                      options->destination.height);
        for (uint32_t dx = 0U; dx < options->destination.width; ++dx) {
            const int64_t output_x = (int64_t)options->destination.x + dx;
            if (output_x < 0 || output_x >= (int64_t)framebuffer->width) continue;
            const uint32_t rx = (uint32_t)((uint64_t)dx * rotated_width /
                                          options->destination.width);
            uint32_t sx = rx, sy = ry;
            if (options->rotation == TABOS_GRAPHICS_ROTATE_90) {
                sx = options->source.width - 1U - ry; sy = rx;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_180) {
                sx = options->source.width - 1U - rx;
                sy = options->source.height - 1U - ry;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_270) {
                sx = ry; sy = options->source.height - 1U - rx;
            }
            if (options->mirror_x) sx = options->source.width - 1U - sx;
            if (options->mirror_y) sy = options->source.height - 1U - sy;
            sx += (uint32_t)options->source.x;
            sy += (uint32_t)options->source.y;
            const tabos_color_t source = options->pixels[(size_t)sy * options->bitmap_width + sx];
            if (options->color_key_enabled &&
                keyed(source, options->color_key_low, options->color_key_high)) continue;
            platform_pixel_t *destination = &framebuffer->pixels[
                (size_t)output_y * framebuffer->stride_pixels + (size_t)output_x];
            *destination = options->opacity == 255U
                ? source : blend(source, *destination, options->opacity);
        }
    }
    return true;
}
