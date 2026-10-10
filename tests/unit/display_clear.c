#include "display_clear.h"
#include <assert.h>
#include <string.h>

static void check(const tabos_graphics_rect_t* covered)
{
    unsigned char actual[7][11];
    memset(actual, 0, sizeof(actual));
    tabos_graphics_rect_t regions[4];
    size_t count = tab5_display_clear_regions(11U, 7U, covered, regions);
    for (size_t i = 0U; i < count; ++i) {
        const tabos_graphics_rect_t* r = &regions[i];
        for (uint32_t y = 0U; y < r->height; ++y) {
            for (uint32_t x = 0U; x < r->width; ++x) {
                assert(r->x >= 0 && r->y >= 0);
                assert((uint32_t) r->x + x < 11U && (uint32_t) r->y + y < 7U);
                assert(actual[(uint32_t) r->y + y][(uint32_t) r->x + x]++ == 0U);
            }
        }
    }
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 11; ++x) {
            bool replaced = covered != NULL && x >= covered->x && y >= covered->y &&
                            (uint32_t) (x - covered->x) < covered->width &&
                            (uint32_t) (y - covered->y) < covered->height;
            assert(actual[y][x] == (replaced ? 0U : 1U));
        }
    }
}
int main(void)
{
    check(NULL);
    for (int y = 0; y <= 7; ++y) {
        for (int x = 0; x <= 11; ++x) {
            for (uint32_t height = 0U; height <= 7U - (uint32_t) y; ++height) {
                for (uint32_t width = 0U; width <= 11U - (uint32_t) x; ++width) {
                    tabos_graphics_rect_t r = {x, y, width, height};
                    check(&r);
                }
            }
        }
    }
    tabos_graphics_rect_t invalid = {-1, 0, 1U, 1U}, regions[4];
    assert(tab5_display_clear_regions(11U, 7U, &invalid, regions) == 1U);
    invalid = (tabos_graphics_rect_t) {10, 6, UINT32_MAX, UINT32_MAX};
    assert(tab5_display_clear_regions(11U, 7U, &invalid, regions) == 1U);
    return 0;
}
