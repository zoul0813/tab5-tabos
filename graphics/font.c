#include <tabos/internal/font.h>
#include <tabos/internal/font_data.h>

#include <stdint.h>

size_t tab_font_glyph_index(unsigned int character)
{
    return character < TAB_FONT_GLYPH_COUNT ? character : 0U;
}

static const uint8_t *glyph_rows(char character)
{
    const size_t glyph = tab_font_glyph_index((unsigned char)character);
    return tab_font_data + (glyph * TAB_FONT_GLYPH_HEIGHT * TAB_FONT_BYTES_PER_ROW);
}

static void put_pixel(tab_framebuffer_t *framebuffer, int x, int y, tab_pixel_t color)
{
    if (x >= 0 && y >= 0 && (size_t)x < framebuffer->width && (size_t)y < framebuffer->height) {
        framebuffer->pixels[((size_t)y * framebuffer->stride_pixels) + (size_t)x] = color;
    }
}

bool tab_font_draw_char(tab_framebuffer_t *framebuffer, int x, int y, char character,
                        unsigned int scale, tab_pixel_t foreground, tab_pixel_t background)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || scale == 0U) {
        return false;
    }
    const uint8_t *rows = glyph_rows(character);
    for (unsigned int row = 0; row < TAB_FONT_GLYPH_HEIGHT; ++row) {
        for (unsigned int column = 0; column < TAB_FONT_GLYPH_WIDTH; ++column) {
            const size_t byte = row * TAB_FONT_BYTES_PER_ROW + (column / 8U);
            const uint8_t mask = (uint8_t)(0x80U >> (column % 8U));
            const tab_pixel_t color = (rows[byte] & mask) != 0U ? foreground : background;
            for (unsigned int sy = 0; sy < scale; ++sy) {
                for (unsigned int sx = 0; sx < scale; ++sx) {
                    put_pixel(framebuffer, x + (int)(column * scale + sx),
                              y + (int)(row * scale + sy), color);
                }
            }
        }
    }
    return true;
}

size_t tab_font_draw_text(tab_framebuffer_t *framebuffer, int x, int y, const char *text,
                          unsigned int scale, tab_pixel_t foreground, tab_pixel_t background)
{
    if (text == NULL || scale == 0U) {
        return 0U;
    }
    size_t count = 0U;
    const int advance = (int)(TAB_FONT_GLYPH_WIDTH * scale);
    while (*text != '\0') {
        if (!tab_font_draw_char(framebuffer, x + (int)count * advance, y, *text,
                                scale, foreground, background)) {
            break;
        }
        ++count;
        ++text;
    }
    return count;
}
