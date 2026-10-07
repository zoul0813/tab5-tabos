#include <pool/game.h>
#include <pool/input.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pool_rule_result_t shot(pool_rules_t* rules, uint16_t before, uint16_t after, unsigned int first, bool rail,
                               const unsigned int* order, unsigned int count)
{
    pool_pot_events_t pots = {0};
    for (unsigned int i = 0U; i < count; ++i) {
        pots.ball[pots.count++] = (uint8_t) order[i];
    }
    const pool_contact_state_t contacts = {first, rail};
    pool_rules_begin(rules, before);
    const pool_rule_result_t result = pool_rules_finish(rules, after, &pots, &contacts);
    const pool_rules_t finished     = *rules;
    assert(pool_rules_finish(rules, after, &pots, &contacts) == POOL_RULE_IDLE);
    assert(memcmp(&finished, rules, sizeof(finished)) == 0);
    return result;
}

static void truth_table(void)
{
    const unsigned int mixed[] = {9U, 2U};
    for (unsigned int player = 0U; player < 2U; ++player) {
        pool_rules_t rules;
        pool_rules_reset(&rules);
        rules.player = player;
        assert(shot(&rules, 0U, (1U << 2U), 1U, false, mixed + 1, 1U) == POOL_KEEP_TURN);
        assert(!rules.break_shot && rules.groups[player] == POOL_OPEN && rules.player == player);
        assert(shot(&rules, 1U << 2U, (1U << 2U) | (1U << 9U), 3U, false, mixed, 1U) == POOL_KEEP_TURN);
        assert(rules.groups[player] == POOL_STRIPES && rules.groups[player ^ 1U] == POOL_SOLIDS);
        assert(shot(&rules, (1U << 2U) | (1U << 9U), (1U << 2U) | (1U << 9U) | (1U << 3U), 10U, false, NULL, 0U) ==
               POOL_CHANGE_TURN);
        assert(rules.player == (player ^ 1U));
        rules.player = player;
        assert(shot(&rules, 0U, (1U << 10U) | (1U << 3U), 10U, false, NULL, 0U) == POOL_KEEP_TURN);
        const pool_foul_t fouls[]  = {POOL_SCRATCH, POOL_NO_CONTACT, POOL_WRONG_FIRST, POOL_NO_RAIL};
        const unsigned int first[] = {0U, 0U, 3U, 10U};
        for (unsigned int i = 0U; i < 4U; ++i) {
            rules.player          = player;
            const uint16_t potted = i == 0U ? 1U : 0U;
            assert(shot(&rules, 0U, potted, first[i], false, NULL, 0U) == POOL_BALL_IN_HAND);
            assert(rules.foul == fouls[i] && rules.player == (player ^ 1U));
        }
        pool_rules_reset(&rules);
        rules.player     = player;
        rules.break_shot = false;
        assert(shot(&rules, 0U, (1U << 2U) | (1U << 9U), 1U, false, mixed, 2U) == POOL_KEEP_TURN);
        assert(rules.groups[player] == POOL_STRIPES);
        pool_rules_reset(&rules);
        rules.break_shot = false;
        assert(shot(&rules, 0U, (1U << 9U) | 1U, 1U, true, mixed, 1U) == POOL_BALL_IN_HAND);
        assert(rules.groups[0] == POOL_OPEN && rules.groups[1] == POOL_OPEN);
        pool_rules_reset(&rules);
        assert(shot(&rules, 0U, 0U, 8U, true, NULL, 0U) == POOL_BALL_IN_HAND && rules.foul == POOL_WRONG_FIRST);
        pool_rules_reset(&rules);
        rules.player = player;
        assert(shot(&rules, 0U, 0U, 1U, true, NULL, 0U) == POOL_CHANGE_TURN);
        assert(rules.player == (player ^ 1U) && rules.foul == POOL_FAIR);
    }
}

