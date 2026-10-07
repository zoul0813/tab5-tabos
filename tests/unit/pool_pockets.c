#include <pool/game.h>
#include <pool/input.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t energy(const pool_ball_t* balls, unsigned int count)
{
    uint64_t result = 0U;
    for (unsigned int i = 0U; i < count; ++i) {
        const pool_vec_t v  = balls[i].velocity;
        result             += (uint64_t) ((int64_t) v.x * v.x + (int64_t) v.y * v.y);
    }
    return result;
}

static void mouths(void)
{
    for (unsigned int pocket = 0U; pocket < POOL_POCKET_COUNT; ++pocket) {
        const pool_pocket_t* p  = &pool_pockets[pocket];
        const pool_vec_t middle = {(p->mouth_a.x + p->mouth_b.x) * POOL_ONE / 2,
                                   (p->mouth_a.y + p->mouth_b.y) * POOL_ONE / 2};
        const int sx            = (middle.x > p->center.x * POOL_ONE) - (middle.x < p->center.x * POOL_ONE);
        const int sy            = (middle.y > p->center.y * POOL_ONE) - (middle.y < p->center.y * POOL_ONE);
        for (unsigned int power = 1U; power <= 100U; ++power) {
            const int32_t speed     = POOL_ONE / 2 + (int32_t) (power - 1U) * (19 * POOL_ONE / 2) / 99;
            const int32_t component = sx != 0 && sy != 0 ? speed * 2896 / POOL_ONE : speed;
            pool_ball_t ball        = {
                {middle.x + sx * POOL_ONE, middle.y + sy * POOL_ONE},
                {         -sx * component,          -sy * component}
            };
            uint16_t potted = 0U;
            pool_pot_events_t events;
            for (unsigned int tick = 0U; tick < 60U && potted == 0U; ++tick) {
                const uint64_t before = energy(&ball, 1U);
                (void) pool_physics_play_step(&ball, 1U, &potted, &events);
                assert(energy(&ball, 1U) <= before);
                if (events.count != 0U) {
                    assert(events.count == 1U && events.ball[0] == 0U && events.pocket[0] == pocket);
                }
            }
            assert(potted == 1U && energy(&ball, 1U) == 0U);
            const pool_ball_t removed = ball;
            for (unsigned int tick = 0U; tick < 20U; ++tick) {
                assert(!pool_physics_play_step(&ball, 1U, &potted, &events));
                assert(events.count == 0U && memcmp(&removed, &ball, sizeof(ball)) == 0);
            }
        }
    }
    puts("600 slow-to-fast center-mouth pots and one-time removal passed");
}

static void jaws_and_misses(void)
{
    for (unsigned int i = 0U; i < POOL_CUSHION_COUNT; ++i) {
        const pool_cushion_t* c = &pool_cushions[i];
        const bool horizontal   = c->a.y == c->b.y;
        const int sign          = (horizontal ? c->a.y : c->a.x) == 0 ? 1 : -1;
        for (unsigned int endpoint = 0U; endpoint < 2U; ++endpoint) {
            const pool_point_t center = endpoint == 0U ? c->a : c->b;
            for (int speed = 1; speed <= 10; ++speed) {
                pool_ball_t ball = {
                    {center.x * POOL_ONE, center.y * POOL_ONE},
                    {                  0,                   0}
                };
                if (horizontal) {
                    ball.position.y += sign * 10 * POOL_ONE;
                    ball.velocity.y  = -sign * speed * POOL_ONE;
                } else {
                    ball.position.x += sign * 10 * POOL_ONE;
                    ball.velocity.x  = -sign * speed * POOL_ONE;
                }
                uint16_t potted = 0U;
                pool_pot_events_t events;
                for (unsigned int tick = 0U; tick < 12U; ++tick) {
                    const uint64_t before = energy(&ball, 1U);
                    (void) pool_physics_play_step(&ball, 1U, &potted, &events);
                    assert(potted == 0U && energy(&ball, 1U) <= before);
                }
                assert(sign * (horizontal ? ball.position.y - center.y * POOL_ONE :
                                            ball.position.x - center.x * POOL_ONE) >=
                       (POOL_BALL_RADIUS + POOL_JAW_RADIUS) * POOL_ONE);
            }
        }
    }
    /* Passing across a side mouth on the cloth must not trigger capture. */
    for (int y = 10; y <= 18; ++y) {
        pool_ball_t ball = {
            {240 * POOL_ONE, y * POOL_ONE},
            { 10 * POOL_ONE,            0}
        };
        uint16_t potted = 0U;
        pool_pot_events_t events;
        for (unsigned int tick = 0U; tick < 10U; ++tick) {
            (void) pool_physics_play_step(&ball, 1U, &potted, &events);
        }
        assert(potted == 0U && ball.position.x > 280 * POOL_ONE);
    }
    /* Open and closed models are bit-identical on the middle of every rail. */
    const pool_ball_t fixtures[] = {
        {  {100 * POOL_ONE, 8 * POOL_ONE}, {777, -8 * POOL_ONE}},
        {{100 * POOL_ONE, 256 * POOL_ONE},  {777, 8 * POOL_ONE}},
        {  {8 * POOL_ONE, 132 * POOL_ONE}, {-8 * POOL_ONE, 777}},
        {{520 * POOL_ONE, 132 * POOL_ONE},  {8 * POOL_ONE, 777}}
    };
    for (unsigned int i = 0U; i < 4U; ++i) {
        pool_ball_t open = fixtures[i], closed = fixtures[i];
        uint16_t potted = 0U;
        pool_pot_events_t events;
        for (unsigned int tick = 0U; tick < 30U; ++tick) {
            (void) pool_physics_play_step(&open, 1U, &potted, &events);
            pool_physics_step(&closed);
            assert(memcmp(&open, &closed, sizeof(open)) == 0);
        }
    }
}

