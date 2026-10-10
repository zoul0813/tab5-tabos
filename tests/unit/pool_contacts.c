#include <pool/game.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static bool closed_step(pool_game_t* game)
{
    if (game->paused || !game->shot_set) {
        return false;
    }
    game->shot_set = pool_physics_world_step(game->balls, POOL_BALL_COUNT);
    return true;
}

static uint64_t energy(const pool_ball_t* balls, unsigned int count)
{
    uint64_t result = 0U;
    for (unsigned int i = 0U; i < count; ++i) {
        const pool_vec_t v  = balls[i].velocity;
        result             += (uint64_t) ((int64_t) v.x * v.x + (int64_t) v.y * v.y);
    }
    return result;
}

static void legal(const pool_ball_t* balls, unsigned int count)
{
    for (unsigned int i = 0U; i < count; ++i) {
        assert(balls[i].position.x >= 7 * POOL_ONE && balls[i].position.x <= 521 * POOL_ONE);
        assert(balls[i].position.y >= 7 * POOL_ONE && balls[i].position.y <= 257 * POOL_ONE);
    }
}

static int32_t overlap(const pool_ball_t* balls, unsigned int count)
{
    int32_t worst = 0;
    for (unsigned int i = 0U; i < count; ++i) {
        for (unsigned int j = i + 1U; j < count; ++j) {
            const int64_t dx        = balls[i].position.x - balls[j].position.x;
            const int64_t dy        = balls[i].position.y - balls[j].position.y;
            const int64_t distance2 = dx * dx + dy * dy;
            int32_t low = 0, high = 14 * POOL_ONE;
            while (low < high) {
                const int32_t middle = low + (high - low + 1) / 2;
                if ((int64_t) middle * middle <= distance2) {
                    low = middle;
                } else {
                    high = middle - 1;
                }
            }
            if (14 * POOL_ONE - low > worst) {
                worst = 14 * POOL_ONE - low;
            }
        }
    }
    return worst;
}

static void pairs(void)
{
    pool_ball_t pair[] = {
        {{200 * POOL_ONE, 132 * POOL_ONE}, {10 * POOL_ONE, 0}},
        {{214 * POOL_ONE, 132 * POOL_ONE},             {0, 0}}
    };
    assert(pool_physics_world_step(pair, 2U));
    assert(pair[0].velocity.x == 10 * POOL_ONE - 10 * POOL_ONE * 197 / 200 - POOL_FRICTION);
    assert(pair[1].velocity.x == 10 * POOL_ONE * 197 / 200 - POOL_FRICTION);
    assert(pair[0].velocity.y == 0 && pair[1].velocity.y == 0);
    assert(overlap(pair, 2U) == 0);
    pair[0] = (pool_ball_t) {
        {200 * POOL_ONE, 132 * POOL_ONE},
        { 10 * POOL_ONE,              0}
    };
    pair[1] = (pool_ball_t) {
        {214 * POOL_ONE, 132 * POOL_ONE},
        {-10 * POOL_ONE,              0}
    };
    (void) pool_physics_world_step(pair, 2U);
    assert(pair[0].velocity.x == -(10 * POOL_ONE * 97 / 100 - POOL_FRICTION));
    assert(pair[0].velocity.x == -pair[1].velocity.x);
    /* Departing overlap separates without reversing velocities. */
    pair[0] = (pool_ball_t) {
        {200 * POOL_ONE, 132 * POOL_ONE},
        {     -POOL_ONE,              0}
    };
    pair[1] = (pool_ball_t) {
        {213 * POOL_ONE, 132 * POOL_ONE},
        {      POOL_ONE,              0}
    };
    (void) pool_physics_world_step(pair, 2U);
    assert(pair[0].velocity.x == -POOL_ONE + POOL_FRICTION);
    assert(pair[1].velocity.x == POOL_ONE - POOL_FRICTION);
    assert(overlap(pair, 2U) == 0);
    /* Coincident and slightly overlapping centers resolve without energy. */
    for (int32_t gap = 0; gap < 14 * POOL_ONE; gap += 997) {
        pair[0] = (pool_ball_t) {
            {200 * POOL_ONE, 132 * POOL_ONE},
            {             0,              0}
        };
        pair[1] = (pool_ball_t) {
            {200 * POOL_ONE + gap, 132 * POOL_ONE},
            {                   0,              0}
        };
        (void) pool_physics_world_step(pair, 2U);
        assert(energy(pair, 2U) == 0U && overlap(pair, 2U) <= 2);
    }
}

