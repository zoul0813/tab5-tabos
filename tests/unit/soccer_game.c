#include <stdlib.h>
#include <soccer/game.h>
#include <soccer/camera.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const soccer_input_t idle = {0};
static void steps(soccer_game_t* g, unsigned int count, soccer_input_t input)
{
    for (unsigned int i = 0U; i < count; ++i) {
        soccer_game_step(g, input);
    }
}
/* Preserve the earlier drill scenarios as focused physics/pass regressions.
   Match lifecycle and active goalkeepers are tested separately below. */
static void practice_reset(soccer_game_t* game)
{
    const uint32_t goals = game->goals, shots = game->shots, passes = game->passes;
    const uint32_t completed = game->completed_passes, tackles = game->tackles;
    soccer_game_reset(game);
    game->goals               = goals;
    game->shots               = shots;
    game->passes              = passes;
    game->completed_passes    = completed;
    game->tackles             = tackles;
    game->mode                = SOCCER_PLAY;
    game->owner               = -1;
    game->possession_ticks    = 0U;
    const int positions[]     = {480, 400, 560};
    game->players[0].position = (soccer_vec_t) {(SOCCER_RIGHT - 208) * SOCCER_ONE, positions[goals % 3U] * SOCCER_ONE};
    game->players[1].position =
        (soccer_vec_t) {(SOCCER_RIGHT - 128) * SOCCER_ONE, (positions[goals % 3U] - 100) * SOCCER_ONE};
    game->players[SOCCER_TEAM_SIZE].position      = (soccer_vec_t) {1470 * SOCCER_ONE, 650 * SOCCER_ONE};
    game->players[SOCCER_TEAM_SIZE + 1].position  = (soccer_vec_t) {1510 * SOCCER_ONE, 250 * SOCCER_ONE};
    game->ball                                    = game->players[0].position;
    game->ball.x                                 += 16 * SOCCER_ONE;
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        if (i % SOCCER_TEAM_SIZE >= 2U) {
            game->players[i].position        = (soccer_vec_t) {40 * SOCCER_ONE, 40 * SOCCER_ONE};
            game->players[i].tackle_cooldown = SOCCER_MATCH_TICKS;
        }
    }
    game->keepers[0].position.x = -1000 * SOCCER_ONE;
    game->keepers[1].position.x = -1000 * SOCCER_ONE;
}

static void movement(void)
{
    soccer_game_t a, b;
    soccer_game_init(&a);
    b = a;
    steps(&a, 100U, (soccer_input_t) {.dx = 1});
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    practice_reset(&a);
    b = a;
    steps(&a, 30U, (soccer_input_t) {.dx = -1});
    steps(&b, 30U, (soccer_input_t) {.dx = -1, .dy = -1});
    const int64_t straight = (SOCCER_RIGHT - 208) * SOCCER_ONE - a.players[a.controlled].position.x;
    const int64_t diagonal = (SOCCER_RIGHT - 208) * SOCCER_ONE - b.players[b.controlled].position.x;
    assert(diagonal * diagonal * 2 > straight * straight * 99 / 100);
    assert(diagonal * diagonal * 2 <= straight * straight);
    a.paused = true;
    b        = a;
    steps(&a, 100U, (soccer_input_t) {.dx = 1, .shoot = true});
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    a.paused = false;
    steps(&a, 1000U, (soccer_input_t) {.dx = -1, .dy = -1});
    assert(a.players[a.controlled].position.x == 40 * SOCCER_ONE &&
           a.players[a.controlled].position.y == 40 * SOCCER_ONE);
    steps(&a, 1000U, (soccer_input_t) {.dx = 1, .dy = 1});
    assert(a.players[a.controlled].position.x == (SOCCER_RIGHT - 8) * SOCCER_ONE &&
           a.players[a.controlled].position.y == (SOCCER_BOTTOM - 8) * SOCCER_ONE);
}
static void possession_and_goals(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    practice_reset(&game);
    soccer_game_step(&game, idle);
    assert(game.owner >= 0);
    soccer_game_step(&game, (soccer_input_t) {.dx = 1});
    assert(game.owner >= 0 && game.ball.x > game.players[game.controlled].position.x);
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.shots == 1U && game.owner < 0 && game.velocity.x > 0 && game.players[game.controlled].kick_ticks > 0U);
    soccer_game_step(&game, idle);
    assert(game.owner < 0 && game.pickup_delay > 0U);
    for (unsigned int i = 0U; i < 180U && game.mode == SOCCER_PLAY; ++i) {
        soccer_game_step(&game, idle);
    }
    assert(game.mode == SOCCER_GOAL && game.goals == 1U && game.shots == 1U);
    assert(game.event == SOCCER_EVENT_GOAL);
    steps(&game, 89U, idle);
    assert(game.mode == SOCCER_GOAL && game.goals == 1U);
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_KICKOFF && game.goals == 1U && game.shots == 1U);
    assert(game.kickoff_team == 1U && game.owner == SOCCER_TEAM_SIZE);
    practice_reset(&game);
    assert(game.goals == 1U && game.shots == 1U && game.pickup_delay == 0U);
}
static void free_ball(soccer_game_t* game, int x, int y, int vx, int vy)
{
    soccer_game_init(game);
    practice_reset(game);
    game->players[0].position = (soccer_vec_t) {40 * SOCCER_ONE, 40 * SOCCER_ONE};
    game->ball                = (soccer_vec_t) {x * SOCCER_ONE, y * SOCCER_ONE};
    game->velocity            = (soccer_vec_t) {vx * SOCCER_ONE, vy * SOCCER_ONE};
}
static void physics(void)
{
    soccer_game_t game;
    free_ball(&game, 300, 200, 3, 0);
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.shots == 0U && game.ball.x > 300 * SOCCER_ONE && game.velocity.x < 3 * SOCCER_ONE);
    steps(&game, 500U, idle);
    assert(game.velocity.x == 0 && game.velocity.y == 0);
    free_ball(&game, SOCCER_RIGHT - 8, 100, 9, 0);
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_PLAY); /* Whole ball has not left yet. */
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_GOAL_KICK && game.restart_team == 1U);
    free_ball(&game, 300, SOCCER_BOTTOM - 8, 0, 9);
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_PLAY);
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_THROW_IN && game.restart_team == 1U);
    free_ball(&game, SOCCER_RIGHT - 8, SOCCER_GOAL_TOP, 9, 0);
    soccer_game_step(&game, idle);
    assert(game.velocity.x < 0 && game.goals == 0U);
    free_ball(&game, SOCCER_RIGHT - 8, 480, 9, 0);
    soccer_game_step(&game, idle);
    assert(game.goals == 0U); /* Centre crosses line; whole ball has not yet crossed. */
    soccer_game_step(&game, idle);
    assert(game.goals == 1U && game.mode == SOCCER_GOAL);
}

static void goal_geometry_and_swept_scoring(void)
{
    assert(SOCCER_GOAL_TOP == 408);
    assert(SOCCER_GOAL_BOTTOM == 552);
    assert(SOCCER_GOAL_CENTRE_Y == 480);
    assert(SOCCER_GOAL_COLLISION_RADIUS == 8);
    assert(SOCCER_GOAL_SCORE_TOP == 416);
    assert(SOCCER_GOAL_SCORE_BOTTOM == 544);
    assert(SOCCER_KEEPER_STANDING_TOP == 420);
    assert(SOCCER_KEEPER_STANDING_BOTTOM == 540);
    assert(SOCCER_GOAL_LINE_CROSSING_OFFSET == 4);
    assert(SOCCER_GOAL_LANE_OFFSET == 48);

    soccer_game_t game;
    const int scoring_y[] = {SOCCER_GOAL_CENTRE_Y, SOCCER_GOAL_SCORE_TOP, SOCCER_GOAL_SCORE_BOTTOM};
    for (unsigned int end = 0U; end < 2U; ++end) {
        const int x  = end == 0U ? SOCCER_LEFT + 10 : SOCCER_RIGHT - 10;
        const int vx = end == 0U ? -16 : 16;
        for (unsigned int lane = 0U; lane < sizeof(scoring_y) / sizeof(scoring_y[0]); ++lane) {
            free_ball(&game, x, scoring_y[lane], vx, 0);
            soccer_game_step(&game, idle);
            assert(game.mode == SOCCER_GOAL);
            assert(game.goals == end && game.opponent_goals == 1U - end);
        }

        const int outside_y[] = {SOCCER_GOAL_SCORE_TOP - 1, SOCCER_GOAL_SCORE_BOTTOM + 1};
        for (unsigned int lane = 0U; lane < sizeof(outside_y) / sizeof(outside_y[0]); ++lane) {
            free_ball(&game, x, outside_y[lane], vx, 0);
            soccer_game_step(&game, idle);
            assert(game.mode != SOCCER_GOAL);
            assert(game.goals == 0U && game.opponent_goals == 0U);
        }

        free_ball(&game, x, SOCCER_GOAL_TOP, vx, 0);
        soccer_game_step(&game, idle);
        assert(game.event == SOCCER_EVENT_POST);
        assert(game.velocity.x * vx < 0);
        assert(game.goals == 0U && game.opponent_goals == 0U);

        /* A complete frame can carry the ball across the line; substeps must
           still find both straight and diagonal whole-ball crossings. */
        free_ball(&game, end == 0U ? SOCCER_LEFT + 20 : SOCCER_RIGHT - 20, 470, end == 0U ? -64 : 64, 0);
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_GOAL);
        free_ball(&game, end == 0U ? SOCCER_LEFT + 20 : SOCCER_RIGHT - 20, 470, end == 0U ? -64 : 64, 32);
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_GOAL);
    }
}
static void camera_motion(void)
{
    soccer_game_t game;
    soccer_camera_t camera;
    soccer_game_init(&game);
    practice_reset(&game);
    soccer_camera_reset(&camera, &game);
    const soccer_vec_t initial                  = camera.position;
    game.players[SOCCER_TEAM_SIZE].position     = (soccer_vec_t) {40 * SOCCER_ONE, 800 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE + 1].position = game.players[SOCCER_TEAM_SIZE].position;
    for (unsigned int i = 0U; i < 240U; ++i) {
        soccer_game_step(&game, (soccer_input_t) {.dx = -1, .dy = -1});
        soccer_camera_step(&camera, &game);
    }
    assert(camera.position.x < initial.x && camera.position.y < initial.y);
    assert(camera.position.x >= 0 && camera.position.y >= 0);
    const soccer_vec_t frozen = camera.position;
    game.paused               = true;
    soccer_camera_step(&camera, &game);
    assert(camera.position.x == frozen.x && camera.position.y == frozen.y);
    game.paused     = false;
    game.owner      = -1;
    game.ball       = (soccer_vec_t) {SOCCER_RIGHT * SOCCER_ONE, 480 * SOCCER_ONE};
    game.velocity.x = 9 * SOCCER_ONE;
    soccer_camera_step(&camera, &game);
    /* A reversal now brakes the old scrolling velocity before moving right. */
    for (unsigned int tick = 0U; tick < 12U; ++tick) {
        soccer_camera_step(&camera, &game);
    }
    assert(camera.position.x > frozen.x);
    for (unsigned int i = 0U; i < 240U; ++i) {
        soccer_camera_step(&camera, &game);
    }
    assert(camera.position.x <= (SOCCER_RIGHT + 64 - SOCCER_VIEW_WIDTH) * SOCCER_ONE);
    practice_reset(&game);
    soccer_camera_reset(&camera, &game);
    assert(camera.position.x == initial.x && camera.position.y == initial.y);
}
static void camera_selection_stability(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_camera_t camera;
    soccer_camera_reset(&camera, &game);
    const soccer_vec_t origin = camera.position;
    game.controlled           = 4U;
    game.players[4].position  = (soccer_vec_t) {40 * SOCCER_ONE, 40 * SOCCER_ONE};
    game.players[4].facing_x  = -1;
    soccer_camera_step(&camera, &game);
    assert(camera.position.x == origin.x && camera.position.y == origin.y);
    game.ball = (soccer_vec_t) {1500 * SOCCER_ONE, 850 * SOCCER_ONE};
    for (unsigned int tick = 0U; tick < 120U; ++tick) {
        const soccer_vec_t before = camera.position;
        soccer_camera_step(&camera, &game);
        assert(abs(camera.position.x - before.x) <= 8 * SOCCER_ONE);
        assert(abs(camera.position.y - before.y) <= 8 * SOCCER_ONE);
    }
    assert(camera.position.x > origin.x && camera.position.y > origin.y);
}