static void oblique_mouths(void)
{
    unsigned int pots = 0U, rebounds = 0U;
    for (unsigned int pocket = 0U; pocket < POOL_POCKET_COUNT; ++pocket) {
        const pool_pocket_t* p  = &pool_pockets[pocket];
        const pool_vec_t middle = {(p->mouth_a.x + p->mouth_b.x) * POOL_ONE / 2,
                                   (p->mouth_a.y + p->mouth_b.y) * POOL_ONE / 2};
        const int sx            = (middle.x > p->center.x * POOL_ONE) - (middle.x < p->center.x * POOL_ONE);
        const int sy            = (middle.y > p->center.y * POOL_ONE) - (middle.y < p->center.y * POOL_ONE);
        const int32_t nx        = sx * (sx != 0 && sy != 0 ? 2896 : POOL_ONE);
        const int32_t ny        = sy * (sx != 0 && sy != 0 ? 2896 : POOL_ONE);
        for (int offset = -20; offset <= 20; ++offset) {
            for (int lateral = -4; lateral <= 4; ++lateral) {
                pool_ball_t ball = {
                    {middle.x + nx * 20 - ny * offset, middle.y + ny * 20 + nx * offset},
                    {          -nx * 7 - ny * lateral,           -ny * 7 + nx * lateral}
                };
                uint16_t potted = 0U;
                pool_pot_events_t events;
                unsigned int ticks = 0U;
                while (ticks++ < 500U) {
                    const uint64_t before = energy(&ball, 1U);
                    const bool moving     = pool_physics_play_step(&ball, 1U, &potted, &events);
                    assert(energy(&ball, 1U) <= before);
                    if (potted == 0U) {
                        assert(ball.position.x >= 0 && ball.position.y >= 0);
                        assert(ball.position.x <= POOL_TABLE_WIDTH * POOL_ONE &&
                               ball.position.y <= POOL_TABLE_HEIGHT * POOL_ONE);
                    } else {
                        assert(events.count == 1U && events.pocket[0] < POOL_POCKET_COUNT);
                    }
                    if (!moving) {
                        break;
                    }
                }
                assert(ticks < 500U);
                pots     += potted != 0U ? 1U : 0U;
                rebounds += potted == 0U ? 1U : 0U;
            }
        }
    }
    assert(pots > 0U && rebounds > 0U);
    printf("2214 oblique mouth cases: %u pots, %u returns to cloth\n", pots, rebounds);
}

static bool key(pool_input_t* input, pool_game_t* game, tabos_key_t key, bool down)
{
    const tabos_input_event_t event = {.type = down ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP, .key = key};
    return pool_input_event(input, game, &event);
}