static void grazing(void)
{
    unsigned int tested = 0U;
    /* Narrow chords can lie entirely between substep endpoints. Both speed
     * signs and different launch phases must detect an actual grazing hit. */
    for (int sign = -1; sign <= 1; sign += 2) {
        for (int32_t speed = 2 * POOL_ONE; speed <= 10 * POOL_ONE; speed += 2 * POOL_ONE) {
            for (int32_t offset = 0; offset <= 14 * POOL_ONE + 256; offset += 128) {
                for (int32_t phase = 0; phase < 5 * POOL_ONE; phase += 1024) {
                    pool_ball_t pair[] = {
                        {        {230 * POOL_ONE - phase, 132 * POOL_ONE},  {speed, 0}},
                        {{250 * POOL_ONE, 132 * POOL_ONE + sign * offset}, {-speed, 0}}
                    };
                    bool deflected = false;
                    for (unsigned int tick = 0U; tick < 20U; ++tick) {
                        const uint64_t before = energy(pair, 2U);
                        (void) pool_physics_world_step(pair, 2U);
                        assert(energy(pair, 2U) <= before);
                        legal(pair, 2U);
                        deflected = pair[1].velocity.y != 0 || pair[1].velocity.x >= 0 || deflected;
                    }
                    if (offset < 14 * POOL_ONE - 128) {
                        assert(deflected);
                    } else if (offset > 14 * POOL_ONE) {
                        assert(!deflected);
                    }
                    assert(overlap(pair, 2U) <= 16);
                    ++tested;
                }
            }
        }
    }
    printf("Grazing/phase/speed sweeps: %u pairs\n", tested);
}

static void chains_and_walls(void)
{
    pool_ball_t balls[POOL_BALL_COUNT] = {0};
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        balls[i].position = (pool_vec_t) {(100 + (int32_t) i * 14) * POOL_ONE, 132 * POOL_ONE};
    }
    balls[0].velocity.x = 10 * POOL_ONE;
    for (unsigned int tick = 0U; tick < 12U; ++tick) {
        const uint64_t before = energy(balls, POOL_BALL_COUNT);
        (void) pool_physics_world_step(balls, POOL_BALL_COUNT);
        assert(energy(balls, POOL_BALL_COUNT) <= before);
    }
    assert(balls[15].position.x > 310 * POOL_ONE);
    assert(overlap(balls, POOL_BALL_COUNT) < POOL_ONE / 8);
    for (unsigned int corner = 0U; corner < 4U; ++corner) {
        const int sx       = (corner & 1U) == 0U ? 1 : -1;
        const int sy       = (corner & 2U) == 0U ? 1 : -1;
        pool_ball_t pair[] = {
            {  {(sx > 0 ? 7 : 521) * POOL_ONE, (sy > 0 ? 7 : 257) * POOL_ONE},                     {0, 0}},
            {{(sx > 0 ? 17 : 511) * POOL_ONE, (sy > 0 ? 17 : 247) * POOL_ONE}, {-sx * 28000, -sy * 28000}}
        };
        for (unsigned int tick = 0U; tick < 400U; ++tick) {
            const uint64_t before = energy(pair, 2U);
            (void) pool_physics_world_step(pair, 2U);
            assert(energy(pair, 2U) <= before);
            legal(pair, 2U);
        }
        assert(energy(pair, 2U) == 0U && overlap(pair, 2U) <= 16);
    }
}

