#include <tabos/internal/font.h>

#include <ctype.h>
#include <stdint.h>

typedef struct {
    char character;
    uint8_t rows[TAB_FONT_GLYPH_HEIGHT];
} glyph_t;

/* Original compact 5x7 glyph set. Lowercase intentionally uses uppercase shapes. */
static const glyph_t glyphs[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}}, {'!', {4, 4, 4, 4, 4, 0, 4}},
    {'"', {10, 10, 0, 0, 0, 0, 0}}, {'#', {10, 31, 10, 10, 31, 10, 0}},
    {'%', {17, 2, 4, 8, 17, 0, 0}}, {'&', {12, 18, 12, 21, 18, 13, 0}},
    {'\'', {4, 4, 0, 0, 0, 0, 0}}, {'(', {2, 4, 8, 8, 8, 4, 2}},
    {')', {8, 4, 2, 2, 2, 4, 8}}, {'*', {0, 21, 14, 31, 14, 21, 0}},
    {'+', {0, 4, 4, 31, 4, 4, 0}}, {',', {0, 0, 0, 0, 0, 4, 8}},
    {'-', {0, 0, 0, 31, 0, 0, 0}}, {'.', {0, 0, 0, 0, 0, 0, 4}},
    {'/', {1, 2, 4, 8, 16, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {':', {0, 4, 0, 0, 4, 0, 0}}, {';', {0, 4, 0, 0, 4, 4, 8}},
    {'<', {2, 4, 8, 16, 8, 4, 2}}, {'=', {0, 0, 31, 0, 31, 0, 0}},
    {'>', {8, 4, 2, 1, 2, 4, 8}}, {'?', {14, 17, 1, 2, 4, 0, 4}},
    {'@', {14, 17, 23, 21, 23, 16, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 2, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'[', {14, 8, 8, 8, 8, 8, 14}}, {'\\', {16, 8, 4, 2, 1, 0, 0}},
    {']', {14, 2, 2, 2, 2, 2, 14}}, {'^', {4, 10, 17, 0, 0, 0, 0}},
    {'_', {0, 0, 0, 0, 0, 0, 31}}, {'`', {8, 4, 0, 0, 0, 0, 0}},
    {'{', {2, 4, 4, 8, 4, 4, 2}}, {'|', {4, 4, 4, 4, 4, 4, 4}},
    {'}', {8, 4, 4, 2, 4, 4, 8}}, {'~', {0, 0, 9, 22, 0, 0, 0}},
};

static const uint8_t *glyph_rows(char character)
{
    unsigned char value = (unsigned char)character;
    if (islower(value)) {
        value = (unsigned char)toupper(value);
    }
    for (size_t index = 0; index < sizeof(glyphs) / sizeof(glyphs[0]); ++index) {
        if ((unsigned char)glyphs[index].character == value) {
            return glyphs[index].rows;
        }
    }
    return glyph_rows('?');
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
            const uint8_t mask = (uint8_t)(1U << (TAB_FONT_GLYPH_WIDTH - column - 1U));
            const tab_pixel_t color = (rows[row] & mask) != 0U ? foreground : background;
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
    const int advance = (int)((TAB_FONT_GLYPH_WIDTH + 1U) * scale);
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
