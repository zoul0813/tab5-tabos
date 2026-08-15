#ifndef TABOS_INTERNAL_FONT_H
#define TABOS_INTERNAL_FONT_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/config/font.h>
#include <tabos/platform/platform.h>

enum {
    FONT_GLYPH_WIDTH = TABOS_FONT_GLYPH_WIDTH,
    FONT_GLYPH_HEIGHT = TABOS_FONT_GLYPH_HEIGHT,
    FONT_GLYPH_COUNT = TABOS_FONT_GLYPH_COUNT,
    FONT_BYTES_PER_ROW = TABOS_FONT_BYTES_PER_ROW,
};

size_t font_glyph_index(unsigned int character);
bool font_draw_char(platform_framebuffer_t *framebuffer, int x, int y, char character,
                        unsigned int scale, platform_pixel_t foreground, platform_pixel_t background);
size_t font_draw_text(platform_framebuffer_t *framebuffer, int x, int y, const char *text,
                          unsigned int scale, platform_pixel_t foreground, platform_pixel_t background);

#endif