static void eight_ball(void)
{
    const uint16_t solid_mask = UINT16_C(0x00fe);
    for (unsigned int player = 0U; player < 2U; ++player) {
        for (unsigned int scratch = 0U; scratch < 2U; ++scratch) {
            pool_rules_t rules;
            pool_rules_reset(&rules);
            rules.player = player;
            assert(shot(&rules, 0U, (uint16_t) ((1U << 8U) | scratch), 1U, true, NULL, 0U) == POOL_RERACK);
            assert(rules.break_shot && rules.player == player && rules.reracked && !rules.complete);
            rules.break_shot          = false;
            rules.groups[player]      = POOL_SOLIDS;
            rules.groups[player ^ 1U] = POOL_STRIPES;
            assert(shot(&rules, solid_mask, (uint16_t) (solid_mask | (1U << 8U) | scratch), 8U, false, NULL, 0U) ==
                   POOL_RESULT);
            assert(rules.complete && rules.winner == (player ^ scratch));
        }
        pool_rules_t rules;
        pool_rules_reset(&rules);
        rules.break_shot     = false;
        rules.player         = player;
        rules.groups[player] = POOL_SOLIDS;
        /* Even if the final solid is captured first, 8 was not legal at start. */
        const unsigned int order[] = {7U, 8U};
        assert(shot(&rules, solid_mask & (uint16_t) ~(1U << 7U), solid_mask | (1U << 8U), 7U, false, order, 2U) ==
               POOL_RESULT);
        assert(rules.early_eight && rules.winner == (player ^ 1U));
        pool_rules_reset(&rules);
        rules.break_shot     = false;
        rules.player         = player;
        rules.groups[player] = POOL_SOLIDS;
        assert(shot(&rules, solid_mask, solid_mask | 1U, 8U, true, NULL, 0U) == POOL_BALL_IN_HAND);
        assert(!rules.complete); /* Scratch without potting 8 does not lose. */
        rules.player = player;
        assert(shot(&rules, solid_mask, solid_mask | (1U << 8U), 9U, true, NULL, 0U) == POOL_RESULT);
        assert(rules.winner == (player ^ 1U) && rules.foul == POOL_WRONG_FIRST);
    }
}

static void complete_racks(void)
{
    for (unsigned int winner = 0U; winner < 2U; ++winner) {
        pool_rules_t rules;
        pool_rules_reset(&rules);
        assert(shot(&rules, 0U, 0U, 1U, true, NULL, 0U) == POOL_CHANGE_TURN);
        if (winner == 0U) {
            assert(shot(&rules, 0U, 0U, 2U, true, NULL, 0U) == POOL_CHANGE_TURN);
        }
        uint16_t potted = 0U;
        for (unsigned int ball = 1U; ball <= 7U; ++ball) {
            assert(pool_rules_legal_target(&rules, potted, ball));
            const uint16_t after = potted | (uint16_t) (1U << ball);
            assert(shot(&rules, potted, after, ball, false, &ball, 1U) == POOL_KEEP_TURN);
            potted = after;
            assert(rules.player == winner && pool_rules_remaining(POOL_SOLIDS, potted) == 7U - ball);
        }
        assert(pool_rules_legal_target(&rules, potted, 8U));
        assert(!pool_rules_legal_target(&rules, potted, 9U));
        assert(shot(&rules, potted, potted | (1U << 8U), 8U, false, NULL, 0U) == POOL_RESULT);
        assert(rules.complete && rules.winner == winner);
        assert(!pool_rules_legal_target(&rules, potted, 8U));
    }
}

static bool press(pool_input_t* input, pool_game_t* game, tabos_key_t key)
{
    const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = key};
    return pool_input_event(input, game, &down);
}

static void release(pool_input_t* input, pool_game_t* game, tabos_key_t key)
{
    const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = key};
    (void) pool_input_event(input, game, &up);
}

static void lifecycle(void)
{
    pool_game_t game;
    pool_game_reset(&game);
    pool_input_t input = {0};
    game.power         = 1U;
    assert(pool_game_shoot(&game));
    assert(press(&input, &game, TABOS_KEY_R));
    const pool_game_t frozen = game;
    for (unsigned int tick = 0U; tick < 100U; ++tick) {
        assert(!pool_game_step(&game));
    }
    assert(memcmp(&game, &frozen, sizeof(game)) == 0);
    assert(press(&input, &game, TABOS_KEY_ESCAPE));
    assert(!game.restart_pending && !input.quit && pool_game_step(&game));
    release(&input, &game, TABOS_KEY_R);
    while (game.shot_set) {
        assert(pool_game_step(&game));
    }
    assert(game.rules.foul == POOL_NO_CONTACT && game.rules.player == 1U && game.placement);
    assert(pool_game_confirm_placement(&game));
    assert(pool_game_can_aim(&game));
    game.paused = true;
    assert(press(&input, &game, TABOS_KEY_R));
    release(&input, &game, TABOS_KEY_R);
    assert(press(&input, &game, TABOS_KEY_R));
    assert(!game.restart_pending && game.paused);
    release(&input, &game, TABOS_KEY_R);
    assert(press(&input, &game, TABOS_KEY_ENTER) == false);
    assert(press(&input, &game, TABOS_KEY_R));
    assert(!press(&input, &game, TABOS_KEY_ENTER));
    assert(game.restart_pending);
    release(&input, &game, TABOS_KEY_ENTER);
    assert(press(&input, &game, TABOS_KEY_ENTER));
    assert(!game.restart_pending && !game.paused && game.rules.break_shot && game.rules.player == 0U);
    game.rules.complete = true;
    game.rules.winner   = 1U;
    assert(!pool_game_shoot(&game) && !pool_game_adjust(&game, 1, 1, false));
    assert(!pool_game_step(&game));
    assert(!press(&input, &game, TABOS_KEY_ENTER));
    release(&input, &game, TABOS_KEY_ENTER);
    assert(press(&input, &game, TABOS_KEY_ENTER));
    assert(!game.rules.complete && pool_game_can_aim(&game));
}

