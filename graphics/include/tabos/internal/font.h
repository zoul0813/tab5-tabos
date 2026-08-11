#ifndef TABOS_INTERNAL_FONT_H
#define TABOS_INTERNAL_FONT_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/config/font.h>
#include <tabos/platform/platform.h>

enum {
    TAB_FONT_GLYPH_WIDTH = TABOS_FONT_GLYPH_WIDTH,
    TAB_FONT_GLYPH_HEIGHT = TABOS_FONT_GLYPH_HEIGHT,
    TAB_FONT_GLYPH_COUNT = TABOS_FONT_GLYPH_COUNT,
    TAB_FONT_BYTES_PER_ROW = TABOS_FONT_BYTES_PER_ROW,
};

size_t tab_font_glyph_index(unsigned int character);
bool tab_font_draw_char(tab_framebuffer_t *framebuffer, int x, int y, char character,
                        unsigned int scale, tab_pixel_t foreground, tab_pixel_t background);
size_t tab_font_draw_text(tab_framebuffer_t *framebuffer, int x, int y, const char *text,
                          unsigned int scale, tab_pixel_t foreground, tab_pixel_t background);

#endif
