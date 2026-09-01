#include <tabos/graphics.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

extern const tabos_elf_api_t* tabos_runtime_api;

#if defined(TABOS_APPLICATION)
_Static_assert(sizeof(void*) == 4U, "TabOS applications require 32-bit pointers");
_Static_assert(sizeof(tabos_graphics_blit_options_t) == 76U, "graphics ABI layout changed");
#endif

static int result(int value)
{
    if (value < 0) {
        errno = -value;
        return -1;
    }
    return value;
}

static bool valid(const tabos_graphics_t* graphics)
{
    return graphics != NULL && graphics->open && tabos_runtime_api != NULL;
}

static bool scaled(const tabos_graphics_t* graphics)
{
    return graphics != NULL && graphics->pixels != NULL;
}

static bool keyed(tabos_color_t color, tabos_color_t low, tabos_color_t high)
{
    const unsigned int red   = (color >> 11U) & 0x1fU;
    const unsigned int green = (color >> 5U) & 0x3fU;
    const unsigned int blue  = color & 0x1fU;
    return red >= ((low >> 11U) & 0x1fU) && red <= ((high >> 11U) & 0x1fU) && green >= ((low >> 5U) & 0x3fU) &&
           green <= ((high >> 5U) & 0x3fU) && blue >= (low & 0x1fU) && blue <= (high & 0x1fU);
}

static tabos_color_t blend(tabos_color_t source, tabos_color_t destination, uint8_t opacity)
{
    const unsigned int inverse = 255U - opacity;
    const unsigned int red =
        ((((source >> 11U) & 0x1fU) * opacity) + (((destination >> 11U) & 0x1fU) * inverse) + 127U) / 255U;
    const unsigned int green =
        ((((source >> 5U) & 0x3fU) * opacity) + (((destination >> 5U) & 0x3fU) * inverse) + 127U) / 255U;
    const unsigned int blue = (((source & 0x1fU) * opacity) + ((destination & 0x1fU) * inverse) + 127U) / 255U;
    return (tabos_color_t) ((red << 11U) | (green << 5U) | blue);
}

static bool inside_clip(const tabos_graphics_blit_options_t* options, int64_t x, int64_t y)
{
    if (!options->clip_enabled) {
        return true;
    }
    const int64_t right  = (int64_t) options->clip.x + options->clip.width;
    const int64_t bottom = (int64_t) options->clip.y + options->clip.height;
    return x >= options->clip.x && x < right && y >= options->clip.y && y < bottom;
}

int tabos_graphics_open(tabos_graphics_t* graphics)
{
    if (graphics == NULL || tabos_runtime_api == NULL || tabos_runtime_api->graphics_open == NULL) {
        errno = graphics == NULL ? EINVAL : ENOSYS;
        return -1;
    }
    const uint32_t requested_width  = graphics->width;
    const uint32_t requested_height = graphics->height;
    if ((requested_width == 0U) != (requested_height == 0U)) {
        errno = EINVAL;
        return -1;
    }
    uint32_t physical_width = 0U, physical_height = 0U;
    if (result(tabos_runtime_api->graphics_open(&physical_width, &physical_height)) < 0) {
        return -1;
    }
    *graphics = (tabos_graphics_t) {
        .width           = physical_width,
        .height          = physical_height,
        .physical_width  = physical_width,
        .physical_height = physical_height,
        .output_width    = physical_width,
        .output_height   = physical_height,
        .scale           = 1U,
        .open            = true,
    };
    if (requested_width == 0U) {
        return 0;
    }
    if (requested_width > physical_width || requested_height > physical_height ||
        (size_t) requested_width > SIZE_MAX / requested_height ||
        (size_t) requested_width * requested_height > SIZE_MAX / sizeof(tabos_color_t)) {
        (void) tabos_runtime_api->graphics_close();
        *graphics = (tabos_graphics_t) {0};
        errno     = EINVAL;
        return -1;
    }
    tabos_color_t* pixels = calloc((size_t) requested_width * requested_height, sizeof(*pixels));
    if (pixels == NULL) {
        (void) tabos_runtime_api->graphics_close();
        *graphics = (tabos_graphics_t) {0};
        errno     = ENOMEM;
        return -1;
    }
    graphics->width                 = requested_width;
    graphics->height                = requested_height;
    const uint32_t horizontal_scale = physical_width / requested_width;
    const uint32_t vertical_scale   = physical_height / requested_height;
    graphics->scale                 = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
    graphics->output_width          = requested_width * graphics->scale;
    graphics->output_height         = requested_height * graphics->scale;
    graphics->output_x              = (graphics->physical_width - graphics->output_width) / 2U;
    graphics->output_y              = (graphics->physical_height - graphics->output_height) / 2U;
    graphics->pixels                = pixels;
    return 0;
}

int tabos_graphics_set_letterbox_color(tabos_graphics_t* graphics, tabos_color_t color)
{
    if (!valid(graphics) || !scaled(graphics)) {
        errno = EINVAL;
        return -1;
    }
    graphics->letterbox_color = color;
    return 0;
}

