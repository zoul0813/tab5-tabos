#ifndef TABOS_GRAPHICS_H
#define TABOS_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t tabos_color_t;

#define TABOS_RGB565(red, green, blue) ((tabos_color_t)( \
    (((uint16_t)(red) & 0xf8U) << 8U) | (((uint16_t)(green) & 0xfcU) << 3U) | \
    ((uint16_t)(blue) >> 3U)))

typedef struct {
    uint32_t width;
    uint32_t height;
    bool open;
} tabos_graphics_t;

int tabos_graphics_open(tabos_graphics_t *graphics);
int tabos_graphics_clear(tabos_graphics_t *graphics, tabos_color_t color);
int tabos_graphics_fill_rect(tabos_graphics_t *graphics, int32_t x, int32_t y,
                             uint32_t width, uint32_t height, tabos_color_t color);
int tabos_graphics_pixel(tabos_graphics_t *graphics, int32_t x, int32_t y,
                         tabos_color_t color);
int tabos_graphics_line(tabos_graphics_t *graphics, int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1, tabos_color_t color);
int tabos_graphics_rect(tabos_graphics_t *graphics, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, tabos_color_t color);
int tabos_graphics_blit(tabos_graphics_t *graphics, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, const tabos_color_t *pixels);
int tabos_graphics_present(tabos_graphics_t *graphics);
int tabos_graphics_close(tabos_graphics_t *graphics);

#endif