static void breaks(void)
{
    unsigned int longest = 0U;
    int32_t worst        = 0;
    uint64_t hash        = UINT64_C(14695981039346656037);
    for (unsigned int run = 0U; run < 64U; ++run) {
        pool_game_t game;
        pool_game_reset(&game);
        game.angle = (POOL_ANGLE_COUNT + run % 16U * 8U - 64U) % POOL_ANGLE_COUNT;
        game.power = 40U + run / 16U * 20U;
        assert(pool_game_shoot(&game));
        unsigned int ticks = 0U;
        while (game.shot_set && ticks < 1000U) {
            const uint64_t before = energy(game.balls, POOL_BALL_COUNT);
            assert(closed_step(&game));
            assert(energy(game.balls, POOL_BALL_COUNT) <= before);
            legal(game.balls, POOL_BALL_COUNT);
            const int32_t penetration = overlap(game.balls, POOL_BALL_COUNT);
            if (penetration > worst) {
                worst = penetration;
            }
            for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
                const int32_t values[] = {game.balls[i].position.x, game.balls[i].position.y, game.balls[i].velocity.x,
                                          game.balls[i].velocity.y};
                for (unsigned int j = 0U; j < 4U; ++j) {
                    hash ^= (uint32_t) values[j];
                    hash *= UINT64_C(1099511628211);
                }
            }
            ++ticks;
        }
        assert(ticks < 1000U && pool_game_can_aim(&game));
        assert(overlap(game.balls, POOL_BALL_COUNT) <= 16);
        if (ticks > longest) {
            longest = ticks;
        }
        if (game.power >= 60U) {
            unsigned int moved = 0U;
            for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
                if (game.balls[i].position.x != pool_initial_balls[i].x * POOL_ONE ||
                    game.balls[i].position.y != pool_initial_balls[i].y * POOL_ONE) {
                    ++moved;
                }
            }
            assert(moved >= 1U);
            if (game.power == 100U) {
                assert(moved >= 10U);
            }
        }
        const pool_game_t rest = game;
        assert(!closed_step(&game) && memcmp(&rest, &game, sizeof(game)) == 0);
        assert(pool_game_shoot(&game));
    }
    assert(longest == 252U && worst == 34);
    assert(hash == UINT64_C(12261552135598244603));
    printf("64 breaks: longest %u ticks, worst penetration %d Q12, hash %llu\n", longest, worst,
           (unsigned long long) hash);
}

static void order_and_repeated_shots(void)
{
    /* Swapping IDs must not change an isolated pair's physical result. */
    for (unsigned int angle = 0U; angle < POOL_ANGLE_COUNT; angle += 17U) {
        const pool_vec_t n = pool_direction(angle);
        pool_ball_t pair[] = {
            {                      {264 * POOL_ONE, 132 * POOL_ONE}, {n.x * 10, n.y * 10}},
            {{264 * POOL_ONE + n.x * 14, 132 * POOL_ONE + n.y * 14},               {0, 0}}
        };
        pool_ball_t reversed[] = {pair[1], pair[0]};
        (void) pool_physics_world_step(pair, 2U);
        (void) pool_physics_world_step(reversed, 2U);
        assert(memcmp(&pair[0], &reversed[1], sizeof(pool_ball_t)) == 0);
        assert(memcmp(&pair[1], &reversed[0], sizeof(pool_ball_t)) == 0);
        const int64_t forward = (int64_t) pair[1].velocity.x * n.x + (int64_t) pair[1].velocity.y * n.y;
        assert(forward > (int64_t) 9 * POOL_ONE * POOL_ONE);
    }
    pool_game_t game;
    pool_game_reset(&game);
    game.shot_set             = true;
    game.balls[15].velocity.x = POOL_ONE;
    assert(closed_step(&game) && game.shot_set && !pool_game_can_aim(&game));
    game.paused              = true;
    const pool_game_t paused = game;
    assert(!closed_step(&game) && memcmp(&paused, &game, sizeof(game)) == 0);
    pool_game_reset(&game);
    uint32_t random = 7U;
    for (unsigned int shot = 0U; shot < 128U; ++shot) {
        random     = random * 1664525U + 1013904223U;
        game.angle = shot == 0U ? 0U : (random >> 16U) % POOL_ANGLE_COUNT;
        game.power = 100U;
        assert(pool_game_shoot(&game));
        unsigned int ticks = 0U;
        while (game.shot_set && ticks++ < 1000U) {
            const uint64_t before = energy(game.balls, POOL_BALL_COUNT);
            assert(closed_step(&game));
            assert(energy(game.balls, POOL_BALL_COUNT) <= before);
            legal(game.balls, POOL_BALL_COUNT);
        }
        assert(ticks < 1000U && pool_game_can_aim(&game));
        assert(overlap(game.balls, POOL_BALL_COUNT) <= 16);
    }
    puts("241 pair-order rotations and 128 consecutive full-power shots passed");
}

int main(void)
{
    pairs();
    grazing();
    chains_and_walls();
    breaks();
    order_and_repeated_shots();
    puts("Pool pair, grazing, chain, corner and break tests passed");
    return 0;
}
