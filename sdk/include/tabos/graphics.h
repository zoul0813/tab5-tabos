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

typedef enum {
    TABOS_GRAPHICS_ROTATE_0 = 0,
    TABOS_GRAPHICS_ROTATE_90 = 1,
    TABOS_GRAPHICS_ROTATE_180 = 2,
    TABOS_GRAPHICS_ROTATE_270 = 3,
} tabos_graphics_rotation_t;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} tabos_graphics_rect_t;

typedef struct {
    const tabos_color_t *pixels;
    uint32_t bitmap_width;
    uint32_t bitmap_height;
    tabos_graphics_rect_t source;
    tabos_graphics_rect_t destination;
    tabos_graphics_rotation_t rotation;
    bool mirror_x;
    bool mirror_y;
    uint8_t opacity;
    bool color_key_enabled;
    tabos_color_t color_key_low;
    tabos_color_t color_key_high;
} tabos_graphics_blit_options_t;

enum {
    TABOS_GRAPHICS_CAP_QUEUED = 1U << 0,
    TABOS_GRAPHICS_CAP_TRANSFORM = 1U << 1,
    TABOS_GRAPHICS_CAP_BLEND = 1U << 2,
    TABOS_GRAPHICS_CAP_COLOR_KEY = 1U << 3,
    TABOS_GRAPHICS_CAP_HARDWARE_ACCELERATED = 1U << 4,
};

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
uint32_t tabos_graphics_capabilities(const tabos_graphics_t *graphics);
int tabos_graphics_blit_ex(tabos_graphics_t *graphics,
                           const tabos_graphics_blit_options_t *options);
int tabos_graphics_present(tabos_graphics_t *graphics);
int tabos_graphics_close(tabos_graphics_t *graphics);

#endif