static void passing(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    practice_reset(&game);
    soccer_game_step(&game, idle);
    const soccer_vec_t start = game.ball;
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.passes == 1U && game.shots == 0U && game.owner < 0);
    assert(game.pass_target == 1 && game.controlled == 0U);
    assert(game.event == SOCCER_EVENT_PASS && game.feedback_ticks == 30U);
    assert(game.ball.x > start.x && game.ball.x < game.players[1].position.x);
    for (unsigned int i = 0U; i < 120U && game.owner < 0; ++i) {
        soccer_game_step(&game, idle);
    }
    assert(game.owner == 1 && game.controlled == 1U && game.completed_passes == 1U);
    /* Manual switching changes control, never ownership or ball position. */
    soccer_game_step(&game, idle);
    const soccer_vec_t received = game.ball;
    game.controlled             = SOCCER_TEAM_SIZE - 1U;
    soccer_game_step(&game, (soccer_input_t) {.switch_player = true});
    assert(game.controlled == 0U && game.owner == 1);
    assert(game.ball.x == received.x && game.ball.y == received.y);
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.pass_target == 0 && game.owner < 0);
    for (unsigned int i = 0U; i < 120U && game.owner < 0; ++i) {
        soccer_game_step(&game, idle);
    }
    assert(game.owner == 0 && game.completed_passes == 2U);
    practice_reset(&game);
    assert(game.owner < 0 && game.controlled == 0U && game.pass_target == -1);
    assert(game.passes == 2U && game.completed_passes == 2U);
    soccer_game_step(&game, idle);
    soccer_game_step(&game, (soccer_input_t) {.dx = -1, .pass = true});
    assert(game.pass_target == -1 && game.velocity.x < 0);
    practice_reset(&game);
    const soccer_vec_t teammate = game.players[1].position;
    steps(&game, 100U, (soccer_input_t) {.dx = -1, .dy = 1});
    assert(game.players[1].position.x < teammate.x && game.players[1].position.y < game.ball.y);
    game.paused                = true;
    const soccer_game_t frozen = game;
    soccer_game_step(&game, (soccer_input_t) {.pass = true, .switch_player = true});
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
}
static void opposition(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    practice_reset(&game);
    soccer_game_step(&game, idle);
    const soccer_vec_t defender = game.players[SOCCER_TEAM_SIZE].position;
    steps(&game, 30U, idle);
    assert(game.players[SOCCER_TEAM_SIZE].position.y < defender.y);
    /* A stationary carrier eventually loses possession to the pressing defender. */
    for (unsigned int i = 0U; i < 300U && game.owner < SOCCER_TEAM_SIZE; ++i) {
        soccer_game_step(&game, idle);
    }
    assert(game.owner >= SOCCER_TEAM_SIZE && game.controlled < SOCCER_TEAM_SIZE);
    const int owner       = game.owner;
    const int32_t upfield = game.players[owner].position.x;
    const uint32_t passes = game.passes;
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.owner == owner && game.passes == passes);
    steps(&game, 30U, idle);
    assert(game.players[owner].position.x < upfield);

    /* A facing, in-range tackle wins possession, never shoots on the same press. */
    game.owner                      = owner;
    game.players[0].position        = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[0].facing_x        = 1;
    game.players[0].facing_y        = 0;
    game.players[0].tackle_cooldown = 0U;
    game.players[owner].position    = (soccer_vec_t) {824 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.possession_ticks           = 0U;
    game.controlled                 = 0U;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.owner == 0 && game.tackles == 1U && game.shots == 0U);
    assert(game.event == SOCCER_EVENT_TACKLE);
    assert(game.possession_ticks > 0U && game.players[0].kick_ticks > 0U);
    steps(&game, 15U, idle);
    assert(game.owner == 0); /* No immediate possession ping-pong. */

    /* Once the slide ends, moving into range during recovery cannot steal. */
    practice_reset(&game);
    game.owner               = SOCCER_TEAM_SIZE;
    game.possession_ticks    = 0U;
    game.players[0].position = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.owner == SOCCER_TEAM_SIZE && game.players[0].tackle_cooldown > 0U);
    steps(&game, 18U, idle);
    game.players[SOCCER_TEAM_SIZE].position    = game.players[0].position;
    game.players[SOCCER_TEAM_SIZE].position.x += 20 * SOCCER_ONE;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.owner == SOCCER_TEAM_SIZE);
    game.players[0].tackle_cooldown = 0U;
    game.players[0].facing_x        = -1;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.owner == SOCCER_TEAM_SIZE); /* Must turn toward the carrier. */

    /* An opponent can intercept an assisted pass without becoming controlled or
       incrementing completed passes. */
    practice_reset(&game);
    soccer_game_step(&game, idle);
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    game.players[SOCCER_TEAM_SIZE].position = game.ball;
    soccer_game_step(&game, idle);
    assert(game.owner == SOCCER_TEAM_SIZE && game.controlled == 0U);
    assert(game.pass_target == -1 && game.pass_ticks == 0U && game.completed_passes == 0U);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 60U, (soccer_input_t) {.shoot = true});
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
    practice_reset(&game);
    assert(game.possession_ticks == 0U && game.players[SOCCER_TEAM_SIZE].tackle_cooldown == 0U);
}

static void matches(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    soccer_game_reset(&game);
    assert(game.mode == SOCCER_KICKOFF && game.match_ticks == SOCCER_MATCH_TICKS);
    const soccer_vec_t centre = game.ball;
    steps(&game, 89U, (soccer_input_t) {.dx = 1, .shoot = true});
    assert(game.mode == SOCCER_KICKOFF && game.ball.x == centre.x && game.ball.y == centre.y);
    assert(game.match_ticks == SOCCER_MATCH_TICKS && game.shots == 0U);
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_PLAY && game.owner == 0);
    assert(game.event == SOCCER_EVENT_WHISTLE);
    soccer_game_step(&game, idle);
    assert(game.match_ticks == SOCCER_MATCH_TICKS - 1U);
    assert(game.event == SOCCER_EVENT_NONE);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 120U, idle);
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
    game.paused      = false;
    game.match_ticks = 1U;
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_FULL_TIME && game.match_ticks == 0U);
    assert(game.event == SOCCER_EVENT_FINISH);
    const soccer_game_t finished = game;
    steps(&game, 120U, (soccer_input_t) {.dx = 1, .shoot = true});
    assert(memcmp(&game, &finished, sizeof(game)) == 0);

    /* Both goal lines use whole-ball crossing. The conceding team restarts;
       goals on the final live tick still count before full time. */
    for (unsigned int end = 0U; end < 2U; ++end) {
        free_ball(&game, end == 0U ? SOCCER_LEFT + 8 : SOCCER_RIGHT - 8, 480, end == 0U ? -9 : 9, 0);
        game.goals       = 0U;
        game.match_ticks = 120U;
        steps(&game, 2U, idle);
        assert(game.mode == SOCCER_GOAL && game.kickoff_team == end);
        assert(game.goals == end && game.opponent_goals == 1U - end);
        const unsigned int remaining = game.match_ticks;
        steps(&game, 90U, idle);
        assert(game.mode == SOCCER_KICKOFF && game.owner == (int) end * SOCCER_TEAM_SIZE);
        assert(game.match_ticks == remaining);
        steps(&game, 90U, idle);
        assert(game.mode == SOCCER_PLAY && game.match_ticks == remaining);
    }
    free_ball(&game, SOCCER_RIGHT - 1, 480, 9, 0);
    game.match_ticks = 1U;
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_GOAL && game.goals == 1U && game.match_ticks == 0U);
    steps(&game, 90U, idle);
    assert(game.mode == SOCCER_FULL_TIME);
    soccer_game_reset(&game);
    assert(game.goals == 0U && game.opponent_goals == 0U && game.match_ticks == SOCCER_MATCH_TICKS);
    /* Run the complete match, including autonomous attacks and all restarts.
       Bounded completion guards against stuck keeper holds or goal loops. */
    for (unsigned int i = 0U; i < 15000U && game.mode != SOCCER_FULL_TIME; ++i) {
        soccer_game_step(&game, idle);
        assert(game.controlled < SOCCER_TEAM_SIZE && game.owner >= -1 && game.owner < SOCCER_PLAYER_COUNT);
        assert(game.keeper_owner >= -1 && game.keeper_owner < 2);
        assert(game.keeper_owner < 0 || game.owner == -1);
        for (unsigned int player = 0U; player < SOCCER_PLAYER_COUNT; ++player) {
            assert(game.players[player].position.x >= (SOCCER_LEFT + 8) * SOCCER_ONE);
            assert(game.players[player].position.x <= (SOCCER_RIGHT - 8) * SOCCER_ONE);
            assert(game.players[player].position.y >= (SOCCER_TOP + 8) * SOCCER_ONE);
            assert(game.players[player].position.y <= (SOCCER_BOTTOM - 8) * SOCCER_ONE);
        }
    }
    assert(game.mode == SOCCER_FULL_TIME && game.match_ticks == 0U);
}

static void keeping(void)
{
    soccer_game_t game;
    for (unsigned int end = 0U; end < 2U; ++end) {
        soccer_game_reset(&game);
        game.mode            = SOCCER_PLAY;
        game.owner           = -1;
        const int direction  = end == 0U ? -1 : 1;
        game.ball            = game.keepers[end].position;
        game.ball.x         -= direction * 20 * SOCCER_ONE;
        game.velocity        = (soccer_vec_t) {direction * 9 * SOCCER_ONE, 0};
        soccer_game_step(&game, idle);
        assert(game.keeper_owner == (int) end && game.velocity.x == 0);
        assert(game.event == SOCCER_EVENT_SAVE);
        assert(game.goals == 0U && game.opponent_goals == 0U && game.owner == -1);
        steps(&game, 44U, (soccer_input_t) {.pass = true, .shoot = true});
        assert(game.keeper_owner == (int) end);
        soccer_game_step(&game, idle);
        assert(game.keeper_owner == -1 && game.velocity.x * direction < 0);
        assert(game.keepers[end].tackle_cooldown > 0U && game.controlled < SOCCER_TEAM_SIZE);
        /* A keeper cannot teleport across the mouth to stop a well-placed shot. */
        soccer_game_reset(&game);
        game.mode                    = SOCCER_PLAY;
        game.owner                   = -1;
        game.keepers[end].position.y = (SOCCER_GOAL_TOP + 12) * SOCCER_ONE;
        game.ball = (soccer_vec_t) {(end == 0U ? SOCCER_LEFT + 36 : SOCCER_RIGHT - 36) * SOCCER_ONE, 510 * SOCCER_ONE};
        game.velocity = (soccer_vec_t) {direction * 9 * SOCCER_ONE, 0};
        steps(&game, 6U, idle);
        assert(game.mode == SOCCER_GOAL);
    }
    soccer_game_reset(&game);
    game.mode                               = SOCCER_PLAY;
    game.owner                              = SOCCER_TEAM_SIZE;
    game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {(SOCCER_LEFT + 180) * SOCCER_ONE, 458 * SOCCER_ONE};
    game.ball                               = game.players[SOCCER_TEAM_SIZE].position;
    soccer_game_step(&game, idle);
    assert(game.owner == -1 && game.velocity.x < 0 && game.shots == 0U);
}

static void tactics(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    assert(SOCCER_PLAYER_COUNT == 10 && SOCCER_TEAM_SIZE == 5);
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        assert(game.players[i].position.x > SOCCER_LEFT * SOCCER_ONE);
        assert(game.players[i].position.x < SOCCER_RIGHT * SOCCER_ONE);
    }
    /* Two midfielders and two defenders support the lone striker. */
    for (unsigned int i = 1U; i < SOCCER_DEFENDER_FIRST; ++i) {
        assert(game.players[i].position.x > 400 * SOCCER_ONE && game.players[i].position.x < 700 * SOCCER_ONE);
    }
    for (unsigned int i = SOCCER_DEFENDER_FIRST; i < SOCCER_TEAM_SIZE; ++i) {
        assert(game.players[i].position.x < 400 * SOCCER_ONE);
        for (unsigned int j = i + 1U; j < SOCCER_TEAM_SIZE; ++j) {
            assert(game.players[i].position.y != game.players[j].position.y);
        }
    }
    steps(&game, 90U, idle);
    for (unsigned int i = 1U; i <= SOCCER_TEAM_SIZE; ++i) {
        soccer_game_step(&game, (soccer_input_t) {.switch_player = true});
        assert(game.controlled == i % SOCCER_TEAM_SIZE && game.owner == 0);
    }
    steps(&game, 30U, idle);
    assert(game.players[1].position.y < game.ball.y);
    assert(game.players[2].position.y > game.ball.y);
    assert(game.players[3].position.x < game.ball.x && game.players[4].position.x < game.ball.x);

    /* A marked straight pass loses to an open diagonal option. */
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position =
            (soccer_vec_t) {100 * SOCCER_ONE, (100 + (int) (i % SOCCER_TEAM_SIZE) * 70) * SOCCER_ONE};
    }
    game.players[0].position                = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                               = game.players[0].position;
    game.players[1].position                = (soccer_vec_t) {960 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[2].position                = (soccer_vec_t) {920 * SOCCER_ONE, 590 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {870 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.pass_target == 2 && game.owner == -1);
    game.players[2].position = game.ball;
    soccer_game_step(&game, idle);
    assert(game.owner == 2 && game.controlled == 2U && game.completed_passes == 1U);

    /* Defensive switching chooses the closest alternative; recovery selects a blue
       player automatically, including a defender beyond the original two slots. */
    game.owner                  = SOCCER_TEAM_SIZE;
    game.ball                   = game.players[SOCCER_TEAM_SIZE].position;
    game.players[4].position    = game.ball;
    game.players[4].position.y += 40 * SOCCER_ONE;
    soccer_game_step(&game, (soccer_input_t) {.switch_player = true});
    assert(game.controlled == 4U);
    game.owner                                 = -1;
    game.last_kicker                           = -1;
    game.players[4].position                   = game.ball;
    game.players[SOCCER_TEAM_SIZE].position.x += 100 * SOCCER_ONE;
    soccer_game_step(&game, idle);
    assert(game.owner == 4 && game.controlled == 4U);

    /* Red uses a reachable outlet under pressure. Its reception must never
       transfer human control or increment blue pass statistics. */
    soccer_game_reset(&game);
    game.mode                                   = SOCCER_PLAY;
    game.owner                                  = SOCCER_TEAM_SIZE;
    game.possession_ticks                       = 0U;
    game.players[0].position                    = (soccer_vec_t) {850 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE].position     = (soccer_vec_t) {900 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE + 1].position = (soccer_vec_t) {760 * SOCCER_ONE, 370 * SOCCER_ONE};
    game.ball                                   = game.players[SOCCER_TEAM_SIZE].position;
    soccer_game_step(&game, idle);
    assert(game.owner == -1 && game.pass_target >= SOCCER_TEAM_SIZE && game.passes == 0U);
    const int receiver              = game.pass_target;
    game.players[receiver].position = game.ball;
    soccer_game_step(&game, idle);
    assert(game.owner == receiver && game.controlled == 0U && game.completed_passes == 0U);
}

static void expanded_squad_reception(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    const unsigned int receiver     = SOCCER_TEAM_SIZE - 1U;
    game.players[receiver].position = (soccer_vec_t) {900 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.pass_target == (int) receiver);
    for (unsigned int i = 0U; i < 120U && game.owner < 0; ++i) {
        soccer_game_step(&game, idle);
    }
    assert(game.owner == (int) receiver && game.controlled == receiver && game.completed_passes == 1U);
}

static void action_feedback(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    practice_reset(&game);
    game.players[0].position = (soccer_vec_t) {(SOCCER_RIGHT - 16) * SOCCER_ONE, SOCCER_GOAL_TOP * SOCCER_ONE};
    game.owner               = 0;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.event == SOCCER_EVENT_POST && game.feedback == SOCCER_EVENT_POST);
    assert(game.shots == 1U); /* Post feedback takes priority over the shot this tick. */
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 40U, idle);
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
    soccer_game_reset(&game);
    assert(game.event == SOCCER_EVENT_NONE && game.feedback_ticks == 0U);
}

