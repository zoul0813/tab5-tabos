#include <starfall/game.h>

#include <string.h>

int main(void)
{
    starfall_game_t game;
    starfall_game_init(&game, 1234U, 900U);
    if (game.mode != STARFALL_TITLE || game.high_score != 900U) return 1;
    starfall_game_start(&game);
    if (game.mode != STARFALL_PLAYING || game.lives != 3U || game.wave != 1U) return 1;

    const int start_x = game.player_x;
    game.input.right = true;
    starfall_game_update(&game);
    if (game.player_x <= start_x) return 1;
    game.input.right = false;
    game.input.fire = true;
    for (unsigned int tick = 0U; tick < 9U; ++tick) starfall_game_update(&game);
    bool found_shot = false;
    for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index)
        if (game.shots[index].active) found_shot = true;
    if (!found_shot) return 1;

    game.input = (starfall_input_t){0};
    game.enemies[0] = (starfall_enemy_t){
        .x = game.player_x, .y = game.player_y, .active = true,
    };
    starfall_game_update(&game);
    if (game.lives != 2U || game.invulnerable_until <= game.tick) return 1;
    game.enemies[0] = (starfall_enemy_t){
        .x = game.player_x, .y = game.player_y, .active = true,
    };
    starfall_game_update(&game);
    if (game.lives != 2U) return 1;

    starfall_game_toggle_pause(&game);
    if (game.mode != STARFALL_PAUSED) return 1;
    const uint32_t paused_tick = game.tick;
    starfall_game_update(&game);
    if (game.tick != paused_tick) return 1;
    starfall_game_toggle_pause(&game);
    if (game.mode != STARFALL_PLAYING) return 1;

    game.score = 1200U;
    game.lives = 1U;
    game.invulnerable_until = 0U;
    game.enemies[0] = (starfall_enemy_t){
        .x = game.player_x, .y = game.player_y, .active = true,
    };
    starfall_game_update(&game);
    if (game.mode != STARFALL_GAME_OVER || game.lives != 0U || game.high_score != 1200U)
        return 1;
    starfall_game_start(&game);
    return game.mode == STARFALL_PLAYING && game.lives == 3U && game.high_score == 1200U
        ? 0 : 1;
}