tabos_color_t* tabos_graphics_pixels(tabos_graphics_t* graphics)
{
    return valid(graphics) && scaled(graphics) ? graphics->pixels : NULL;
}

int tabos_graphics_clear(tabos_graphics_t* graphics, tabos_color_t color)
{
    if (!valid(graphics)) {
        errno = EINVAL;
        return -1;
    }
    if (scaled(graphics)) {
        const size_t count = (size_t) graphics->width * graphics->height;
        for (size_t index = 0U; index < count; ++index) {
            graphics->pixels[index] = color;
        }
        return 0;
    }
    if (tabos_runtime_api->graphics_clear == NULL) {
        errno = valid(graphics) ? ENOSYS : EINVAL;
        return -1;
    }
    return result(tabos_runtime_api->graphics_clear(color));
}
int tabos_graphics_fill_rect(tabos_graphics_t* graphics, int32_t x, int32_t y, uint32_t width, uint32_t height,
                             tabos_color_t color)
{
    if (!valid(graphics)) {
        errno = EINVAL;
        return -1;
    }
    if (scaled(graphics)) {
        const int64_t right         = (int64_t) x + width;
        const int64_t bottom        = (int64_t) y + height;
        const int32_t left          = x < 0 ? 0 : x;
        const int32_t top           = y < 0 ? 0 : y;
        const int32_t clipped_right = right > (int64_t) graphics->width ? (int32_t) graphics->width : (int32_t) right;
        const int32_t clipped_bottom =
            bottom > (int64_t) graphics->height ? (int32_t) graphics->height : (int32_t) bottom;
        for (int32_t row = top; row < clipped_bottom; ++row) {
            tabos_color_t* destination = graphics->pixels + (size_t) row * graphics->width + (size_t) left;
            for (int32_t column = left; column < clipped_right; ++column) {
                *destination++ = color;
            }
        }
        return 0;
    }
    if (tabos_runtime_api->graphics_fill_rect == NULL) {
        errno = valid(graphics) ? ENOSYS : EINVAL;
        return -1;
    }
    return result(tabos_runtime_api->graphics_fill_rect(x, y, width, height, color));
}

int tabos_graphics_pixel(tabos_graphics_t* graphics, int32_t x, int32_t y, tabos_color_t color)
{
    return tabos_graphics_fill_rect(graphics, x, y, 1U, 1U, color);
}

int tabos_graphics_line(tabos_graphics_t* graphics, int32_t x0, int32_t y0, int32_t x1, int32_t y1, tabos_color_t color)
{
    int32_t dx    = x1 >= x0 ? x1 - x0 : x0 - x1;
    int32_t sx    = x0 < x1 ? 1 : -1;
    int32_t dy    = y1 >= y0 ? y0 - y1 : y1 - y0;
    int32_t sy    = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        if (tabos_graphics_pixel(graphics, x0, y0, color) != 0) {
            return -1;
        }
        if (x0 == x1 && y0 == y1) {
            return 0;
        }
        const int32_t twice_error = error * 2;
        if (twice_error >= dy) {
            error += dy;
            x0    += sx;
        }
        if (twice_error <= dx) {
            error += dx;
            y0    += sy;
        }
    }
}

