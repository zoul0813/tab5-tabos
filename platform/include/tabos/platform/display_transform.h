#ifndef TABOS_PLATFORM_DISPLAY_TRANSFORM_H
#define TABOS_PLATFORM_DISPLAY_TRANSFORM_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/platform/platform.h>

static inline bool platform_framebuffer_rotate_clockwise(const platform_framebuffer_t* source,
                                                         platform_pixel_t* destination, size_t destination_width,
                                                         size_t destination_height)
{
    if (source == NULL || source->pixels == NULL || destination == NULL || source->stride_pixels < source->width ||
        destination_width != source->height || destination_height != source->width) {
        return false;
    }

    for (size_t y = 0; y < source->height; ++y) {
        for (size_t x = 0; x < source->width; ++x) {
            const size_t destination_x = (source->height - 1U) - y;
            const size_t destination_y = x;
            destination[(destination_y * destination_width) + destination_x] =
                source->pixels[(y * source->stride_pixels) + x];
        }
    }

    return true;
}

static inline bool platform_framebuffer_rotate_counter_clockwise(const platform_framebuffer_t* source,
                                                                 platform_pixel_t* destination,
                                                                 size_t destination_width, size_t destination_height)
{
    if (source == NULL || source->pixels == NULL || destination == NULL || source->stride_pixels < source->width ||
        destination_width != source->height || destination_height != source->width) {
        return false;
    }

    for (size_t y = 0; y < source->height; ++y) {
        for (size_t x = 0; x < source->width; ++x) {
            const size_t destination_x = y;
            const size_t destination_y = (source->width - 1U) - x;
            destination[(destination_y * destination_width) + destination_x] =
                source->pixels[(y * source->stride_pixels) + x];
        }
    }

    return true;
}

#endif
