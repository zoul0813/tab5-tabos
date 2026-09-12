#include <snake/render.h>

#include <stdio.h>
#include <string.h>

#define BACKGROUND TABOS_RGB565(10, 20, 26)
#define PANEL      TABOS_RGB565(18, 34, 42)
#define GREEN      TABOS_RGB565(116, 232, 128)
#define WHITE      TABOS_RGB565(226, 240, 232)
#define MUTED      TABOS_RGB565(126, 158, 158)
#define RED        TABOS_RGB565(255, 112, 96)

typedef struct {
        char character;
        uint8_t rows[7];
} glyph_t;
static const glyph_t glyphs[] = {
#include "../assets/font5x7.inc"
};

static void text(tabos_graphics_t* graphics, int x, int y, const char* value, unsigned int scale, tabos_color_t color)
{
    while (*value != '\0') {
        const uint8_t* rows = glyphs[0].rows;
        for (size_t i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
            if (glyphs[i].character == *value) {
                rows = glyphs[i].rows;
                break;
            }
        }
        for (unsigned int row = 0U; row < 7U; ++row) {
            for (unsigned int column = 0U; column < 5U; ++column) {
                if ((rows[row] & (1U << (4U - column))) != 0U) {
                    (void) tabos_graphics_fill_rect(graphics, x + (int) (column * scale), y + (int) (row * scale),
                                                    scale, scale, color);
                }
            }
        }
        x += (int) (6U * scale);
        ++value;
    }
}

static void centered(tabos_graphics_t* graphics, int y, const char* value, unsigned int scale, tabos_color_t color)
{
    text(graphics, (SNAKE_WIDTH - (int) (strlen(value) * 6U * scale)) / 2, y, value, scale, color);
}

static void cell(tabos_graphics_t* graphics, uint16_t position, unsigned int inset, tabos_color_t color)
{
    const int x = 64 + (position % SNAKE_COLUMNS) * 16 + (int) inset;
    const int y = 40 + (position / SNAKE_COLUMNS) * 16 + (int) inset;
    (void) tabos_graphics_fill_rect(graphics, x, y, 16U - 2U * inset, 16U - 2U * inset, color);
}

int snake_render(tabos_graphics_t* graphics, const snake_game_t* game, bool sound_enabled)
{
    if (tabos_graphics_clear(graphics, BACKGROUND) != 0) {
        return -1;
    }
    text(graphics, 64, 13, "SNAKE", 2U, GREEN);
    char hud[64];
    (void) snprintf(hud, sizeof(hud), "SCORE %lu   BEST %lu   SPEED %lu   SOUND %s",
                    (unsigned long) snake_game_score(game), (unsigned long) game->best,
                    (unsigned long) ((160U - snake_game_interval(game)) / 10U + 1U), sound_enabled ? "ON" : "OFF");
    text(graphics, 256, 17, hud, 1U, WHITE);
    (void) tabos_graphics_rect(graphics, 62, 38, 516U, 292U, MUTED);
    for (uint16_t i = 0U; i < SNAKE_CAPACITY; ++i) {
        const unsigned int parity = ((unsigned int) i % SNAKE_COLUMNS + (unsigned int) i / SNAKE_COLUMNS) % 2U;
        cell(graphics, i, 0U, parity == 0U ? PANEL : TABOS_RGB565(20, 38, 44));
    }
    if (game->mode != SNAKE_WON) {
        cell(graphics, game->food, 3U, RED);
        const int fx = 64 + (game->food % SNAKE_COLUMNS) * 16;
        const int fy = 40 + (game->food / SNAKE_COLUMNS) * 16;
        (void) tabos_graphics_fill_rect(graphics, fx + 8, fy + 1, 3U, 3U, GREEN);
        (void) tabos_graphics_fill_rect(graphics, fx + 4, fy + 4, 3U, 3U, WHITE);
    }
    for (unsigned int i = game->length; i > 0U; --i) {
        cell(graphics, game->cells[i - 1U], 1U, i == 1U ? GREEN : TABOS_RGB565(52, 164, 112));
    }
    const int hx        = 64 + (game->cells[0] % SNAKE_COLUMNS) * 16;
    const int hy        = 40 + (game->cells[0] / SNAKE_COLUMNS) * 16;
    const bool vertical = game->direction == SNAKE_UP || game->direction == SNAKE_DOWN;
    const int eye_x     = game->direction == SNAKE_LEFT ? 3 : 10;
    const int eye_y     = game->direction == SNAKE_UP ? 3 : 10;
    for (int i = 0; i < 2; ++i) {
        (void) tabos_graphics_fill_rect(graphics, hx + (vertical ? 3 + i * 7 : eye_x),
                                        hy + (vertical ? eye_y : 3 + i * 7), 3U, 3U, BACKGROUND);
    }
    centered(graphics, 343, "ARROWS OR WASD MOVE   P PAUSE   R RESTART   M SOUND   Q QUIT", 1U, MUTED);

    if (game->mode != SNAKE_PLAYING) {
        (void) tabos_graphics_fill_rect(graphics, 128, 104, 384U, 154U, BACKGROUND);
        (void) tabos_graphics_rect(graphics, 128, 104, 384U, 154U, GREEN);
        const char* title  = "SNAKE";
        const char* action = "ENTER OR SPACE TO START";
        const char* detail = "EAT APPLES. KEEP GROWING.";
        if (game->mode == SNAKE_PAUSED) {
            title  = "PAUSED";
            action = "P OR SPACE TO RESUME";
            detail = "TAKE YOUR TIME";
        } else if (game->mode == SNAKE_OVER) {
            title  = "GAME OVER";
            action = "ENTER OR SPACE TO TRY AGAIN";
            detail = "AVOID THE WALLS AND YOUR TAIL";
        } else if (game->mode == SNAKE_WON) {
            title  = "YOU WIN";
            action = "ENTER OR SPACE TO PLAY AGAIN";
            detail = "EVERY CELL. ONE SNAKE.";
        }
        centered(graphics, 124, title, 4U, game->mode == SNAKE_OVER ? RED : GREEN);
        centered(graphics, 170, detail, 1U, WHITE);
        centered(graphics, 202, action, 1U, GREEN);
        centered(graphics, 234, "Q OR ESC TO RETURN TO SHELL", 1U, MUTED);
    }
    return tabos_graphics_present(graphics);
}
