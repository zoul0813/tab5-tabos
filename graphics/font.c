#include <tabos/internal/font.h>
#include <tabos/internal/font_data.h>
#include <tabos/internal/raster.h>

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

bool font_draw_char(platform_framebuffer_t *framebuffer, int x, int y, char character,
                        unsigned int scale, platform_pixel_t foreground, platform_pixel_t background)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || scale == 0U) {
        return false;
    }
    const uint8_t *rows = glyph_rows(character);
    for (unsigned int row = 0; row < FONT_GLYPH_HEIGHT; ++row) {
        for (unsigned int sy = 0; sy < scale; ++sy) {
            const int output_y = y + (int)(row * scale + sy);
            if (output_y < 0 || (size_t)output_y >= framebuffer->height) continue;
            unsigned int column = 0U;
            while (column < FONT_GLYPH_WIDTH) {
                const size_t byte = row * FONT_BYTES_PER_ROW + (column / 8U);
                const uint8_t mask = (uint8_t)(0x80U >> (column % 8U));
                const platform_pixel_t color =
                    (rows[byte] & mask) != 0U ? foreground : background;
                unsigned int run = 1U;
                while (column + run < FONT_GLYPH_WIDTH) {
                    const unsigned int next = column + run;
                    const size_t next_byte = row * FONT_BYTES_PER_ROW + (next / 8U);
                    const uint8_t next_mask = (uint8_t)(0x80U >> (next % 8U));
                    const platform_pixel_t next_color =
                        (rows[next_byte] & next_mask) != 0U ? foreground : background;
                    if (next_color != color) break;
                    ++run;
                }
                int output_x = x + (int)(column * scale);
                size_t count = (size_t)run * scale;
                if (output_x < 0) {
                    const size_t clipped = (size_t)(-output_x);
                    if (clipped >= count) { column += run; continue; }
                    output_x = 0; count -= clipped;
                }
                if ((size_t)output_x < framebuffer->width) {
                    const size_t available = framebuffer->width - (size_t)output_x;
                    if (count > available) count = available;
                    raster_fill_span(framebuffer->pixels +
                        (size_t)output_y * framebuffer->stride_pixels + (size_t)output_x,
                        count, color);
                }
                column += run;
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