static void live_ball(soccer_game_t* game, int x, int y, int vx, int vy, unsigned int touch)
{
    soccer_game_reset(game);
    game->mode            = SOCCER_PLAY;
    game->owner           = -1;
    game->last_touch_team = touch;
    game->ball            = (soccer_vec_t) {x * SOCCER_ONE, y * SOCCER_ONE};
    game->velocity        = (soccer_vec_t) {vx * SOCCER_ONE, vy * SOCCER_ONE};
}

static void restarts(void)
{
    soccer_game_t game;
    for (unsigned int team = 0U; team < 2U; ++team) {
        for (unsigned int side = 0U; side < 2U; ++side) {
            live_ball(&game, 800, side == 0U ? SOCCER_TOP - 2 : SOCCER_BOTTOM + 2, 0, side == 0U ? -9 : 9, team);
            soccer_game_step(&game, idle);
            assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_THROW_IN);
            assert(game.restart_team == 1U - team && game.last_touch_team == team);
            assert(game.event == SOCCER_EVENT_WHISTLE && game.velocity.y == 0);
            const unsigned int remaining = game.match_ticks;
            const soccer_vec_t spot      = game.ball;
            game.paused                  = true;
            const soccer_game_t frozen   = game;
            steps(&game, 60U, (soccer_input_t) {.pass = true});
            assert(memcmp(&game, &frozen, sizeof(game)) == 0);
            game.paused = false;
            steps(&game, 89U, (soccer_input_t) {.pass = true});
            assert(game.mode == SOCCER_RESTART && game.ball.x == spot.x && game.ball.y == spot.y);
            assert(game.match_ticks == remaining);
            soccer_game_step(&game, (soccer_input_t) {.pass = true});
            assert(game.mode == SOCCER_PLAY && game.match_ticks == remaining);
            assert(game.last_touch_team == 1U - team && game.pass_target >= 0);
            assert(game.velocity.y * (side == 0U ? 1 : -1) > 0);
        }
        /* Each end: last touched by defender means corner, by attacker means goal kick. */
        const int x  = team == 0U ? SOCCER_LEFT - 2 : SOCCER_RIGHT + 2;
        const int vx = team == 0U ? -9 : 9;
        live_ball(&game, x, 100, vx, 0, team);
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_CORNER);
        assert(game.restart_team == 1U - team && game.ball.y == (SOCCER_TOP + 8) * SOCCER_ONE);
        steps(&game, 180U, idle);
        assert(game.mode == SOCCER_PLAY); /* Human inactivity cannot stall the restart forever. */
        live_ball(&game, x, 800, vx, 0, 1U - team);
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_GOAL_KICK);
        assert(game.restart_team == team);
        steps(&game, 90U, idle);
        assert(game.mode == SOCCER_PLAY && game.keepers[team].tackle_cooldown > 0U);
        assert(game.velocity.x * (team == 0U ? 1 : -1) > 0);
        assert(game.last_touch_team == team && game.keeper_owner == -1);
    }
    /* First crossed line wins near a corner, regardless of goal-line loop order. */
    live_ball(&game, SOCCER_RIGHT + 2, SOCCER_TOP - 3, 9, -9, 1U);
    soccer_game_step(&game, idle);
    assert(game.restart_kind == SOCCER_THROW_IN);
    live_ball(&game, SOCCER_RIGHT + 3, SOCCER_TOP - 2, 9, -9, 1U);
    soccer_game_step(&game, idle);
    assert(game.restart_kind == SOCCER_CORNER);

    live_ball(&game, SOCCER_RIGHT + 3, 100, 9, 0, 1U);
    soccer_game_step(&game, idle);
    soccer_camera_t camera;
    soccer_camera_reset(&camera, &game);
    assert(camera.position.x > 900 * SOCCER_ONE && camera.position.y == 0);
    steps(&game, 89U, idle);
    soccer_game_step(&game, (soccer_input_t) {.dx = -1, .dy = 1, .shoot = true});
    assert(game.mode == SOCCER_PLAY && game.velocity.x < 0 && game.velocity.y > 0);
    assert(game.shots == 1U && game.pass_target == -1 && game.last_touch_team == 0U);

    /* A controlled dribble can leave the touchline, or carry the whole ball into goal. */
    soccer_game_reset(&game);
    game.mode                = SOCCER_PLAY;
    game.players[0].position = (soccer_vec_t) {800 * SOCCER_ONE, (SOCCER_TOP + 8) * SOCCER_ONE};
    game.players[0].facing_x = 0;
    game.players[0].facing_y = -1;
    game.ball                = game.players[0].position;
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_RESTART && game.restart_team == 1U && game.last_touch_team == 0U);
    soccer_game_reset(&game);
    game.mode                = SOCCER_PLAY;
    game.players[0].position = (soccer_vec_t) {(SOCCER_RIGHT - 8) * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                = game.players[0].position;
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_GOAL && game.goals == 1U);

    soccer_game_reset(&game);
    game.mode                = SOCCER_PLAY;
    game.players[0].position = (soccer_vec_t) {(SOCCER_RIGHT - 50) * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                = game.players[0].position;
    soccer_game_step(&game, (soccer_input_t) {.dx = 1});
    assert(game.keeper_owner == 1 && game.owner == -1 && game.last_touch_team == 1U);

    live_ball(&game, 800, SOCCER_TOP - 2, 0, -9, 0U);
    game.match_ticks = 1U;
    soccer_game_step(&game, idle);
    assert(game.mode == SOCCER_FULL_TIME && game.event == SOCCER_EVENT_FINISH);
    soccer_game_reset(&game);
    assert(game.mode == SOCCER_KICKOFF && game.restart_ticks == 0U);
}

static void aerial_ball(void)
{
    soccer_game_t game;
    soccer_game_init(&game);
    practice_reset(&game);
    soccer_game_step(&game, idle);
    soccer_game_step(&game, (soccer_input_t) {.lob = true});
    assert(game.owner == -1 && game.pass_target == 1 && game.ball_height > 6 * SOCCER_ONE);
    int32_t peak = game.ball_height;
    for (unsigned int i = 0U; i < 120U && game.owner < 0; ++i) {
        soccer_game_step(&game, idle);
        if (game.ball_height > peak) {
            peak = game.ball_height;
        }
    }
    assert(peak > 40 * SOCCER_ONE);
    assert(game.owner == 1 && game.controlled == 1U && game.completed_passes == 1U);
    assert(game.ball_height == 0 && game.vertical_velocity == 0);

    live_ball(&game, 800, 480, 6, 0, 0U);
    game.ball_height                        = 40 * SOCCER_ONE;
    game.players[SOCCER_TEAM_SIZE].position = game.ball;
    soccer_game_step(&game, idle);
    assert(game.owner == -1 && game.ball_height > 24 * SOCCER_ONE);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 90U, idle);
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);

    free_ball(&game, 600, 200, 0, 0);
    game.ball_height = 20 * SOCCER_ONE;
    bool bounced     = false;
    for (unsigned int i = 0U; i < 80U; ++i) {
        soccer_game_step(&game, idle);
        bounced = bounced || game.vertical_velocity > 0;
        assert(game.ball_height >= 0);
    }
    assert(bounced && game.ball_height == 0 && game.vertical_velocity == 0);

    for (unsigned int end = 0U; end < 2U; ++end) {
        const int direction = end == 0U ? -1 : 1;
        /* Too high for the keeper; too high for the goal as well. */
        live_ball(&game, end == 0U ? SOCCER_LEFT + 24 : SOCCER_RIGHT - 24, 480, direction * 9, 0, 1U - end);
        game.ball_height = 60 * SOCCER_ONE;
        steps(&game, 5U, idle);
        assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_GOAL_KICK);
        assert(game.goals == 0U && game.opponent_goals == 0U && game.ball_height == 0);
        free_ball(&game, end == 0U ? SOCCER_LEFT + 1 : SOCCER_RIGHT - 1, 480, direction * 9, 0);
        game.ball_height = 16 * SOCCER_ONE;
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_GOAL); /* Below the bar with the keeper beaten. */
        /* At bar height, reflect back onto the pitch. */
        live_ball(&game, end == 0U ? SOCCER_LEFT + 12 : SOCCER_RIGHT - 12, 480, direction * 9, 0, 1U - end);
        game.ball_height = SOCCER_GOAL_HEIGHT * SOCCER_ONE;
        soccer_game_step(&game, idle);
        assert(game.event == SOCCER_EVENT_POST && game.velocity.x * direction < 0);
        assert(game.mode == SOCCER_PLAY);
        /* A lower flight is reachable by the keeper. */
        live_ball(&game, end == 0U ? SOCCER_LEFT + 34 : SOCCER_RIGHT - 34, 480, direction * 9, 0, 1U - end);
        game.ball_height = 20 * SOCCER_ONE;
        soccer_game_step(&game, idle);
        assert(game.keeper_owner == (int) end && game.ball_height == 0 && game.vertical_velocity == 0);
    }
    soccer_game_reset(&game);
    assert(game.ball_height == 0 && game.vertical_velocity == 0);
}

static void heading_fixture(soccer_game_t* game, int height)
{
    live_ball(game, 600, 480, 0, 0, 0U);
    for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
        game->players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    game->players[0].position = game->ball;
    game->ball_height         = height * SOCCER_ONE;
}

static void heading(void)
{
    soccer_game_t game;
    heading_fixture(&game, 30);
    game.pass_target = 1;
    game.pass_ticks  = 60U;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.players[0].jump_ticks == 24U && game.headers == 0U);
    steps(&game, 6U, idle);
    assert(game.headers == 1U && game.event == SOCCER_EVENT_HEADER);
    assert(game.players[0].headed && game.owner == -1 && game.shots == 0U);
    assert(game.velocity.x > 0 && game.vertical_velocity < 0);
    assert(game.pass_target == -1 && game.last_touch_team == 0U && game.last_kicker == 0);
    const unsigned int cooldown = game.players[0].jump_cooldown;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.headers == 1U && game.players[0].jump_cooldown == cooldown - 1U);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 60U, (soccer_input_t) {.shoot = true});
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);

    heading_fixture(&game, 30);
    soccer_game_step(&game, (soccer_input_t) {.shoot = true, .dx = -1, .dy = -1});
    steps(&game, 6U, idle);
    assert(game.headers == 1U && game.velocity.x < 0 && game.velocity.y < 0);
    heading_fixture(&game, 140);
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    steps(&game, 24U, idle);
    assert(game.headers == 0U && game.players[0].jump_ticks == 0U);
    heading_fixture(&game, 30);
    game.players[0].position.x -= 100 * SOCCER_ONE;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    steps(&game, 24U, idle);
    assert(game.headers == 0U); /* Jumping far from the ball is a miss. */

    /* Red challenges an opponent's lob and heads upfield, never selecting a red
       player for the user or counting its header as a blue action. */
    heading_fixture(&game, 34);
    game.players[0].position.x              -= 200 * SOCCER_ONE;
    game.players[SOCCER_TEAM_SIZE].position  = game.ball;
    soccer_game_step(&game, idle);
    assert(game.players[SOCCER_TEAM_SIZE].jump_ticks > 0U);
    steps(&game, 6U, idle);
    assert(game.last_touch_team == 1U && game.velocity.x < 0 && game.headers == 0U);
    assert(game.controlled == 0U && game.players[SOCCER_TEAM_SIZE].headed);
    soccer_game_reset(&game);
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        assert(game.players[i].jump_ticks == 0U && game.players[i].jump_cooldown == 0U && !game.players[i].headed);
    }
}

