#include <starfall/render.h>

#include <stdio.h>
#include <string.h>

typedef struct { char character; uint8_t rows[7]; } glyph_t;
static const glyph_t glyphs[] = {
#include "../assets/font5x7.inc"
};

#define COLOR_SPACE TABOS_RGB565(2, 5, 18)
#define COLOR_CYAN TABOS_RGB565(48, 224, 255)
#define COLOR_WHITE TABOS_RGB565(240, 248, 255)
#define COLOR_YELLOW TABOS_RGB565(255, 216, 48)
#define COLOR_RED TABOS_RGB565(255, 48, 72)
#define COLOR_MAGENTA TABOS_RGB565(224, 64, 255)
#define COLOR_GREEN TABOS_RGB565(64, 240, 128)

static const uint8_t *glyph_rows(char character)
{
    for (size_t index = 0U; index < sizeof(glyphs) / sizeof(glyphs[0]); ++index)
        if (glyphs[index].character == character) return glyphs[index].rows;
    return glyphs[0].rows;
}

static void draw_text(tabos_graphics_t *graphics, int x, int y, const char *text,
                      unsigned int scale, tabos_color_t color)
{
    while (*text != '\0') {
        const uint8_t *rows = glyph_rows(*text++);
        for (unsigned int row = 0U; row < 7U; ++row)
            for (unsigned int column = 0U; column < 5U; ++column)
                if ((rows[row] & (1U << (4U - column))) != 0U)
                    (void)tabos_graphics_fill_rect(graphics,
                        x + (int)(column * scale), y + (int)(row * scale),
                        scale, scale, color);
        x += (int)(6U * scale);
    }
}

static void centered(tabos_graphics_t *graphics, int y, const char *text,
                     unsigned int scale, tabos_color_t color)
{
    const int width = (int)(strlen(text) * 6U * scale);
    draw_text(graphics, (STARFALL_WIDTH - width) / 2, y, text, scale, color);
}

static void draw_player(tabos_graphics_t *graphics, const starfall_game_t *game)
{
    if (game->tick < game->invulnerable_until && ((game->tick / 4U) & 1U) != 0U) return;
    const int x = game->player_x, y = game->player_y;
    (void)tabos_graphics_fill_rect(graphics, x + 8, y, 4U, 16U, COLOR_WHITE);
    (void)tabos_graphics_fill_rect(graphics, x + 4, y + 6, 12U, 8U, COLOR_CYAN);
    (void)tabos_graphics_fill_rect(graphics, x, y + 11, 20U, 5U, COLOR_MAGENTA);
    (void)tabos_graphics_fill_rect(graphics, x + 7, y + 16, 3U, 5U, COLOR_YELLOW);
    (void)tabos_graphics_fill_rect(graphics, x + 11, y + 16, 3U, 5U, COLOR_RED);
}

static void draw_enemy(tabos_graphics_t *graphics, const starfall_enemy_t *enemy)
{
    const tabos_color_t color = enemy->kind == 0U ? COLOR_GREEN :
        enemy->kind == 1U ? COLOR_YELLOW : COLOR_RED;
    (void)tabos_graphics_fill_rect(graphics, enemy->x + 4, enemy->y, 10U, 4U, color);
    (void)tabos_graphics_fill_rect(graphics, enemy->x, enemy->y + 4, 18U, 6U, color);
    (void)tabos_graphics_fill_rect(graphics, enemy->x + 3, enemy->y + 10, 4U, 4U, color);
    (void)tabos_graphics_fill_rect(graphics, enemy->x + 11, enemy->y + 10, 4U, 4U, color);
    (void)tabos_graphics_fill_rect(graphics, enemy->x + 6, enemy->y + 5, 2U, 2U, COLOR_SPACE);
    (void)tabos_graphics_fill_rect(graphics, enemy->x + 11, enemy->y + 5, 2U, 2U, COLOR_SPACE);
}

int starfall_render(tabos_graphics_t *graphics, const starfall_game_t *game)
{
    if (tabos_graphics_clear(graphics, COLOR_SPACE) != 0) return -1;
    for (unsigned int index = 0U; index < STARFALL_STAR_COUNT; ++index) {
        const starfall_star_t *star = &game->stars[index];
        const tabos_color_t color = star->speed == 1U ? TABOS_RGB565(48, 64, 96) :
            star->speed == 2U ? TABOS_RGB565(112, 144, 192) : COLOR_WHITE;
        (void)tabos_graphics_fill_rect(graphics, star->x, star->y,
                                       star->speed == 3U ? 2U : 1U, star->speed, color);
    }
    (void)tabos_graphics_fill_rect(graphics, 0, 21, STARFALL_WIDTH, 1U,
                                   TABOS_RGB565(24, 72, 112));
    char hud[72];
    (void)snprintf(hud, sizeof(hud), "SCORE %06lu  HI %06lu  LIVES %u  WAVE %lu",
                   (unsigned long)game->score, (unsigned long)game->high_score,
                   game->lives, (unsigned long)game->wave);
    draw_text(graphics, 8, 6, hud, 1U, COLOR_CYAN);

    for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index)
        if (game->shots[index].active)
            (void)tabos_graphics_fill_rect(graphics, game->shots[index].x,
                game->shots[index].y, 3U, 8U, COLOR_YELLOW);
    for (unsigned int index = 0U; index < STARFALL_ENEMY_COUNT; ++index)
        if (game->enemies[index].active) draw_enemy(graphics, &game->enemies[index]);
    for (unsigned int index = 0U; index < STARFALL_PARTICLE_COUNT; ++index)
        if (game->particles[index].active)
            (void)tabos_graphics_fill_rect(graphics, game->particles[index].x,
                game->particles[index].y, 2U, 2U,
                (game->particles[index].life & 1U) != 0U ? COLOR_YELLOW : COLOR_RED);
    if (game->mode == STARFALL_PLAYING) draw_player(graphics, game);

    if (game->mode == STARFALL_TITLE) {
        centered(graphics, 102, "STARFALL", 5U, COLOR_CYAN);
        centered(graphics, 170, "A S MOVE  K FIRE", 2U, COLOR_WHITE);
        centered(graphics, 210, "PRESS K", 2U, COLOR_YELLOW);
        centered(graphics, 244, "Q QUIT", 1U, COLOR_GREEN);
    } else if (game->mode == STARFALL_PAUSED) {
        centered(graphics, 145, "PAUSED", 4U, COLOR_CYAN);
        centered(graphics, 190, "P RESUME", 2U, COLOR_WHITE);
    } else if (game->mode == STARFALL_GAME_OVER) {
        centered(graphics, 130, "GAME OVER", 4U, COLOR_RED);
        centered(graphics, 180, "K RESTART", 2U, COLOR_WHITE);
        centered(graphics, 215, "Q QUIT", 1U, COLOR_GREEN);
    }
    if (game->tick < game->damage_flash_until)
        (void)tabos_graphics_rect(graphics, 2, 23, STARFALL_WIDTH - 4U,
                                  STARFALL_HEIGHT - 25U, COLOR_RED);
    return tabos_graphics_present(graphics);
}
