#ifndef TAB5_DISPLAY_CLEAR_H
#define TAB5_DISPLAY_CLEAR_H

#include <tabos/graphics.h>
#include <stddef.h>

/* Partition a cleared frame around an opaque in-bounds replacement. */
static inline size_t tab5_display_clear_regions(uint32_t width, uint32_t height, const tabos_graphics_rect_t* covered,
                                                tabos_graphics_rect_t regions[4])
{
    if (covered == NULL || covered->x < 0 || covered->y < 0 ||
        (uint64_t) (uint32_t) covered->x + covered->width > width ||
        (uint64_t) (uint32_t) covered->y + covered->height > height) {
        regions[0] = (tabos_graphics_rect_t) {0, 0, width, height};
        return 1U;
    }
    const uint32_t right  = (uint32_t) covered->x + covered->width;
    const uint32_t bottom = (uint32_t) covered->y + covered->height;
    regions[0]            = (tabos_graphics_rect_t) {0, 0, width, (uint32_t) covered->y};
    regions[1]            = (tabos_graphics_rect_t) {0, (int32_t) bottom, width, height - bottom};
    regions[2]            = (tabos_graphics_rect_t) {0, covered->y, (uint32_t) covered->x, covered->height};
    regions[3]            = (tabos_graphics_rect_t) {(int32_t) right, covered->y, width - right, covered->height};
    return 4U;
}
#endif