static void keeper_dives(void)
{
    soccer_game_t game;
    for (unsigned int end = 0U; end < 2U; ++end) {
        for (int lateral = -1; lateral <= 1; lateral += 2) {
            soccer_game_reset(&game);
            game.mode         = SOCCER_PLAY;
            game.owner        = -1;
            const int facing  = game.keepers[end].facing_x;
            game.ball         = game.keepers[end].position;
            game.ball.x      += facing * 80 * SOCCER_ONE;
            game.ball.y      += lateral * 32 * SOCCER_ONE;
            game.velocity     = (soccer_vec_t) {-facing * 9 * SOCCER_ONE, 0};
            soccer_game_step(&game, idle);
            assert(game.dives[end].ticks == 0U);
            assert(game.dives[end].reaction_ticks == SOCCER_KEEPER_REACTION_TICKS - 1U);
            assert(game.dives[end].observed);
            const int32_t start_y      = game.keepers[end].position.y;
            game.paused                = true;
            const soccer_game_t frozen = game;
            steps(&game, 5U, idle);
            assert(memcmp(&game, &frozen, sizeof(game)) == 0);
            game.paused = false;
            steps(&game, SOCCER_KEEPER_REACTION_TICKS - 1U, idle);
            assert(game.dives[end].ticks == SOCCER_KEEPER_DIVE_TICKS + SOCCER_KEEPER_RECOVERY_TICKS);
            assert(game.dives[end].direction == lateral);
            steps(&game, 7U, idle);
            assert(game.keeper_owner == (int) end);
            assert((game.keepers[end].position.y - start_y) * lateral > 0);
            assert(abs(game.keepers[end].position.y - start_y) <= 45 * SOCCER_ONE);
            steps(&game, 50U, idle);
            assert(game.keeper_owner == -1 && game.dives[end].ticks == 0U);

            /* A deflection cannot reverse a committed dive; recovery cannot catch. */
            game.owner                = -1;
            game.keeper_owner         = -1;
            game.dives[end]           = (soccer_dive_t) {.ticks = 40U, .direction = lateral};
            game.ball                 = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
            game.velocity             = (soccer_vec_t) {0, -lateral * SOCCER_ONE};
            const int32_t committed_y = game.keepers[end].position.y;
            soccer_game_step(&game, idle);
            assert((game.keepers[end].position.y - committed_y) * lateral >= 0);
            game.dives[end].ticks             = 25U;
            game.keepers[end].tackle_cooldown = 0U;
            game.ball                         = game.keepers[end].position;
            game.velocity                     = (soccer_vec_t) {0, 0};
            soccer_game_step(&game, idle);
            assert(game.keeper_owner == -1);
            assert(game.dives[end].ticks == 24U);
            soccer_game_reset(&game);
            assert(game.dives[0].ticks == 0U && game.dives[1].ticks == 0U);
        }
        /* Do not dive after departing balls or unreachable high lobs. */
        for (unsigned int high = 0U; high < 2U; ++high) {
            soccer_game_reset(&game);
            game.mode         = SOCCER_PLAY;
            game.owner        = -1;
            const int facing  = game.keepers[end].facing_x;
            game.ball         = game.keepers[end].position;
            game.ball.x      += facing * 80 * SOCCER_ONE;
            game.ball.y      += 32 * SOCCER_ONE;
            game.velocity.x   = facing * 9 * SOCCER_ONE;
            if (high != 0U) {
                game.velocity.x  = -game.velocity.x;
                game.ball_height = 60 * SOCCER_ONE;
            }
            soccer_game_step(&game, idle);
            assert(game.dives[end].ticks == 0U);
        }
    }
}

