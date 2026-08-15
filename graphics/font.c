#include <tabos/internal/font.h>
#include <tabos/internal/font_data.h>

#include <stdint.h>

size_t font_glyph_index(unsigned int character)
{
    return character < FONT_GLYPH_COUNT ? character : 0U;
}

static const uint8_t *glyph_rows(char character)
{
    const size_t glyph = font_glyph_index((unsigned char)character);
    return font_data + (glyph * FONT_GLYPH_HEIGHT * FONT_BYTES_PER_ROW);
}

static void put_pixel(platform_framebuffer_t *framebuffer, int x, int y, platform_pixel_t color)
{
    if (x >= 0 && y >= 0 && (size_t)x < framebuffer->width && (size_t)y < framebuffer->height) {
        framebuffer->pixels[((size_t)y * framebuffer->stride_pixels) + (size_t)x] = color;
    }
}

bool font_draw_char(platform_framebuffer_t *framebuffer, int x, int y, char character,
                        unsigned int scale, platform_pixel_t foreground, platform_pixel_t background)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || scale == 0U) {
        return false;
    }
    const uint8_t *rows = glyph_rows(character);
    for (unsigned int row = 0; row < FONT_GLYPH_HEIGHT; ++row) {
        for (unsigned int column = 0; column < FONT_GLYPH_WIDTH; ++column) {
            const size_t byte = row * FONT_BYTES_PER_ROW + (column / 8U);
            const uint8_t mask = (uint8_t)(0x80U >> (column % 8U));
            const platform_pixel_t color = (rows[byte] & mask) != 0U ? foreground : background;
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

size_t font_draw_text(platform_framebuffer_t *framebuffer, int x, int y, const char *text,
                          unsigned int scale, platform_pixel_t foreground, platform_pixel_t background)
{
    if (text == NULL || scale == 0U) {
        return 0U;
    }
    size_t count = 0U;
    const int advance = (int)(FONT_GLYPH_WIDTH * scale);
    while (*text != '\0') {
        if (!font_draw_char(framebuffer, x + (int)count * advance, y, *text,
                                scale, foreground, background)) {
            break;
        }
        ++count;
        ++text;
    }
    return count;
}