int tabos_graphics_rect(tabos_graphics_t* graphics, int32_t x, int32_t y, uint32_t width, uint32_t height,
                        tabos_color_t color)
{
    if (width == 0U || height == 0U) {
        return 0;
    }
    if (tabos_graphics_fill_rect(graphics, x, y, width, 1U, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x, y + (int32_t) height - 1, width, 1U, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x, y, 1U, height, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x + (int32_t) width - 1, y, 1U, height, color) != 0) {
        return -1;
    }
    return 0;
}
int tabos_graphics_blit(tabos_graphics_t* graphics, int32_t x, int32_t y, uint32_t width, uint32_t height,
                        const tabos_color_t* pixels)
{
    if (!valid(graphics) || pixels == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (scaled(graphics)) {
        const tabos_graphics_blit_options_t options = {
            .pixels        = pixels,
            .bitmap_width  = width,
            .bitmap_height = height,
            .source        = {.width = width, .height = height},
            .destination   = {.x = x, .y = y, .width = width, .height = height},
            .opacity       = 255U,
        };
        return tabos_graphics_blit_ex(graphics, &options);
    }
    if (tabos_runtime_api->graphics_blit == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return result(tabos_runtime_api->graphics_blit(x, y, width, height, pixels));
}

uint32_t tabos_graphics_capabilities(const tabos_graphics_t* graphics)
{
    if (!valid(graphics) || tabos_runtime_api->graphics_capabilities == NULL) {
        return 0U;
    }
    uint32_t capabilities = tabos_runtime_api->graphics_capabilities();
    if (scaled(graphics)) {
        capabilities |= TABOS_GRAPHICS_CAP_SCALED_CANVAS;
    }
    return capabilities;
}

static int scaled_blit(tabos_graphics_t* graphics, const tabos_graphics_blit_options_t* options)
{
    if (options->bitmap_width == 0U || options->bitmap_height == 0U || options->source.width == 0U ||
        options->source.height == 0U || options->destination.width == 0U || options->destination.height == 0U ||
        options->source.x < 0 || options->source.y < 0 ||
        (uint64_t) (uint32_t) options->source.x + options->source.width > options->bitmap_width ||
        (uint64_t) (uint32_t) options->source.y + options->source.height > options->bitmap_height ||
        options->rotation > TABOS_GRAPHICS_ROTATE_270) {
        errno = EINVAL;
        return -1;
    }

    const uint32_t rotated_width =
        options->rotation == TABOS_GRAPHICS_ROTATE_90 || options->rotation == TABOS_GRAPHICS_ROTATE_270 ?
            options->source.height :
            options->source.width;
    const uint32_t rotated_height =
        options->rotation == TABOS_GRAPHICS_ROTATE_90 || options->rotation == TABOS_GRAPHICS_ROTATE_270 ?
            options->source.width :
            options->source.height;
    for (uint32_t dy = 0U; dy < options->destination.height; ++dy) {
        const int64_t output_y = (int64_t) options->destination.y + dy;
        if (output_y < 0 || output_y >= (int64_t) graphics->height) {
            continue;
        }
        const uint32_t ry = (uint32_t) ((uint64_t) dy * rotated_height / options->destination.height);
        for (uint32_t dx = 0U; dx < options->destination.width; ++dx) {
            const int64_t output_x = (int64_t) options->destination.x + dx;
            if (output_x < 0 || output_x >= (int64_t) graphics->width || !inside_clip(options, output_x, output_y)) {
                continue;
            }
            const uint32_t rx = (uint32_t) ((uint64_t) dx * rotated_width / options->destination.width);
            uint32_t sx       = rx;
            uint32_t sy       = ry;
            if (options->rotation == TABOS_GRAPHICS_ROTATE_90) {
                sx = options->source.width - 1U - ry;
                sy = rx;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_180) {
                sx = options->source.width - 1U - rx;
                sy = options->source.height - 1U - ry;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_270) {
                sx = ry;
                sy = options->source.height - 1U - rx;
            }
            if (options->mirror_x) {
                sx = options->source.width - 1U - sx;
            }
            if (options->mirror_y) {
                sy = options->source.height - 1U - sy;
            }
            sx                         += (uint32_t) options->source.x;
            sy                         += (uint32_t) options->source.y;
            const tabos_color_t source  = options->pixels[(size_t) sy * options->bitmap_width + sx];
            if (options->color_key_enabled && keyed(source, options->color_key_low, options->color_key_high)) {
                continue;
            }
            tabos_color_t* destination = &graphics->pixels[(size_t) output_y * graphics->width + (size_t) output_x];
            *destination = options->opacity == 255U ? source : blend(source, *destination, options->opacity);
        }
    }
    return 0;
}

int tabos_graphics_blit_ex(tabos_graphics_t* graphics, const tabos_graphics_blit_options_t* options)
{
    if (!valid(graphics) || options == NULL || options->pixels == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (scaled(graphics)) {
        return scaled_blit(graphics, options);
    }
    if (tabos_runtime_api->graphics_blit_ex == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return result(tabos_runtime_api->graphics_blit_ex(options));
}
int tabos_graphics_present(tabos_graphics_t* graphics)
{
    if (!valid(graphics)) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api->graphics_present == NULL) {
        errno = ENOSYS;
        return -1;
    }
    if (scaled(graphics)) {
        if (tabos_runtime_api->graphics_blit_ex == NULL) {
            errno = ENOSYS;
            return -1;
        }
        const bool letterboxed =
            graphics->output_width != graphics->physical_width || graphics->output_height != graphics->physical_height;
        if (letterboxed) {
            if (tabos_runtime_api->graphics_clear == NULL) {
                errno = ENOSYS;
                return -1;
            }
            if (result(tabos_runtime_api->graphics_clear(graphics->letterbox_color)) != 0) {
                return -1;
            }
        }
        const tabos_graphics_blit_options_t options = {
            .pixels        = graphics->pixels,
            .bitmap_width  = graphics->width,
            .bitmap_height = graphics->height,
            .source        = {.width = graphics->width, .height = graphics->height},
            .destination =
                {
                              .x      = (int32_t) graphics->output_x,
                              .y      = (int32_t) graphics->output_y,
                              .width  = graphics->output_width,
                              .height = graphics->output_height,
                              },
            .opacity = 255U,
        };
        if (result(tabos_runtime_api->graphics_blit_ex(&options)) != 0) {
            return -1;
        }
    }
    return result(tabos_runtime_api->graphics_present());
}

int tabos_graphics_set_overlays(tabos_graphics_t* graphics, uint32_t flags)
{
    if (!valid(graphics) || (flags & ~(uint32_t) TABOS_GRAPHICS_OVERLAY_ALL) != 0U) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api->graphics_set_overlays == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return result(tabos_runtime_api->graphics_set_overlays(flags));
}

int tabos_graphics_close(tabos_graphics_t* graphics)
{
    if (!valid(graphics)) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api->graphics_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int value = result(tabos_runtime_api->graphics_close());
    if (value == 0) {
        free(graphics->pixels);
        *graphics = (tabos_graphics_t) {0};
    }
    return value;
}