static void attacking_options(void)
{
    soccer_game_t game;
    /* A defender just beyond the receiver marks the endpoint without blocking the segment. */
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    game.players[0].position                = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                               = game.players[0].position;
    game.players[1].position                = (soccer_vec_t) {960 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[2].position                = (soccer_vec_t) {920 * SOCCER_ONE, 590 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {980 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    assert(game.pass_target == 2);

    for (unsigned int team = 0U; team < 2U; ++team) {
        const unsigned int carrier = team * SOCCER_TEAM_SIZE;
        const unsigned int support = carrier + 1U;
        const unsigned int marker  = (1U - team) * SOCCER_TEAM_SIZE + 2U;
        const int direction        = team == 0U ? 1 : -1;
        for (unsigned int marked = 0U; marked < 2U; ++marked) {
            soccer_game_reset(&game);
            game.mode = SOCCER_PLAY;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            game.owner                     = (int) carrier;
            game.ball                      = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
            game.players[carrier].position = game.ball;
            game.players[support].position = (soccer_vec_t) {(800 - direction * 60) * SOCCER_ONE, 345 * SOCCER_ONE};
            if (marked != 0U) {
                game.players[marker].position = (soccer_vec_t) {(800 - direction * 30) * SOCCER_ONE, 412 * SOCCER_ONE};
            }
            soccer_game_step(&game, idle);
            if (marked != 0U) {
                assert(game.players[support].position.y != 345 * SOCCER_ONE);
            } else {
                assert(game.players[support].position.y == 345 * SOCCER_ONE);
            }
        }
        /* Supporting players keep advancing during ground/lob passes and keeper holds,
           but recover after the opponent takes possession. The designated receiver is separate. */
        for (unsigned int situation = 0U; situation < 4U; ++situation) {
            soccer_game_reset(&game);
            game.mode                      = SOCCER_PLAY;
            game.owner                     = -1;
            game.ball                      = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
            game.players[support].position = (soccer_vec_t) {(800 - direction * 200) * SOCCER_ONE, 350 * SOCCER_ONE};
            game.players[carrier].position = (soccer_vec_t) {840 * SOCCER_ONE, 480 * SOCCER_ONE};
            if (situation < 2U) {
                game.pass_target      = (int) carrier + 2;
                game.pass_ticks       = 60U;
                game.pass_destination = game.players[carrier + 2U].position;
                game.ball_height      = (int32_t) situation * 20 * SOCCER_ONE;
            } else if (situation == 2U) {
                game.keeper_owner = (int) team;
                game.keeper_ticks = 45U;
            } else {
                game.owner = (int) ((1U - team) * SOCCER_TEAM_SIZE);
            }
            soccer_game_step(&game, idle);
            const int32_t progress =
                (game.players[support].position.x - (800 - direction * 200) * SOCCER_ONE) * direction;
            if (situation < 3U) {
                assert(progress > 0);
            } else {
                assert(progress < 0);
            }
        }
    }
}

static void charged_shots(void)
{
    soccer_game_t game;
    int32_t quick_speed = 0;
    for (unsigned int duration = 0U; duration <= 30U; duration += 15U) {
        soccer_game_reset(&game);
        practice_reset(&game);
        game.owner = 0;
        if (duration == 0U) {
            soccer_game_step(&game, (soccer_input_t) {.shoot = true});
            quick_speed = game.velocity.x;
        } else {
            soccer_game_step(&game, (soccer_input_t) {.shoot = true, .shoot_held = true});
            assert(game.shots == 0U && game.shot_charge == 1U && game.owner == 0);
            steps(&game, duration - 1U, (soccer_input_t) {.shoot_held = true});
            if (duration < 30U) {
                assert(game.shots == 0U && game.shot_charge == duration);
                soccer_game_step(&game, idle);
            }
            assert(game.velocity.x > quick_speed && game.velocity.x <= quick_speed * 3 / 2 + 1);
        }
        assert(game.shots == 1U && game.shot_charge == 0U && game.owner == -1);
        steps(&game, 3U, (soccer_input_t) {.shoot_held = true});
        assert(game.shots == 1U && game.shot_charge == 0U);
    }
    for (unsigned int cancel = 0U; cancel < 5U; ++cancel) {
        soccer_game_reset(&game);
        practice_reset(&game);
        game.owner = 0;
        soccer_game_step(&game, (soccer_input_t) {.shoot = true, .shoot_held = true});
        game.paused                = true;
        const soccer_game_t frozen = game;
        steps(&game, 5U, idle);
        assert(memcmp(&game, &frozen, sizeof(game)) == 0);
        game.paused          = false;
        soccer_input_t input = {.shoot_held = true};
        if (cancel == 0U) {
            input.pass = true;
        } else if (cancel == 1U) {
            input.lob = true;
        } else if (cancel == 2U) {
            input.switch_player = true;
        } else if (cancel == 3U) {
            game.owner = SOCCER_TEAM_SIZE;
        } else {
            soccer_game_reset(&game);
        }
        soccer_game_step(&game, input);
        assert(game.shot_charge == 0U && game.shots == 0U);
    }
    /* Holding a defensive action does not turn a won ball into a charged shot. */
    soccer_game_reset(&game);
    practice_reset(&game);
    game.owner                              = SOCCER_TEAM_SIZE;
    game.players[SOCCER_TEAM_SIZE].position = game.ball;
    soccer_game_step(&game, (soccer_input_t) {.shoot = true, .shoot_held = true});
    assert(game.owner == 0 && game.shot_charge == 0U && game.shots == 0U);
    steps(&game, 5U, (soccer_input_t) {.shoot_held = true});
    assert(game.shot_charge == 0U && game.shots == 0U);
}

static void shot_fixture(soccer_game_t* game, unsigned int team)
{
    soccer_game_reset(game);
    game->mode             = SOCCER_PLAY;
    game->controlled       = team * SOCCER_TEAM_SIZE;
    game->owner            = (int) game->controlled;
    game->possession_ticks = 120U;
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        game->players[i].position        = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
        game->players[i].tackle_cooldown = SOCCER_MATCH_TICKS;
    }
    soccer_player_t* shooter    = &game->players[game->controlled];
    shooter->position           = (soccer_vec_t) {800 * SOCCER_ONE, SOCCER_GOAL_CENTRE_Y * SOCCER_ONE};
    shooter->facing_x           = team == 0U ? 1 : -1;
    shooter->facing_y           = 0;
    shooter->tackle_cooldown    = 0U;
    game->ball                  = shooter->position;
    game->keepers[0].position.x = -1000 * SOCCER_ONE;
    game->keepers[1].position.x = -1000 * SOCCER_ONE;
}

static int projected_shot_y(const soccer_game_t* game, unsigned int team)
{
    const int32_t target_x = (team == 0U ? SOCCER_RIGHT + SOCCER_GOAL_LINE_CROSSING_OFFSET :
                                           SOCCER_LEFT - SOCCER_GOAL_LINE_CROSSING_OFFSET) *
                             SOCCER_ONE;
    assert(game->velocity.x != 0);
    return (int) ((game->ball.y + (int64_t) game->velocity.y * (target_x - game->ball.x) / game->velocity.x) /
                  SOCCER_ONE);
}

static void take_charged_shot(soccer_game_t* game, soccer_input_t aim, unsigned int charge)
{
    if (charge == 0U) {
        aim.shoot = true;
        soccer_game_step(game, aim);
        return;
    }
    aim.shoot      = true;
    aim.shoot_held = true;
    steps(game, charge, aim);
    if (charge < SOCCER_SHOT_MAX_CHARGE) {
        aim.shoot      = false;
        aim.shoot_held = false;
        soccer_game_step(game, aim);
    }
}

static void goal_relative_shots(void)
{
    soccer_game_t game;
    for (unsigned int team = 0U; team < 2U; ++team) {
        const int attack  = team == 0U ? 1 : -1;
        const int lanes[] = {
            SOCCER_GOAL_CENTRE_Y - SOCCER_GOAL_LANE_OFFSET,
            SOCCER_GOAL_CENTRE_Y,
            SOCCER_GOAL_CENTRE_Y + SOCCER_GOAL_LANE_OFFSET,
        };
        const int aim_y[] = {-1, 0, 1};
        for (unsigned int lane = 0U; lane < 3U; ++lane) {
            shot_fixture(&game, team);
            take_charged_shot(&game, (soccer_input_t) {.dx = attack, .dy = aim_y[lane]}, 0U);
            assert(game.owner == -1 && game.stats[team].shots == 1U);
            assert(game.velocity.x * attack > 0);
            assert(abs(projected_shot_y(&game, team) - lanes[lane]) <= 1);
        }

        /* Releasing without a direction uses the current facing and therefore
           retains central goal intent when facing toward goal. */
        shot_fixture(&game, team);
        take_charged_shot(&game, idle, 0U);
        assert(abs(projected_shot_y(&game, team) - SOCCER_GOAL_CENTRE_Y) <= 1);

        /* Side/back input remains a raw eight-way clearance. */
        shot_fixture(&game, team);
        take_charged_shot(&game, (soccer_input_t) {.dx = -attack, .dy = -1}, 0U);
        assert(game.velocity.x * attack < 0 && game.velocity.y < 0);
        assert(abs(abs(game.velocity.x) - abs(game.velocity.y)) <= 1);

        const unsigned int charges[] = {0U, 15U, 30U};
        const int expected_speeds[]  = {
            SOCCER_SHOT_BASE_SPEED * 250 / 256,
            (SOCCER_SHOT_BASE_SPEED * 5 / 4) * 250 / 256,
            (SOCCER_SHOT_BASE_SPEED * 3 / 2) * 250 / 256,
        };
        for (unsigned int level = 0U; level < 3U; ++level) {
            shot_fixture(&game, team);
            take_charged_shot(&game, (soccer_input_t) {.dx = attack}, charges[level]);
            assert(abs(game.velocity.x) == expected_speeds[level]);
            assert(game.velocity.y == 0);
        }

        shot_fixture(&game, team);
        take_charged_shot(&game, (soccer_input_t) {.dx = attack, .dy = -1}, 15U);
        assert(abs(projected_shot_y(&game, team) - (SOCCER_GOAL_CENTRE_Y - SOCCER_GOAL_LANE_OFFSET)) <= 1);
        shot_fixture(&game, team);
        take_charged_shot(&game, (soccer_input_t) {.dx = attack, .dy = -1}, 30U);
        assert(abs(projected_shot_y(&game, team) -
                   (SOCCER_GOAL_CENTRE_Y - SOCCER_GOAL_LANE_OFFSET - SOCCER_SHOT_MAX_PLACEMENT_ERROR)) <= 2);
    }

    soccer_game_t duplicate;
    shot_fixture(&game, 0U);
    duplicate = game;
    take_charged_shot(&game, (soccer_input_t) {.dx = 1, .dy = 1}, 30U);
    take_charged_shot(&duplicate, (soccer_input_t) {.dx = 1, .dy = 1}, 30U);
    assert(memcmp(&game, &duplicate, sizeof(game)) == 0);
}

static void defensive_marking(void)
{
    soccer_game_t game;
    for (unsigned int team = 0U; team < 2U; ++team) {
        const unsigned int base  = team * SOCCER_TEAM_SIZE;
        const unsigned int enemy = (1U - team) * SOCCER_TEAM_SIZE;
        const int direction      = team == 0U ? 1 : -1;
        for (unsigned int scenario = 0U; scenario < 4U; ++scenario) {
            soccer_game_reset(&game);
            game.mode             = SOCCER_PLAY;
            game.owner            = (int) enemy;
            game.possession_ticks = 120U;
            game.ball             = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {(800 + direction * 600) * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            game.players[enemy].position     = game.ball;
            game.players[base + 2U].position = (soccer_vec_t) {820 * SOCCER_ONE, 480 * SOCCER_ONE};
            const int32_t home_x             = (800 - direction * 514) * SOCCER_ONE;
            game.players[base + 3U].position = (soccer_vec_t) {home_x, 397 * SOCCER_ONE};
            game.players[base + 4U].position = (soccer_vec_t) {home_x, 562 * SOCCER_ONE};
            if (scenario != 2U) {
                game.players[enemy + 1U].position =
                    (soccer_vec_t) {(800 - direction * 440) * SOCCER_ONE, 450 * SOCCER_ONE};
            }
            if (scenario == 0U) {
                game.players[enemy + 2U].position =
                    (soccer_vec_t) {(800 - direction * 440) * SOCCER_ONE, 510 * SOCCER_ONE};
            }
            if (scenario == 3U && team == 0U) {
                game.controlled = base + 3U;
            }
            soccer_game_step(&game, idle);
            if (scenario == 2U || (scenario == 3U && team == 0U)) {
                assert(game.players[base + 3U].position.x == home_x);
                assert(game.players[base + 3U].position.y == 397 * SOCCER_ONE);
            } else {
                assert((game.players[base + 3U].position.x - home_x) * direction > 0);
                assert(game.players[base + 3U].position.y > 397 * SOCCER_ONE);
                assert((game.players[enemy + 1U].position.x - game.players[base + 3U].position.x) * direction > 0);
            }
            if (scenario == 0U) {
                assert(game.players[base + 4U].position.y < 562 * SOCCER_ONE);
            } else if (scenario != 3U) {
                /* A single attacker does not pull both centre-backs out of shape. */
                assert(game.players[base + 4U].position.x == home_x);
                assert(game.players[base + 4U].position.y == 562 * SOCCER_ONE);
            }
        }
    }
}

static void through_passes(void)
{
    soccer_game_t game;
    for (unsigned int scenario = 0U; scenario < 5U; ++scenario) {
        soccer_game_reset(&game);
        game.mode = SOCCER_PLAY;
        for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
            game.players[i].position = (soccer_vec_t) {40 * SOCCER_ONE, 40 * SOCCER_ONE};
        }
        game.players[0].position = (soccer_vec_t) {600 * SOCCER_ONE, 480 * SOCCER_ONE};
        game.players[1].position = (soccer_vec_t) {740 * SOCCER_ONE, 480 * SOCCER_ONE};
        if (scenario == 3U) {
            game.players[0].position.x = 1400 * SOCCER_ONE;
            game.players[1].position.x = 1540 * SOCCER_ONE;
        }
        game.ball        = game.players[0].position;
        game.shot_charge = 10U;
        soccer_game_step(&game, (soccer_input_t) {.through = true});
        assert(game.owner == -1 && game.pass_target == 1 && game.through_pass);
        assert(game.passes == 1U && game.shots == 0U && game.shot_charge == 0U);
        assert(game.ball_height == 0 && game.vertical_velocity == 0);
        if (scenario == 0U || scenario == 4U) {
            assert(game.pass_destination.x == game.players[1].position.x + 80 * SOCCER_ONE);
            for (unsigned int i = 0U; i < 100U && game.completed_passes == 0U; ++i) {
                soccer_game_step(&game, idle);
            }
            assert(game.completed_passes == 1U && game.owner == 1 && game.controlled == 1U);
            assert(!game.through_pass && game.players[1].position.x > 760 * SOCCER_ONE);
        } else if (scenario == 1U) {
            game.players[SOCCER_TEAM_SIZE].position = game.ball;
            soccer_game_step(&game, idle);
            assert(game.owner == SOCCER_TEAM_SIZE && game.completed_passes == 0U);
            assert(!game.through_pass && game.pass_target == -1);
        } else if (scenario == 2U) {
            game.pass_ticks = 1U;
            soccer_game_step(&game, idle);
            assert(!game.through_pass && game.pass_target == -1);
        } else {
            assert(game.pass_destination.x == (SOCCER_RIGHT - 24) * SOCCER_ONE);
            assert(game.pass_destination.y >= (SOCCER_TOP + 24) * SOCCER_ONE);
            assert(game.pass_destination.y <= (SOCCER_BOTTOM - 24) * SOCCER_ONE);
        }
    }
    soccer_game_reset(&game);
    game.mode                = SOCCER_PLAY;
    game.players[0].facing_x = -1;
    for (unsigned int i = 1U; i < SOCCER_TEAM_SIZE; ++i) {
        game.players[i].position.x = 1200 * SOCCER_ONE;
    }
    soccer_game_step(&game, (soccer_input_t) {.through = true});
    assert(game.pass_target == -1 && !game.through_pass && game.velocity.x < 0);
    assert(game.passes == 1U && game.owner == -1);
    soccer_game_reset(&game);
    assert(!game.through_pass);
}

static void opponent_through_passes(void)
{
    soccer_game_t game;
    for (int level = SOCCER_EASY; level <= SOCCER_HARD; ++level) {
        for (unsigned int scenario = 0U; scenario < 5U; ++scenario) {
            soccer_game_reset(&game);
            game.mode             = SOCCER_PLAY;
            game.owner            = SOCCER_TEAM_SIZE;
            game.possession_ticks = scenario == 4U ? 30U : 0U;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {1400 * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            game.players[SOCCER_TEAM_SIZE].position      = (soccer_vec_t) {1000 * SOCCER_ONE, 480 * SOCCER_ONE};
            game.players[SOCCER_TEAM_SIZE + 1U].position = (soccer_vec_t) {860 * SOCCER_ONE, 450 * SOCCER_ONE};
            if (scenario == 1U) {
                game.players[0].position = (soccer_vec_t) {780 * SOCCER_ONE, 420 * SOCCER_ONE};
            } else if (scenario == 2U) {
                game.players[0].position = (soccer_vec_t) {900 * SOCCER_ONE, 450 * SOCCER_ONE};
            } else if (scenario == 3U) {
                game.players[SOCCER_TEAM_SIZE].position.x      = 290 * SOCCER_ONE;
                game.players[SOCCER_TEAM_SIZE + 1U].position.x = 160 * SOCCER_ONE;
            }
            game.ball = game.players[SOCCER_TEAM_SIZE].position;
            soccer_game_step(&game, idle);
            if (scenario == 0U) {
                assert(game.through_pass && game.pass_target == SOCCER_TEAM_SIZE + 1 && game.owner == -1);
                assert(game.pass_destination.x < game.players[SOCCER_TEAM_SIZE + 1U].position.x);
                for (unsigned int i = 0U; i < 100U && game.owner < 0; ++i) {
                    soccer_game_step(&game, idle);
                }
                assert(game.owner == SOCCER_TEAM_SIZE + 1 && !game.through_pass);
            } else {
                assert(!game.through_pass);
                if (scenario == 2U) {
                    assert(game.owner == -1 && game.pass_target == SOCCER_TEAM_SIZE + 1);
                } else {
                    assert(game.owner == SOCCER_TEAM_SIZE);
                }
            }
            assert(game.passes == 0U && game.completed_passes == 0U && game.controlled == 0U);
        }
    }
}

static void keeper_parries(void)
{
    soccer_game_t game;
    for (unsigned int end = 0U; end < 2U; ++end) {
        for (unsigned int scenario = 0U; scenario < 3U; ++scenario) {
            soccer_game_reset(&game);
            game.mode          = SOCCER_PLAY;
            game.owner         = -1;
            const int facing   = game.keepers[end].facing_x;
            game.ball          = game.keepers[end].position;
            game.ball.x       += facing * 20 * SOCCER_ONE;
            game.velocity      = (soccer_vec_t) {-facing * 12 * SOCCER_ONE, 0};
            game.pass_target   = (int) ((1U - end) * SOCCER_TEAM_SIZE);
            game.through_pass  = true;
            game.pass_ticks    = 60U;
            soccer_game_step(&game, idle);
            assert(game.event == SOCCER_EVENT_SAVE && game.keeper_owner == -1 && game.owner == -1);
            assert(game.velocity.x * facing > 0 && game.velocity.y != 0);
            assert(game.last_touch_team == end && game.keepers[end].tackle_cooldown == 18U);
            assert(game.pass_target == -1 && !game.through_pass && game.last_kicker == -1);
            game.paused                = true;
            const soccer_game_t frozen = game;
            steps(&game, 3U, idle);
            assert(memcmp(&game, &frozen, sizeof(game)) == 0);
            game.paused = false;
            if (scenario == 0U) {
                const unsigned int attacker     = (1U - end) * SOCCER_TEAM_SIZE;
                game.players[attacker].position = game.ball;
                soccer_game_step(&game, idle);
                assert(game.last_touch_team == 1U - end && game.keeper_owner == -1);
                if (attacker < SOCCER_TEAM_SIZE) {
                    assert(game.owner == (int) attacker);
                } else {
                    assert(game.owner == -1 && game.event == SOCCER_EVENT_SHOT);
                }
            } else if (scenario == 1U) {
                /* A quick second shot can beat a keeper still recovering. */
                game.ball      = game.keepers[end].position;
                game.ball.x   += facing * 10 * SOCCER_ONE;
                game.velocity  = (soccer_vec_t) {-facing * 9 * SOCCER_ONE, 0};
                steps(&game, 6U, idle);
                assert(game.mode == SOCCER_GOAL);
                assert(game.goals + game.opponent_goals == 1U);
            } else {
                /* A saved ball deflected over the keeper's goal line is a corner. */
                game.ball = (soccer_vec_t) {(end == 0U ? SOCCER_LEFT : SOCCER_RIGHT) * SOCCER_ONE, 600 * SOCCER_ONE};
                game.velocity = (soccer_vec_t) {-facing * 9 * SOCCER_ONE, 0};
                soccer_game_step(&game, idle);
                assert(game.mode == SOCCER_RESTART && game.restart_kind == SOCCER_CORNER);
                assert(game.restart_team == 1U - end);
            }
        }
    }
}

static void match_statistics(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    steps(&game, 90U, idle);
    assert(game.stats[0].possession == 0U && game.stats[1].possession == 0U);
    steps(&game, 10U, idle);
    assert(game.stats[0].possession == 10U);
    soccer_game_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.stats[0].shots == 1U && game.shot_team == 0);
    assert(game.stats[0].on_target == 0U);
    game.ball      = game.keepers[1].position;
    game.ball.x   -= 20 * SOCCER_ONE;
    game.velocity  = (soccer_vec_t) {12 * SOCCER_ONE, 0};
    soccer_game_step(&game, idle);
    assert(game.stats[0].on_target == 1U && game.stats[1].saves == 1U && game.shot_team == -1);
    /* Collecting the spilled shot later must not add a second save. */
    game.keepers[1].tackle_cooldown = 0U;
    game.ball                       = game.keepers[1].position;
    game.velocity                   = (soccer_vec_t) {0, 0};
    soccer_game_step(&game, idle);
    assert(game.keeper_owner == 1 && game.stats[1].saves == 1U);
    steps(&game, 5U, idle);
    assert(game.stats[1].possession == 5U);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 5U, idle);
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);

    for (unsigned int team = 0U; team < 2U; ++team) {
        soccer_game_reset(&game);
        game.mode              = SOCCER_PLAY;
        game.owner             = -1;
        game.shot_team         = (int) team;
        game.stats[team].shots = 1U;
        const int direction    = team == 0U ? 1 : -1;
        game.ball     = (soccer_vec_t) {(team == 0U ? SOCCER_RIGHT : SOCCER_LEFT) * SOCCER_ONE, 480 * SOCCER_ONE};
        game.velocity = (soccer_vec_t) {direction * 9 * SOCCER_ONE, 0};
        soccer_game_step(&game, idle);
        assert(game.mode == SOCCER_GOAL && game.stats[team].on_target == 1U);
        steps(&game, 90U, idle);
        assert(game.mode == SOCCER_KICKOFF && game.stats[team].shots == 1U && game.stats[team].on_target == 1U);
    }
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_game_step(&game, (soccer_input_t) {.pass = true});
    game.ball     = game.keepers[1].position;
    game.velocity = (soccer_vec_t) {0, 0};
    soccer_game_step(&game, idle);
    assert(game.stats[0].shots == 0U && game.stats[1].saves == 0U);
    soccer_game_reset(&game);
    for (unsigned int team = 0U; team < 2U; ++team) {
        assert(game.stats[team].shots == 0U && game.stats[team].on_target == 0U);
        assert(game.stats[team].saves == 0U && game.stats[team].possession == 0U);
    }
}