static void scratch_and_records(void)
{
    pool_game_t game;
    pool_game_reset(&game);
    game.balls[0] = (pool_ball_t) {
        {264 * POOL_ONE,       POOL_ONE},
        {             0, -10 * POOL_ONE}
    };
    game.balls[1] = (pool_ball_t) {
        {264 * POOL_ONE, 263 * POOL_ONE},
        {             0,  10 * POOL_ONE}
    };
    game.balls[2].velocity.x = POOL_ONE;
    game.shot_set            = true;
    assert(pool_game_step(&game));
    assert((game.potted & 3U) == 3U && game.shot_pots.count == 2U);
    assert(game.shot_pots.ball[0] == 0U && game.shot_pots.pocket[0] == 1U);
    assert(game.shot_pots.ball[1] == 1U && game.shot_pots.pocket[1] == 4U);
    assert(game.shot_set && !game.placement && !pool_game_can_aim(&game));
    pool_input_t input = {0};
    assert(!key(&input, &game, TABOS_KEY_ENTER, true));
    assert(!pool_game_confirm_placement(&game));
    game.paused              = true;
    const pool_game_t paused = game;
    assert(!pool_game_step(&game) && memcmp(&game, &paused, sizeof(game)) == 0);
    game.paused = false;
    for (unsigned int tick = 0U; tick < 100U && game.shot_set; ++tick) {
        (void) pool_game_step(&game);
    }
    assert(game.placement && game.shot_pots.count == 2U);
    assert(pool_game_placement_valid(&game));
    assert(!key(&input, &game, TABOS_KEY_ENTER, true)); /* Held during motion, no automatic confirm. */
    game.placement_position = game.balls[3].position;
    assert(!pool_game_placement_valid(&game) && !pool_game_confirm_placement(&game));
    game.placement_position = (pool_vec_t) {7 * POOL_ONE, 7 * POOL_ONE};
    assert(!pool_game_placement_valid(&game));
    game.placement_position = (pool_vec_t) {264 * POOL_ONE, -POOL_ONE};
    assert(!pool_game_placement_valid(&game));
    game.placement_position = (pool_vec_t) {132 * POOL_ONE, 132 * POOL_ONE};
    assert(key(&input, &game, TABOS_KEY_RIGHT, true));
    assert(game.placement_position.x == 134 * POOL_ONE);
    (void) key(&input, &game, TABOS_KEY_RIGHT, false);
    assert(!pool_game_shoot(&game));
    (void) key(&input, &game, TABOS_KEY_ENTER, false);
    assert(key(&input, &game, TABOS_KEY_ENTER, true));
    assert(!game.placement && game.potted == 2U && pool_game_can_aim(&game));
    assert(game.balls[0].position.x == 134 * POOL_ONE);
    (void) key(&input, &game, TABOS_KEY_ENTER, false);
    assert(pool_game_shoot(&game) && game.shot_pots.count == 0U);
    /* Removed object cannot collide even if its stored position overlaps cue. */
    game.balls[1].position   = game.balls[0].position;
    const pool_vec_t removed = game.balls[1].position;
    (void) pool_game_step(&game);
    assert(game.balls[1].position.x == removed.x && game.balls[1].position.y == removed.y);
    assert(game.potted == 2U);
    pool_game_reset(&game);
    assert(game.potted == 0U && game.shot_pots.count == 0U && !game.placement);
}

static void open_table_stress(void)
{
    for (unsigned int run = 0U; run < 128U; ++run) {
        pool_game_t game;
        pool_game_reset(&game);
        game.angle = run * 31U;
        game.power = 100U;
        assert(pool_game_shoot(&game));
        unsigned int ticks = 0U;
        while (game.shot_set && ticks++ < 700U) {
            const uint64_t before = energy(game.balls, POOL_BALL_COUNT);
            assert(pool_game_step(&game));
            assert(energy(game.balls, POOL_BALL_COUNT) <= before);
            assert(game.shot_pots.count <= POOL_BALL_COUNT);
            for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
                if ((game.potted & (1U << i)) == 0U) {
                    assert(game.balls[i].position.x >= 0 && game.balls[i].position.x <= POOL_TABLE_WIDTH * POOL_ONE);
                    assert(game.balls[i].position.y >= 0 && game.balls[i].position.y <= POOL_TABLE_HEIGHT * POOL_ONE);
                }
            }
        }
        assert(ticks < 700U);
        assert(game.placement || pool_game_can_aim(&game));
    }
    puts("128 open-table full-power trajectories settle without escape or energy gain");
}

int main(void)
{
    mouths();
    jaws_and_misses();
    oblique_mouths();
    scratch_and_records();
    open_table_stress();
    puts("Pool pots, jaws, removal, scratch and placement passed");
    return 0;
}
