#include <soccer/visuals.h>
#include <stdlib.h>
#include <string.h>

/* Octagonal distance approximation: bounded integer work, no floating point. */
static uint32_t travel(soccer_vec_t before, soccer_vec_t after)
{
    const int32_t dx = abs(after.x - before.x), dy = abs(after.y - before.y);
    if (dx > 64 * SOCCER_ONE || dy > 64 * SOCCER_ONE) {
        return 0U; /* Restart placement is not running or rolling. */
    }
    const int32_t major = dx > dy ? dx : dy;
    const int32_t minor = dx > dy ? dy : dx;
    return (uint32_t) (major + minor * 3 / 8);
}

static const soccer_player_t* actor(const soccer_game_t* game, unsigned int index)
{
    return index < SOCCER_PLAYER_COUNT ? &game->players[index] : &game->keepers[index - SOCCER_PLAYER_COUNT];
}

void soccer_visuals_reset(soccer_visuals_t* visuals, const soccer_game_t* game)
{
    memset(visuals, 0, sizeof(*visuals));
    for (unsigned int i = 0U; i < SOCCER_VISUAL_ACTORS; ++i) {
        const soccer_player_t* player = actor(game, i);
        visuals->position[i]          = player->position;
        visuals->action_x[i]          = player->facing_x;
        visuals->action_y[i]          = player->facing_y;
        visuals->action[i]            = SOCCER_EVENT_NONE;
    }
    visuals->ball_position = game->ball;
    visuals->mode          = game->mode;
}

void soccer_visuals_step(soccer_visuals_t* visuals, const soccer_game_t* game)
{
    if (game->paused) {
        return;
    }
    if (game->mode != visuals->mode && game->mode != SOCCER_PLAY) {
        soccer_visuals_reset(visuals, game);
        return;
    }
    for (unsigned int i = 0U; i < SOCCER_VISUAL_ACTORS; ++i) {
        const soccer_player_t* player = actor(game, i);
        const uint32_t moved          = travel(visuals->position[i], player->position);
        visuals->moving[i]            = moved != 0U && game->mode == SOCCER_PLAY;
        if (player->slide_ticks == 0U && player->jump_ticks == 0U && player->kick_ticks == 0U) {
            visuals->run_distance[i] = (visuals->run_distance[i] + moved) % (64U * SOCCER_ONE);
        }
        if (player->kick_ticks > visuals->previous_kick[i] && player->slide_ticks == 0U) {
            visuals->action_x[i] = player->facing_x;
            visuals->action_y[i] = player->facing_y;
            visuals->action[i]   = game->event == SOCCER_EVENT_PASS ? SOCCER_EVENT_PASS : SOCCER_EVENT_SHOT;
        }
        visuals->previous_kick[i] = player->kick_ticks;
        visuals->position[i]      = player->position;
    }
    if (game->ball_height <= 4 * SOCCER_ONE && game->keeper_owner < 0) {
        visuals->ball_distance =
            (visuals->ball_distance + travel(visuals->ball_position, game->ball)) % (32U * SOCCER_ONE);
    }
    visuals->ball_position = game->ball;
    visuals->mode          = game->mode;
}