static void trace_parity(void)
{
    unsigned int hits = 0U, rails = 0U;
    for (unsigned int run = 0U; run < 32U; ++run) {
        pool_game_t game;
        pool_game_reset(&game);
        game.power = 100U;
        game.angle = (POOL_ANGLE_COUNT + run * 4U - 64U) % POOL_ANGLE_COUNT;
        assert(pool_game_shoot(&game));
        pool_ball_t plain[POOL_BALL_COUNT];
        memcpy(plain, game.balls, sizeof(plain));
        uint16_t potted = 0U;
        for (unsigned int tick = 0U; tick < 500U; ++tick) {
            pool_pot_events_t a, b;
            const bool moving_a =
                pool_physics_match_step(game.balls, POOL_BALL_COUNT, &game.potted, &a, &game.contacts);
            const bool moving_b = pool_physics_play_step(plain, POOL_BALL_COUNT, &potted, &b);
            assert(moving_a == moving_b && potted == game.potted);
            assert(memcmp(plain, game.balls, sizeof(plain)) == 0);
            assert(a.count == b.count);
            for (unsigned int i = 0U; i < a.count; ++i) {
                assert(a.ball[i] == b.ball[i] && a.pocket[i] == b.pocket[i]);
            }
            if (!moving_a) {
                break;
            }
        }
        hits  += game.contacts.first != 0U ? 1U : 0U;
        rails += game.contacts.rail_after ? 1U : 0U;
    }
    assert(hits == 32U && rails > 0U);
    /* Rail before any cue contact must not satisfy the rule. */
    pool_ball_t ball = {
        {100 * POOL_ONE,   8 * POOL_ONE},
        {             0, -10 * POOL_ONE}
    };
    uint16_t potted = 0U;
    pool_pot_events_t events;
    pool_contact_state_t contacts = {0};
    (void) pool_physics_match_step(&ball, 1U, &potted, &events, &contacts);
    assert(!contacts.rail_after && contacts.first == 0U);
    ball.position.y = 8 * POOL_ONE;
    ball.velocity.y = -10 * POOL_ONE;
    contacts.first  = 1U;
    (void) pool_physics_match_step(&ball, 1U, &potted, &events, &contacts);
    assert(contacts.rail_after);
    puts("32 traced rack trajectories preserve every position, velocity and pot event");
}

static void integrated_outcomes(void)
{
    for (unsigned int mode = 0U; mode < 3U; ++mode) {
        pool_game_t game;
        pool_game_reset(&game);
        game.potted            = UINT16_C(0xfefe); /* Only cue and 8 active. */
        game.balls[0].position = (pool_vec_t) {264 * POOL_ONE, 244 * POOL_ONE};
        game.balls[8].position = (pool_vec_t) {264 * POOL_ONE, 259 * POOL_ONE};
        game.angle             = 1024U;
        game.power             = 100U;
        if (mode != 2U) {
            game.rules.break_shot = false;
            game.rules.groups[0]  = POOL_SOLIDS;
            game.rules.groups[1]  = POOL_STRIPES;
        }
        if (mode == 1U) {
            game.potted &= (uint16_t) ~(1U << 1U);
        }
        assert(pool_game_shoot(&game));
        for (unsigned int tick = 0U; tick < 400U && game.shot_set; ++tick) {
            assert(pool_game_step(&game));
        }
        assert(!game.shot_set);
        if (mode == 2U) {
            assert(game.rules.reracked && game.rules.break_shot && !game.rules.complete && game.potted == 0U);
            assert(game.balls[0].position.x == pool_initial_balls[0].x * POOL_ONE);
        } else {
            assert(game.rules.complete && game.rules.winner == mode);
            assert(game.contacts.first == 8U && !game.placement);
            assert(!pool_game_shoot(&game));
        }
    }
}

int main(void)
{
    truth_table();
    eight_ball();
    complete_racks();
    lifecycle();
    trace_parity();
    integrated_outcomes();
    puts("Pool turns, groups, fouls, 8-ball outcomes, restart and trace tests passed");
    return 0;
}
