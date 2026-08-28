#include <clock/render.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CLOCK_BACKGROUND TABOS_RGB565(2, 8, 14)
#define CLOCK_PANEL      TABOS_RGB565(5, 18, 28)
#define CLOCK_SEGMENT    TABOS_RGB565(64, 240, 224)
#define CLOCK_GLOW       TABOS_RGB565(12, 72, 72)
#define CLOCK_UNLIT      TABOS_RGB565(7, 32, 38)
#define CLOCK_TEXT       TABOS_RGB565(144, 208, 200)
#define CLOCK_DIVIDER    TABOS_RGB565(12, 52, 60)

enum {
    DIGIT_WIDTH      = 72,
    DIGIT_HEIGHT     = 144,
    DIGIT_THICKNESS  = 12,
    SEGMENT_GAP      = 4,
    DIGIT_PAIR_GAP   = 10,
    COLON_WIDTH      = 12,
    COLON_SIDE_GAP   = 16,
    TIME_FIELD_WIDTH = DIGIT_WIDTH * 6 + DIGIT_PAIR_GAP * 3 + (COLON_SIDE_GAP * 2 + COLON_WIDTH) * 2,
};

typedef struct {
        char character;
        uint8_t rows[7];
} small_glyph_t;

static const small_glyph_t small_glyphs[] = {
    {' ',        {0, 0, 0, 0, 0, 0, 0}},
    {'-',       {0, 0, 0, 31, 0, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1',      {4, 12, 4, 4, 4, 4, 14}},
    {'2',     {14, 17, 1, 2, 4, 8, 31}},
    {'3',     {30, 1, 1, 14, 1, 1, 30}},
    {'4',     {2, 6, 10, 18, 31, 2, 2}},
    {'5',   {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7',       {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9',   {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I',      {14, 4, 4, 4, 4, 4, 14}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S',   {15, 16, 16, 14, 1, 1, 30}},
    {'T',       {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
};

static const uint8_t digit_segments[] = {
    0x3fU, 0x06U, 0x5bU, 0x4fU, 0x66U, 0x6dU, 0x7dU, 0x07U, 0x7fU, 0x6fU,
};

static const uint8_t* glyph_rows(char character)
{
    for (size_t index = 0U; index < sizeof(small_glyphs) / sizeof(small_glyphs[0]); ++index) {
        if (small_glyphs[index].character == character) {
            return small_glyphs[index].rows;
        }
    }
    return small_glyphs[0].rows;
}

static void draw_small_text(tabos_graphics_t* graphics, int x, int y, const char* text, unsigned int scale)
{
    while (*text != '\0') {
        const uint8_t* rows = glyph_rows(*text++);
        for (unsigned int row = 0U; row < 7U; ++row) {
            for (unsigned int column = 0U; column < 5U; ++column) {
                if ((rows[row] & (1U << (4U - column))) != 0U) {
                    (void) tabos_graphics_fill_rect(graphics, x + (int) (column * scale), y + (int) (row * scale),
                                                    scale, scale, CLOCK_TEXT);
                }
            }
        }
        x += (int) (6U * scale);
    }
}

static void draw_segment(tabos_graphics_t* graphics, int x, int y, unsigned int width, unsigned int height, bool active)
{
    if (active) {
        (void) tabos_graphics_fill_rect(graphics, x - 1, y - 1, width + 2U, height + 2U, CLOCK_GLOW);
    }
    (void) tabos_graphics_fill_rect(graphics, x, y, width, height, active ? CLOCK_SEGMENT : CLOCK_UNLIT);
}

static void draw_digit(tabos_graphics_t* graphics, int x, int y, unsigned int digit)
{
    const uint8_t segments              = digit_segments[digit];
    const unsigned int horizontal_width = DIGIT_WIDTH - (DIGIT_THICKNESS + SEGMENT_GAP) * 2U;
    const unsigned int vertical_height  = DIGIT_HEIGHT / 2U - DIGIT_THICKNESS - SEGMENT_GAP * 2U;
    draw_segment(graphics, x + DIGIT_THICKNESS + SEGMENT_GAP, y, horizontal_width, DIGIT_THICKNESS,
                 (segments & 0x01U) != 0U);
    draw_segment(graphics, x + DIGIT_WIDTH - DIGIT_THICKNESS, y + DIGIT_THICKNESS + SEGMENT_GAP, DIGIT_THICKNESS,
                 vertical_height, (segments & 0x02U) != 0U);
    draw_segment(graphics, x + DIGIT_WIDTH - DIGIT_THICKNESS,
                 y + DIGIT_HEIGHT / 2U + DIGIT_THICKNESS / 2U + SEGMENT_GAP, DIGIT_THICKNESS, vertical_height,
                 (segments & 0x04U) != 0U);
    draw_segment(graphics, x + DIGIT_THICKNESS + SEGMENT_GAP, y + DIGIT_HEIGHT - DIGIT_THICKNESS, horizontal_width,
                 DIGIT_THICKNESS, (segments & 0x08U) != 0U);
    draw_segment(graphics, x, y + DIGIT_HEIGHT / 2U + DIGIT_THICKNESS / 2U + SEGMENT_GAP, DIGIT_THICKNESS,
                 vertical_height, (segments & 0x10U) != 0U);
    draw_segment(graphics, x, y + DIGIT_THICKNESS + SEGMENT_GAP, DIGIT_THICKNESS, vertical_height,
                 (segments & 0x20U) != 0U);
    draw_segment(graphics, x + DIGIT_THICKNESS + SEGMENT_GAP, y + DIGIT_HEIGHT / 2U - DIGIT_THICKNESS / 2U,
                 horizontal_width, DIGIT_THICKNESS, (segments & 0x40U) != 0U);
}

static void draw_colon(tabos_graphics_t* graphics, int x, int y)
{
    draw_segment(graphics, x, y + 44, COLON_WIDTH, COLON_WIDTH, true);
    draw_segment(graphics, x, y + 88, COLON_WIDTH, COLON_WIDTH, true);
}

int clock_render(tabos_graphics_t* graphics, const struct tm* calendar)
{
    static const char* const weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    if (graphics == NULL || calendar == NULL || calendar->tm_wday < 0 || calendar->tm_wday > 6) {
        return -1;
    }
    if (tabos_graphics_clear(graphics, CLOCK_BACKGROUND) != 0 ||
        tabos_graphics_fill_rect(graphics, 16, 16, CLOCK_WIDTH - 32U, CLOCK_HEIGHT - 32U, CLOCK_PANEL) != 0) {
        return -1;
    }
    (void) tabos_graphics_fill_rect(graphics, 32, 72, CLOCK_WIDTH - 64U, 1U, CLOCK_DIVIDER);

    char date[32];
    (void) snprintf(date, sizeof(date), "%s %04d-%02d-%02d", weekdays[calendar->tm_wday], calendar->tm_year + 1900,
                    calendar->tm_mon + 1, calendar->tm_mday);
    const int date_width = (int) strlen(date) * 12;
    draw_small_text(graphics, CLOCK_WIDTH - 32 - date_width, 34, date, 2U);

    const unsigned int digits[] = {
        (unsigned int) calendar->tm_hour / 10U, (unsigned int) calendar->tm_hour % 10U,
        (unsigned int) calendar->tm_min / 10U,  (unsigned int) calendar->tm_min % 10U,
        (unsigned int) calendar->tm_sec / 10U,  (unsigned int) calendar->tm_sec % 10U,
    };
    int x       = (CLOCK_WIDTH - TIME_FIELD_WIDTH) / 2;
    const int y = 112;
    for (unsigned int index = 0U; index < 6U; ++index) {
        draw_digit(graphics, x, y, digits[index]);
        x += DIGIT_WIDTH;
        if (index == 0U || index == 2U || index == 4U) {
            x += DIGIT_PAIR_GAP;
        } else if (index == 1U || index == 3U) {
            x += COLON_SIDE_GAP;
            draw_colon(graphics, x, y);
            x += COLON_WIDTH + COLON_SIDE_GAP;
        }
    }
    return tabos_graphics_present(graphics);
}