static void difficulty_levels(void)
{
    int32_t distance[3];
    for (int level = SOCCER_EASY; level <= SOCCER_HARD; ++level) {
        soccer_game_t game;
        soccer_game_reset(&game);
        assert(game.difficulty == SOCCER_NORMAL);
        game.difficulty                         = (soccer_difficulty_t) level;
        game.mode                               = SOCCER_PLAY;
        game.owner                              = SOCCER_TEAM_SIZE;
        game.possession_ticks                   = 100U;
        game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {1000 * SOCCER_ONE, 458 * SOCCER_ONE};
        game.ball                               = game.players[SOCCER_TEAM_SIZE].position;
        const int32_t blue_start                = game.players[0].position.x;
        steps(&game, 10U, (soccer_input_t) {.dx = 1});
        distance[level] = 1000 * SOCCER_ONE - game.players[SOCCER_TEAM_SIZE].position.x;
        assert(game.players[0].position.x - blue_start == 25 * SOCCER_ONE);
        assert(game.match_ticks == SOCCER_MATCH_TICKS - 10U);
    }
    assert(distance[SOCCER_EASY] < distance[SOCCER_NORMAL]);
    assert(distance[SOCCER_NORMAL] < distance[SOCCER_HARD]);
    assert(distance[SOCCER_HARD] < 25 * SOCCER_ONE);
}

static void sprinting(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    for (unsigned int i = 1U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {40 * SOCCER_ONE, 40 * SOCCER_ONE};
    }
    game.players[0].position = (soccer_vec_t) {600 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                = game.players[0].position;
    soccer_game_t diagonal   = game;
    steps(&game, 30U, (soccer_input_t) {.dx = 1, .sprint = true});
    steps(&diagonal, 30U, (soccer_input_t) {.dx = 1, .dy = 1, .sprint = true});
    const int64_t straight = game.players[0].position.x - 600 * SOCCER_ONE;
    const int64_t dx       = diagonal.players[0].position.x - 600 * SOCCER_ONE;
    const int64_t dy       = diagonal.players[0].position.y - 480 * SOCCER_ONE;
    assert(straight == 105 * SOCCER_ONE);
    assert(dx * dx + dy * dy > straight * straight * 99 / 100);
    assert(dx * dx + dy * dy <= straight * straight);
    assert(game.players[0].stamina == 60U && game.players[0].sprinting);
    steps(&game, 60U, (soccer_input_t) {.dx = 1, .sprint = true});
    assert(game.players[0].stamina == 0U && game.players[0].sprint_rest == 60U);
    game.paused                = true;
    const soccer_game_t frozen = game;
    steps(&game, 5U, idle);
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
    game.paused               = false;
    const int32_t exhausted_x = game.players[0].position.x;
    steps(&game, 10U, (soccer_input_t) {.dx = 1, .sprint = true});
    assert(game.players[0].position.x - exhausted_x == 25 * SOCCER_ONE);
    assert(game.players[0].stamina == 10U && game.players[0].sprint_rest == 50U);
    soccer_game_step(&game, (soccer_input_t) {.switch_player = true, .dx = 1, .sprint = true});
    assert(game.controlled == 1U && game.players[1].stamina == 89U);
    assert(game.players[0].stamina == 11U && !game.players[0].sprinting);
    steps(&game, 80U, idle);
    assert(game.players[0].stamina == 90U && game.players[0].sprint_rest == 0U);
    soccer_game_reset(&game);
    assert(game.players[0].stamina == 90U && !game.players[0].sprinting);
    game.mode                  = SOCCER_PLAY;
    game.owner                 = -1;
    game.players[0].position.x = (SOCCER_RIGHT - 8) * SOCCER_ONE;
    soccer_game_step(&game, (soccer_input_t) {.dx = 1, .sprint = true});
    assert(game.players[0].stamina == 90U && !game.players[0].sprinting);
}

static void defensive_switching(void)
{
    for (unsigned int scenario = 0U; scenario < 6U; ++scenario) {
        soccer_game_t game;
        soccer_game_reset(&game);
        game.mode  = SOCCER_PLAY;
        game.owner = -1;
        game.ball  = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
        for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
            game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
        }
        game.players[1].position        = (soccer_vec_t) {850 * SOCCER_ONE, 480 * SOCCER_ONE};
        game.players[2].position        = (soccer_vec_t) {900 * SOCCER_ONE, 480 * SOCCER_ONE};
        game.players[1].jump_ticks      = scenario == 0U ? 12U : 0U;
        game.players[1].tackle_cooldown = scenario == 1U ? 25U : 24U;
        if (scenario == 2U) {
            for (unsigned int i = 1U; i < SOCCER_TEAM_SIZE; ++i) {
                game.players[i].jump_ticks = 12U;
            }
        }
        if (scenario == 4U) {
            /* Possession keeps predictable cycling even if the next player is airborne. */
            game.owner                 = 0;
            game.players[1].jump_ticks = 12U;
        }
        soccer_game_step(&game, (soccer_input_t) {.switch_player = scenario != 5U});
        unsigned int expected = 1U;
        if (scenario < 2U) {
            expected = 2U;
        } else if (scenario == 5U) {
            expected = 0U;
        }
        assert(game.controlled == expected);
        assert(game.controlled < SOCCER_TEAM_SIZE);
    }
}

static void arcade_buttons(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = true});
    assert(game.passes == 0U && game.pass_charge > 0U);
    soccer_game_arcade_step(&game, idle);
    assert(game.passes == 1U && game.ball_height == 0 && game.pass_charge == 0U);

    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = true});
    for (unsigned int tick = 0U; tick < 17U; ++tick) {
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass_held = true});
    }
    assert(game.passes == 1U && game.ball_height > 0 && game.pass_charge == 0U);
    for (unsigned int tick = 0U; tick < 40U; ++tick) {
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass_held = true});
    }
    assert(game.passes == 1U);

    /* A defensive J press does nothing and cannot become a pass on collection. */
    soccer_game_reset(&game);
    game.mode  = SOCCER_PLAY;
    game.owner = -1;
    game.ball  = (soccer_vec_t) {500 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = true});
    const unsigned int defender = game.controlled;
    assert(defender == 0U && game.passes == 0U && game.tackles == 0U);
    game.owner            = (int) defender;
    game.possession_ticks = 30U;
    soccer_game_arcade_step(&game, (soccer_input_t) {.pass_held = true});
    soccer_game_arcade_step(&game, idle);
    assert(game.controlled == defender && game.passes == 0U);

    /* Losing possession, pausing and shooting each cancel a pending pass. */
    for (unsigned int scenario = 0U; scenario < 3U; ++scenario) {
        soccer_game_reset(&game);
        game.mode = SOCCER_PLAY;
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = true});
        if (scenario == 0U) {
            game.owner = -1;
            game.ball  = (soccer_vec_t) {500 * SOCCER_ONE, 480 * SOCCER_ONE};
        } else if (scenario == 1U) {
            game.paused = true;
        }
        soccer_game_arcade_step(&game, (soccer_input_t) {.shoot = scenario == 2U});
        assert(game.pass_charge == 0U && game.passes == 0U);
    }
    /* The same gesture takes short/lofted corners and waits through setup. */
    for (unsigned int lofted = 0U; lofted < 2U; ++lofted) {
        soccer_game_reset(&game);
        game.mode             = SOCCER_RESTART;
        game.restart_kind     = SOCCER_CORNER;
        game.restart_team     = 0U;
        game.restart_taker    = 0U;
        game.restart_receiver = 1U;
        game.restart_ticks    = 120U;
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = true});
        assert(game.mode == SOCCER_RESTART && game.pass_charge == 0U);
        game.restart_ticks = 90U;
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true, .pass_held = lofted != 0U});
        if (lofted != 0U) {
            for (unsigned int tick = 0U; tick < 16U; ++tick) {
                soccer_game_arcade_step(&game, (soccer_input_t) {.pass_held = true});
            }
        }
        assert(game.mode == SOCCER_PLAY && game.passes == 1U);
        assert((game.ball_height > 0) == (lofted != 0U));
    }
    /* Removed action flags cannot enter gameplay through the arcade adapter. */
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_game_arcade_step(
        &game, (soccer_input_t) {.dx = 1, .sprint = true, .through = true, .lob = true, .switch_player = true});
    assert(game.controlled == 0U && game.passes == 0U && !game.players[0].sprinting);
}

static void automatic_control(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode             = SOCCER_PLAY;
    game.owner            = SOCCER_TEAM_SIZE;
    game.possession_ticks = 90U;
    game.ball             = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    game.players[SOCCER_TEAM_SIZE].position = game.ball;
    game.players[1].position                = (soccer_vec_t) {850 * SOCCER_ONE, 480 * SOCCER_ONE};
    /* A tackle press protects the current selection even with a nearer defender. */
    soccer_game_arcade_step(&game, (soccer_input_t) {.shoot = true});
    for (unsigned int tick = 0U; tick < 18U; ++tick) {
        soccer_game_arcade_step(&game, idle);
    }
    assert(game.controlled == 0U && game.selection_cooldown > 0U);
    game.selection_cooldown = 0U;
    /* A better-placed defender must not take control away immediately. */
    const int32_t start_x = game.players[0].position.x;
    soccer_game_arcade_step(&game, (soccer_input_t) {.dx = 1});
    assert(game.controlled == 0U && game.players[0].position.x > start_x);
    game.owner = -1;
    game.ball  = (soccer_vec_t) {700 * SOCCER_ONE, 480 * SOCCER_ONE};
    soccer_game_arcade_step(&game, idle);
    game.owner = SOCCER_TEAM_SIZE;
    soccer_game_arcade_step(&game, idle);
    assert(game.controlled == 0U);
    soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true});
    assert(game.controlled == 0U);
    for (unsigned int tick = 0U; tick < 12U; ++tick) {
        soccer_game_arcade_step(&game, idle);
    }
    assert(game.controlled == 1U && game.selection_cooldown > 0U);
    /* A closer candidate cannot cause an immediate second switch. */
    game.players[0].position    = game.ball;
    game.players[0].position.x -= 45 * SOCCER_ONE;
    soccer_game_arcade_step(&game, idle);
    assert(game.controlled == 1U);
    /* A teammate collecting the ball still receives control automatically. */
    game.owner               = -1;
    game.last_kicker         = -1;
    game.ball                = (soccer_vec_t) {500 * SOCCER_ONE, 500 * SOCCER_ONE};
    game.players[2].position = game.ball;
    soccer_game_arcade_step(&game, idle);
    assert(game.owner == 2 && game.controlled == 2U);
}

