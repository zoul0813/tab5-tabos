#include <soccer/visuals.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    soccer_game_t game = {0};
    game.mode          = SOCCER_PLAY;
    game.keeper_owner  = -1;
    soccer_visuals_t visuals;
    soccer_visuals_reset(&visuals, &game);
    game.players[0].moving = true; /* Intent without displacement must not animate. */
    soccer_visuals_step(&visuals, &game);
    assert(!visuals.moving[0] && visuals.run_distance[0] == 0U);
    game.players[0].position.x += 16 * SOCCER_ONE;
    game.ball.x                += 8 * SOCCER_ONE;
    game.ball.y                -= 8 * SOCCER_ONE; /* Old x+y spin stayed frozen on this diagonal. */
    soccer_game_t before        = game;
    soccer_visuals_step(&visuals, &game);
    assert(memcmp(&before, &game, sizeof(game)) == 0);
    assert(visuals.run_distance[0] == 16U * SOCCER_ONE && visuals.ball_distance > 0U);
    const uint32_t roll = visuals.ball_distance;
    soccer_visuals_step(&visuals, &game);
    assert(!visuals.moving[0] && visuals.ball_distance == roll);
    game.ball_height  = 20 * SOCCER_ONE;
    game.ball.x      += 10 * SOCCER_ONE;
    soccer_visuals_step(&visuals, &game);
    assert(visuals.ball_distance == roll);
    game.players[0].kick_ticks = 10U;
    game.players[0].facing_x   = -1;
    game.event                 = SOCCER_EVENT_PASS;
    soccer_visuals_step(&visuals, &game);
    game.players[0].facing_x   = 1;
    game.players[0].kick_ticks = 9U;
    soccer_visuals_step(&visuals, &game);
    assert(visuals.action_x[0] == -1);
    assert(visuals.action[0] == SOCCER_EVENT_PASS);
    game.players[SOCCER_TEAM_SIZE].position.y += 12 * SOCCER_ONE;
    soccer_visuals_step(&visuals, &game);
    assert(visuals.moving[SOCCER_TEAM_SIZE]);
    assert(visuals.run_distance[SOCCER_TEAM_SIZE] == 12U * SOCCER_ONE);
    soccer_visuals_t frozen = visuals;
    game.paused             = true;
    soccer_visuals_step(&visuals, &game);
    assert(memcmp(&visuals, &frozen, sizeof(visuals)) == 0);
    game.paused                 = false;
    game.mode                   = SOCCER_KICKOFF;
    game.players[0].position.x += 500 * SOCCER_ONE;
    soccer_visuals_step(&visuals, &game);
    assert(visuals.run_distance[0] == 0U && visuals.run_distance[SOCCER_TEAM_SIZE] == 0U &&
           visuals.ball_distance == 0U);
    puts("Soccer visual observer checks passed");
    return 0;
}