static void opposing_player_spacing(void)
{
    for (unsigned int side = 0U; side < 2U; ++side) {
        soccer_game_t game;
        soccer_game_reset(&game);
        game.mode  = SOCCER_PLAY;
        game.owner = -1;
        game.ball  = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
        for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
            game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
        }
        const unsigned int blue = 2U, red = SOCCER_TEAM_SIZE + 2U;
        game.players[blue].position = (soccer_vec_t) {(side == 0U ? 200 : 1400) * SOCCER_ONE, 650 * SOCCER_ONE};
        game.players[red].position  = game.players[blue].position;
        soccer_game_step(&game, idle);
        int32_t dx = game.players[blue].position.x - game.players[red].position.x;
        int32_t dy = game.players[blue].position.y - game.players[red].position.y;
        assert(dx * dx + dy * dy >= 36 * SOCCER_ONE * SOCCER_ONE);
        /* Separation does not displace the human, including at the pitch edge. */
        game.controlled             = blue;
        game.players[blue].position = (soccer_vec_t) {(SOCCER_LEFT + 8) * SOCCER_ONE, (SOCCER_TOP + 8) * SOCCER_ONE};
        game.players[red].position  = game.players[blue].position;
        const soccer_vec_t human    = game.players[blue].position;
        steps(&game, 30U, idle);
        assert(game.players[blue].position.x == human.x && game.players[blue].position.y == human.y);
        for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
            assert(game.players[i].position.x >= (SOCCER_LEFT + 8) * SOCCER_ONE);
            assert(game.players[i].position.y >= (SOCCER_TOP + 8) * SOCCER_ONE);
            assert(game.players[i].position.x <= (SOCCER_RIGHT - 8) * SOCCER_ONE);
            assert(game.players[i].position.y <= (SOCCER_BOTTOM - 8) * SOCCER_ONE);
        }
    }
}

static void tackle_recovery_control(void)
{
    for (unsigned int side = 0U; side < 2U; ++side) {
        soccer_game_t game;
        soccer_game_reset(&game);
        game.mode             = SOCCER_PLAY;
        game.owner            = SOCCER_TEAM_SIZE;
        game.possession_ticks = 0U;
        game.ball             = (soccer_vec_t) {(side == 0U ? 600 : 1000) * SOCCER_ONE, 480 * SOCCER_ONE};
        for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
            game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
        }
        game.players[SOCCER_TEAM_SIZE].position  = game.ball;
        game.players[1].position                 = game.ball;
        game.players[1].position.x              += 20 * SOCCER_ONE;
        game.players[1].facing_x                 = -1;
        game.players[1].facing_y                 = 0;
        soccer_game_arcade_step(&game, idle);
        assert(game.owner == 1 && game.controlled == 1U && game.tackles == 1U);
        assert(game.shots == 0U && game.passes == 0U);
        const int32_t slide_start = game.players[1].position.x;
        soccer_game_arcade_step(&game, (soccer_input_t) {.dx = 1});
        assert(game.players[1].position.x < slide_start); /* Committed slide cannot turn. */
        steps(&game, 18U, idle);
        const int32_t before = game.players[1].position.x;
        soccer_game_arcade_step(&game, (soccer_input_t) {.dx = 1});
        assert(game.players[1].position.x > before && game.owner == 1);
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true});
        assert(game.passes == 1U && game.owner == -1);
    }
}

static void arcade_full_matches(void)
{
    unsigned int restarts_seen = 0U;
    uint32_t tackles = 0U, passes = 0U, shots = 0U;
    for (unsigned int difficulty = 0U; difficulty < 3U; ++difficulty) {
        for (unsigned int trial = 0U; trial < 4U; ++trial) {
            soccer_game_t game;
            soccer_game_reset(&game);
            game.difficulty = (soccer_difficulty_t) difficulty;
            soccer_camera_t camera;
            soccer_camera_reset(&camera, &game);
            unsigned int tick = 0U;
            while (game.mode != SOCCER_FULL_TIME && tick < 30000U) {
                soccer_input_t buttons        = {0};
                const soccer_player_t* player = &game.players[game.controlled];
                soccer_vec_t target           = game.ball;
                if (game.owner == (int) game.controlled) {
                    target = (soccer_vec_t) {SOCCER_RIGHT * SOCCER_ONE, (440 + (int) trial * 25) * SOCCER_ONE};
                }
                if (abs(target.x - player->position.x) > 4 * SOCCER_ONE) {
                    buttons.dx = target.x > player->position.x ? 1 : -1;
                }
                if (abs(target.y - player->position.y) > 4 * SOCCER_ONE) {
                    buttons.dy = target.y > player->position.y ? 1 : -1;
                }
                const unsigned int phase          = (tick + trial * 11U) % 90U;
                buttons.pass                      = phase == 0U;
                buttons.pass_held                 = phase < (trial % 2U == 0U ? 1U : 22U);
                buttons.shoot                     = phase == 45U;
                buttons.shoot_held                = phase >= 45U && phase < 60U;
                const soccer_mode_t previous_mode = game.mode;
                const unsigned int remaining      = game.match_ticks;
                soccer_game_arcade_step(&game, buttons);
                const soccer_vec_t before_camera = camera.position;
                soccer_camera_step(&camera, &game);
                assert(abs(camera.position.x - before_camera.x) <= 8 * SOCCER_ONE);
                assert(abs(camera.position.y - before_camera.y) <= 8 * SOCCER_ONE);
                assert(game.controlled < SOCCER_TEAM_SIZE);
                assert(game.owner >= -1 && game.owner < SOCCER_PLAYER_COUNT);
                assert(game.match_ticks <= remaining);
                if (game.mode == SOCCER_PLAY && game.owner >= 0 && game.owner < SOCCER_TEAM_SIZE) {
                    assert(game.controlled == (unsigned int) game.owner);
                }
                if (game.mode == SOCCER_RESTART && previous_mode != SOCCER_RESTART) {
                    ++restarts_seen;
                }
                if (tick % 997U == 0U) {
                    game.paused              = true;
                    const unsigned int clock = game.match_ticks;
                    const soccer_vec_t ball  = game.ball;
                    for (unsigned int pause = 0U; pause < 5U; ++pause) {
                        soccer_game_arcade_step(&game, idle);
                    }
                    assert(game.match_ticks == clock && game.ball.x == ball.x && game.ball.y == ball.y);
                    game.paused = false;
                }
                ++tick;
            }
            assert(game.mode == SOCCER_FULL_TIME && game.match_ticks == 0U);
            tackles += game.tackles;
            passes  += game.passes;
            shots   += game.shots;
        }
    }
    assert(tackles > 0U && passes > 0U && shots > 0U && restarts_seen > 0U);
    printf("12 arcade matches: %u tackles, %u passes, %u shots, %u restarts\n", tackles, passes, shots, restarts_seen);
}

static void keeper_distribution(void)
{
    for (unsigned int team = 0U; team < 2U; ++team) {
        for (unsigned int blocked = 0U; blocked < 3U; ++blocked) {
            soccer_game_t game;
            soccer_game_reset(&game);
            game.mode           = SOCCER_PLAY;
            game.owner          = -1;
            game.keeper_owner   = (int) team;
            game.keeper_ticks   = 1U;
            game.ball           = game.keepers[team].position;
            const int direction = team == 0U ? 1 : -1;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {800 * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            unsigned int outlet              = team * SOCCER_TEAM_SIZE + 3U;
            game.players[outlet].position    = game.ball;
            game.players[outlet].position.x += direction * 200 * SOCCER_ONE;
            game.players[outlet].position.y += 70 * SOCCER_ONE;
            /* The old preferred player is unreachable; use a nearby defender. */
            if (blocked != 0U) {
                game.players[(1U - team) * SOCCER_TEAM_SIZE].position = game.players[outlet].position;
            }
            if (blocked == 2U) {
                ++outlet;
                game.players[outlet].position    = game.ball;
                game.players[outlet].position.x += direction * 240 * SOCCER_ONE;
                game.players[outlet].position.y -= 70 * SOCCER_ONE;
            }
            soccer_game_arcade_step(&game, idle);
            assert(game.keeper_owner == -1 && game.velocity.x * direction > 0);
            if (blocked == 1U) {
                assert(game.pass_target == -1 && game.velocity.y == 0);
            } else {
                assert(game.pass_target == (int) outlet && game.pass_ticks > 0U);
                for (unsigned int tick = 0U; tick < 120U && game.owner < 0; ++tick) {
                    soccer_game_arcade_step(&game, idle);
                }
                assert(game.owner == (int) outlet);
                if (team == 0U) {
                    assert(game.controlled == outlet && game.completed_passes == 1U && game.passes == 1U);
                }
            }
        }
    }
}

static void passing_support_runs(void)
{
    for (unsigned int defensive = 0U; defensive < 2U; ++defensive) {
        soccer_game_t game;
        soccer_game_reset(&game);
        game.mode                 = SOCCER_PLAY;
        const unsigned int passer = defensive == 0U ? 0U : SOCCER_DEFENDER_FIRST;
        game.controlled           = passer;
        game.owner                = (int) passer;
        for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
            game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
        }
        game.players[passer].position = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
        game.players[passer].facing_x = 1;
        game.players[passer].facing_y = 0;
        game.players[1].position      = (soccer_vec_t) {920 * SOCCER_ONE, 400 * SOCCER_ONE};
        game.ball                     = game.players[passer].position;
        soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true});
        assert(game.pass_target == 1);
        if (defensive != 0U) {
            assert(game.support_ticks[0] == 0U);
            continue;
        }
        assert(game.support_ticks[0] > 0U && game.support_runner[0] == passer);
        game.paused                  = true;
        const unsigned int remaining = game.support_ticks[0];
        soccer_game_arcade_step(&game, idle);
        assert(game.support_ticks[0] == remaining);
        game.paused = false;
        for (unsigned int tick = 0U; tick < 100U && game.owner < 0; ++tick) {
            soccer_game_arcade_step(&game, idle);
        }
        assert(game.owner == 1 && game.controlled == 1U);
        const soccer_vec_t before = game.players[passer].position;
        for (unsigned int tick = 0U; tick < 10U; ++tick) {
            soccer_game_arcade_step(&game, idle);
        }
        assert(game.players[passer].position.x > before.x);
        assert(game.players[passer].position.y > before.y);
        assert(game.controlled == 1U);
        game.owner = SOCCER_TEAM_SIZE;
        soccer_game_arcade_step(&game, idle);
        assert(game.support_ticks[0] == 0U);
        soccer_game_reset(&game);
        assert(game.support_ticks[0] == 0U && game.support_ticks[1] == 0U);
    }
}

static void stationary_ball_contests(void)
{
    static const int directions[][2] = {
        { 1,  0},
        { 1,  1},
        { 0,  1},
        {-1,  1},
        {-1,  0},
        {-1, -1},
        { 0, -1},
        { 1, -1}
    };
    for (unsigned int difficulty = 0U; difficulty < 3U; ++difficulty) {
        for (unsigned int direction = 0U; direction < 8U; ++direction) {
            for (unsigned int protected = 0U; protected < 2U; ++protected) {
                soccer_game_t game;
                soccer_game_reset(&game);
                game.mode             = SOCCER_PLAY;
                game.difficulty       = (soccer_difficulty_t) difficulty;
                game.owner            = 0;
                game.possession_ticks = protected != 0U ? 12U : 0U;
                for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                    game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
                }
                game.players[0].position                 = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
                game.players[0].facing_x                 = directions[direction][0];
                game.players[0].facing_y                 = directions[direction][1];
                game.ball                                = game.players[0].position;
                game.ball.x                             += directions[direction][0] * 14 * SOCCER_ONE;
                game.ball.y                             += directions[direction][1] * 14 * SOCCER_ONE;
                game.players[SOCCER_TEAM_SIZE].position  = game.ball;
                game.players[SOCCER_TEAM_SIZE].facing_x  = directions[direction][0];
                game.players[SOCCER_TEAM_SIZE].facing_y  = directions[direction][1];
                for (unsigned int tick = 0U; tick < 90U && game.owner == 0; ++tick) {
                    soccer_game_arcade_step(&game, idle);
                    if (protected != 0U && tick < 11U) {
                        assert(game.owner == 0);
                    }
                }
                /* The AI must face the carrier, not repeatedly tackle away from
                   them because it reached the ball and stopped steering. */
                assert(game.owner == SOCCER_TEAM_SIZE);
                assert(game.match_ticks < SOCCER_MATCH_TICKS);
            }
        }
    }
}

static void camera_acceleration(void)
{
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode = SOCCER_PLAY;
    soccer_camera_t camera;
    soccer_camera_reset(&camera, &game);
    assert(camera.velocity.x == 0 && camera.velocity.y == 0);
    game.ball.x = 1500 * SOCCER_ONE;
    for (unsigned int tick = 0U; tick < 12U; ++tick) {
        const int32_t speed = camera.velocity.x;
        soccer_camera_step(&camera, &game);
        assert(abs(camera.velocity.x - speed) <= SOCCER_ONE);
        assert(camera.velocity.x <= 8 * SOCCER_ONE);
    }
    assert(camera.velocity.x == 8 * SOCCER_ONE);
    game.ball.x = 100 * SOCCER_ONE;
    for (unsigned int tick = 0U; tick < 20U; ++tick) {
        const int32_t speed = camera.velocity.x;
        soccer_camera_step(&camera, &game);
        assert(abs(camera.velocity.x - speed) <= SOCCER_ONE);
    }
    assert(camera.velocity.x < 0);
    game.paused                  = true;
    const soccer_camera_t frozen = camera;
    soccer_camera_step(&camera, &game);
    assert(memcmp(&camera, &frozen, sizeof(camera)) == 0);
    game.paused = false;
    for (unsigned int tick = 0U; tick < 600U; ++tick) {
        soccer_camera_step(&camera, &game);
        assert(camera.position.x >= 0);
    }
    assert(camera.position.x == 0 && camera.velocity.x == 0);
    soccer_camera_reset(&camera, &game);
    assert(camera.velocity.x == 0 && camera.velocity.y == 0);
}

static void kickoff_pass_direction(void)
{
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            soccer_game_t game;
            soccer_game_reset(&game);
            const soccer_vec_t start = game.players[0].position;
            for (unsigned int tick = 0U; tick < 90U; ++tick) {
                soccer_game_arcade_step(&game, (soccer_input_t) {.dx = dx, .dy = dy});
            }
            assert(game.mode == SOCCER_PLAY);
            assert(game.players[0].position.x == start.x && game.players[0].position.y == start.y);
            assert(game.players[0].facing_x == dx && game.players[0].facing_y == dy);
            soccer_game_arcade_step(&game, (soccer_input_t) {.pass = true});
            assert(game.passes == 1U && game.owner == -1);
            assert(game.velocity.x * dx + game.velocity.y * dy > 0);
            if (dx != 0 && dy == 0) {
                assert(game.velocity.x * dx > 0);
            }
        }
    }
}

static void ai_slide_distance(void)
{
    for (unsigned int team = 0U; team < 2U; ++team) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                soccer_game_t game;
                soccer_game_reset(&game);
                game.mode             = SOCCER_PLAY;
                game.owner            = team == 0U ? SOCCER_TEAM_SIZE : 0;
                game.possession_ticks = 120U;
                for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                    game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
                }
                const unsigned int index = team * SOCCER_TEAM_SIZE + 1U;
                soccer_player_t* player  = &game.players[index];
                player->position         = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
                player->slide_ticks      = 18U;
                player->slide_x          = dx;
                player->slide_y          = dy;
                player->tackle_cooldown  = 42U;
                steps(&game, 18U, idle);
                const int distance = dx != 0 && dy != 0 ? 36 * 181 : 36 * SOCCER_ONE;
                assert(player->position.x == 800 * SOCCER_ONE + dx * distance);
                assert(player->position.y == 480 * SOCCER_ONE + dy * distance);
                assert(player->slide_ticks == 0U);
            }
        }
    }
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode             = SOCCER_PLAY;
    game.owner            = SOCCER_TEAM_SIZE;
    game.possession_ticks = 0U;
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    game.players[1].position                = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[1].slide_ticks             = 1U;
    game.players[1].slide_x                 = 1;
    game.players[1].tackle_cooldown         = 25U;
    game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {820 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                               = game.players[SOCCER_TEAM_SIZE].position;
    soccer_game_arcade_step(&game, (soccer_input_t) {.dx = 1});
    assert(game.controlled == 1U && game.owner == 1);
    assert(game.players[1].position.x == 801 * SOCCER_ONE);
    soccer_game_arcade_step(&game, (soccer_input_t) {.dx = 1});
    assert(game.players[1].position.x > 801 * SOCCER_ONE);
}

static void sliding_tackles(void)
{
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            soccer_game_t game;
            soccer_game_reset(&game);
            game.mode             = SOCCER_PLAY;
            game.owner            = SOCCER_TEAM_SIZE;
            game.possession_ticks = 120U;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            game.players[0].position = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
            game.players[0].facing_x = dx;
            game.players[0].facing_y = dy;
            soccer_game_arcade_step(&game, (soccer_input_t) {.shoot = true});
            assert(game.players[0].slide_ticks == 18U);
            const soccer_vec_t start = game.players[0].position;
            game.paused              = true;
            soccer_game_arcade_step(&game, idle);
            assert(game.players[0].slide_ticks == 18U && game.players[0].position.x == start.x);
            game.paused = false;
            for (unsigned int tick = 0U; tick < 18U; ++tick) {
                soccer_game_arcade_step(&game, (soccer_input_t) {.dx = -dx, .dy = -dy, .shoot_held = true});
            }
            const int distance = dx != 0 && dy != 0 ? 36 * 181 : 36 * SOCCER_ONE;
            assert(game.players[0].position.x == start.x + dx * distance);
            assert(game.players[0].position.y == start.y + dy * distance);
            assert(game.players[0].slide_ticks == 0U && game.players[0].tackle_cooldown > 0U);
            soccer_game_reset(&game);
            assert(game.players[0].slide_ticks == 0U);
        }
    }
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode             = SOCCER_PLAY;
    game.owner            = SOCCER_TEAM_SIZE;
    game.possession_ticks = 0U;
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        game.players[i].position = (soccer_vec_t) {100 * SOCCER_ONE, 100 * SOCCER_ONE};
    }
    game.players[0].position                = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {844 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.ball                               = game.players[SOCCER_TEAM_SIZE].position;
    soccer_game_arcade_step(&game, (soccer_input_t) {.shoot = true});
    assert(game.owner == SOCCER_TEAM_SIZE);
    for (unsigned int tick = 0U; tick < 12U && game.owner != 0; ++tick) {
        soccer_game_arcade_step(&game, (soccer_input_t) {.shoot_held = true});
    }
    assert(game.owner == 0 && game.tackles == 1U && game.shots == 0U);
    assert(game.players[0].slide_ticks > 0U && game.players[0].slide_hit);
    game.owner                                 = SOCCER_TEAM_SIZE;
    game.possession_ticks                      = 0U;
    game.players[SOCCER_TEAM_SIZE].position    = game.players[0].position;
    game.players[SOCCER_TEAM_SIZE].position.x += 15 * SOCCER_ONE;
    soccer_game_arcade_step(&game, idle);
    assert(game.owner == SOCCER_TEAM_SIZE && game.tackles == 1U); /* One win per slide. */
    game.players[0].position.x = (SOCCER_RIGHT - 8) * SOCCER_ONE;
    soccer_game_arcade_step(&game, idle);
    assert(game.players[0].position.x <= (SOCCER_RIGHT - 8) * SOCCER_ONE);
}

static void opponent_dribbling_space(void)
{
    for (unsigned int difficulty = 0U; difficulty < 3U; ++difficulty) {
        for (unsigned int scenario = 0U; scenario < 5U; ++scenario) {
            soccer_game_t game;
            soccer_game_reset(&game);
            game.mode             = SOCCER_PLAY;
            game.difficulty       = (soccer_difficulty_t) difficulty;
            game.owner            = SOCCER_TEAM_SIZE;
            game.possession_ticks = 60U;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {1200 * SOCCER_ONE, 800 * SOCCER_ONE};
            }
            const int y                             = scenario == 3U ? SOCCER_TOP + 30 : 458;
            game.players[SOCCER_TEAM_SIZE].position = (soccer_vec_t) {800 * SOCCER_ONE, y * SOCCER_ONE};
            game.ball                               = game.players[SOCCER_TEAM_SIZE].position;
            if (scenario != 0U) {
                game.players[0].position = (soccer_vec_t) {750 * SOCCER_ONE, y * SOCCER_ONE};
            }
            if (scenario == 2U || scenario == 4U) {
                game.players[1].position = (soccer_vec_t) {736 * SOCCER_ONE, (y - 56) * SOCCER_ONE};
            }
            if (scenario == 4U) {
                game.players[2].position = (soccer_vec_t) {736 * SOCCER_ONE, (y + 56) * SOCCER_ONE};
            }
            soccer_game_step(&game, idle);
            const soccer_vec_t position = game.players[SOCCER_TEAM_SIZE].position;
            assert(position.x < 800 * SOCCER_ONE);
            assert(game.owner == SOCCER_TEAM_SIZE);
            if (scenario == 0U || scenario == 4U) {
                assert(position.y == y * SOCCER_ONE);
            } else if (scenario == 1U) {
                assert(position.y < y * SOCCER_ONE);
            } else {
                assert(position.y > y * SOCCER_ONE);
            }
        }
    }
}

static void recovering_presser_support(void)
{
    for (unsigned int team = 0U; team < 2U; ++team) {
        for (unsigned int difficulty = 0U; difficulty < 3U; ++difficulty) {
            for (unsigned int scenario = 0U; scenario < 5U; ++scenario) {
                soccer_game_t game;
                soccer_game_reset(&game);
                game.mode                = SOCCER_PLAY;
                game.difficulty          = (soccer_difficulty_t) difficulty;
                const unsigned int base  = team * SOCCER_TEAM_SIZE;
                const unsigned int enemy = (1U - team) * SOCCER_TEAM_SIZE;
                game.owner               = (int) enemy;
                game.controlled          = team == 0U ? 4U : enemy;
                game.possession_ticks    = 0U;
                for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                    game.players[i].position = (soccer_vec_t) {1200 * SOCCER_ONE, 100 * SOCCER_ONE};
                }
                game.ball                          = (soccer_vec_t) {800 * SOCCER_ONE, 480 * SOCCER_ONE};
                game.players[enemy].position       = game.ball;
                game.players[base].position        = game.ball;
                game.players[base].tackle_cooldown = 35U;
                game.players[base + 1U].position   = (soccer_vec_t) {820 * SOCCER_ONE, 480 * SOCCER_ONE};
                if (scenario == 1U) {
                    game.players[base + 1U].tackle_cooldown = 35U;
                } else if (scenario == 2U) {
                    game.players[base + 1U].jump_ticks = 20U;
                } else if (scenario == 4U) {
                    game.players[base + 1U].position.x = 950 * SOCCER_ONE;
                } else if (scenario == 3U && team == 0U) {
                    game.controlled = base;
                }
                soccer_game_step(&game, idle);
                if (scenario == 0U || (scenario == 3U && team == 1U)) {
                    assert(game.owner == (int) (base + 1U));
                } else {
                    assert(game.owner == (int) enemy);
                }
            }
        }
    }
}

static void opponent_shot_choices(void)
{
    for (unsigned int difficulty = 0U; difficulty < 3U; ++difficulty) {
        for (unsigned int blocked = 0U; blocked < 4U; ++blocked) {
            soccer_game_t game;
            soccer_game_reset(&game);
            game.mode             = SOCCER_PLAY;
            game.difficulty       = (soccer_difficulty_t) difficulty;
            game.owner            = SOCCER_TEAM_SIZE;
            game.possession_ticks = 0U;
            for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
                game.players[i].position = (soccer_vec_t) {800 * SOCCER_ONE, 100 * SOCCER_ONE};
            }
            game.players[SOCCER_TEAM_SIZE].position      = (soccer_vec_t) {210 * SOCCER_ONE, 458 * SOCCER_ONE};
            game.ball                                    = game.players[SOCCER_TEAM_SIZE].position;
            game.players[SOCCER_TEAM_SIZE + 1U].position = (soccer_vec_t) {150 * SOCCER_ONE, 600 * SOCCER_ONE};
            if (blocked > 0U) {
                game.players[0].position = (soccer_vec_t) {65 * SOCCER_ONE, 510 * SOCCER_ONE};
            }
            if (blocked > 1U) {
                game.players[1].position = (soccer_vec_t) {65 * SOCCER_ONE, 446 * SOCCER_ONE};
            }
            if (blocked > 2U) {
                game.players[2].position = game.players[SOCCER_TEAM_SIZE + 1U].position;
            }
            soccer_game_arcade_step(&game, idle);
            if (blocked < 2U) {
                assert(game.stats[1].shots == 1U && game.owner == -1);
                assert(game.velocity.x < 0);
                assert((game.velocity.y > 0) == (blocked == 0U));
            } else if (blocked == 2U) {
                assert(game.stats[1].shots == 0U && game.pass_target == SOCCER_TEAM_SIZE + 1);
            } else {
                assert(game.stats[1].shots == 0U && game.owner == SOCCER_TEAM_SIZE);
                assert(game.players[SOCCER_TEAM_SIZE].moving);
            }
        }
    }
}

int main(void)
{
    kickoff_pass_direction();
    ai_slide_distance();
    sliding_tackles();
    opponent_dribbling_space();
    recovering_presser_support();
    opponent_shot_choices();
    camera_acceleration();
    stationary_ball_contests();
    passing_support_runs();
    keeper_distribution();
    arcade_full_matches();
    tackle_recovery_control();
    opposing_player_spacing();
    automatic_control();
    arcade_buttons();
    defensive_switching();
    sprinting();
    difficulty_levels();
    match_statistics();
    keeper_parries();
    opponent_through_passes();
    through_passes();
    defensive_marking();
    goal_relative_shots();
    charged_shots();
    attacking_options();
    keeper_dives();
    heading();
    aerial_ball();
    restarts();
    action_feedback();
    expanded_squad_reception();
    tactics();
    matches();
    keeping();
    opposition();
    passing();
    camera_selection_stability();
    camera_motion();
    movement();
    possession_and_goals();
    goal_geometry_and_swept_scoring();
    physics();
    puts("Soccer rules passed");
    return 0;
}
