#include <pool/game.h>
#include <pool/input.h>
#include <pool/clock.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int64_t energy(pool_vec_t v)
{
    return (int64_t) v.x * v.x + (int64_t) v.y * v.y;
}

static void bounds(pool_ball_t ball)
{
    assert(ball.position.x >= POOL_BALL_RADIUS * POOL_ONE);
    assert(ball.position.x <= (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE);
    assert(ball.position.y >= POOL_BALL_RADIUS * POOL_ONE);
    assert(ball.position.y <= (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE);
}

/* Retain Stage 3's exact single-ball replay independently of the new rack. */
static bool single_step(pool_game_t* game)
{
    if (game->paused || !game->shot_set) {
        return false;
    }
    game->shot_set = pool_physics_world_step(game->balls, 1U);
    return true;
}

static void trajectories(void)
{
    unsigned int longest        = 0U;
    uint64_t hash               = UINT64_C(14695981039346656037);
    const unsigned int powers[] = {1U, 50U, 100U};
    for (unsigned int a = 0U; a < POOL_ANGLE_COUNT; ++a) {
        for (unsigned int p = 0U; p < 3U; ++p) {
            pool_game_t game;
            pool_game_reset(&game);
            game.angle = a;
            game.power = powers[p];
            assert(pool_game_shoot(&game));
            unsigned int ticks = 0U;
            while (game.shot_set && ticks < 400U) {
                const int64_t before = energy(game.balls[0].velocity);
                assert(single_step(&game));
                bounds(game.balls[0]);
                assert(energy(game.balls[0].velocity) < before);
                const int32_t values[] = {game.balls[0].position.x, game.balls[0].position.y, game.balls[0].velocity.x,
                                          game.balls[0].velocity.y};
                for (unsigned int i = 0U; i < 4U; ++i) {
                    hash ^= (uint32_t) values[i];
                    hash *= UINT64_C(1099511628211);
                }
                ++ticks;
            }
            assert(pool_game_can_aim(&game) && ticks < 400U);
            if (ticks > longest) {
                longest = ticks;
            }
            const pool_game_t stopped = game;
            for (unsigned int i = 0U; i < 120U; ++i) {
                assert(!single_step(&game));
            }
            assert(memcmp(&game, &stopped, sizeof(game)) == 0);
            for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
                assert(game.balls[i].position.x == pool_initial_balls[i].x * POOL_ONE);
                assert(game.balls[i].position.y == pool_initial_balls[i].y * POOL_ONE);
                assert(energy(game.balls[i].velocity) == 0);
            }
            assert(pool_game_shoot(&game));
        }
    }
    assert(longest == 285U);
    assert(hash == UINT64_C(10097316036875414284));
    printf("12288 trajectories: longest %u ticks; replay hash %llu\n", longest, (unsigned long long) hash);
}

static void friction_and_remainders(void)
{
    const unsigned int powers[]   = {1U, 50U, 100U};
    const unsigned int expected[] = {17U, 186U, 359U};
    for (unsigned int i = 0U; i < 3U; ++i) {
        pool_game_t game;
        pool_game_reset(&game);
        game.power = powers[i];
        assert(pool_game_shoot(&game));
        unsigned int ticks = 0U;
        while (game.shot_set && ticks < 400U) {
            /* Recenter to measure rolling loss without cushion losses. */
            game.balls[0].position = (pool_vec_t) {132 * POOL_ONE, 132 * POOL_ONE};
            assert(single_step(&game));
            ++ticks;
        }
        assert(ticks == expected[i]);
    }
    for (int sign = -1; sign <= 1; sign += 2) {
        pool_ball_t ball = {
            {132 * POOL_ONE, 132 * POOL_ONE},
            {   sign * 1031,     sign * 711}
        };
        pool_physics_step(&ball);
        assert(ball.position.x == 132 * POOL_ONE + sign * 1031);
        assert(ball.position.y == 132 * POOL_ONE + sign * 711);
        ball.velocity           = (pool_vec_t) {POOL_STOP_SPEED, 0};
        const pool_vec_t before = ball.position;
        pool_physics_step(&ball);
        assert(energy(ball.velocity) == 0);
        assert(ball.position.x == before.x && ball.position.y == before.y);
    }
    pool_ball_t horizontal = {
        {132 * POOL_ONE, 132 * POOL_ONE},
        {         20000,              0}
    };
    pool_ball_t diagonal = {
        {132 * POOL_ONE, 132 * POOL_ONE},
        {         14142,          14142}
    };
    pool_physics_step(&horizontal);
    pool_physics_step(&diagonal);
    /* Comparable magnitudes after friction, not equal per-axis losses. */
    const int64_t difference = energy(horizontal.velocity) - energy(diagonal.velocity);
    assert(difference > -60000 && difference < 60000);
    assert(diagonal.velocity.x == diagonal.velocity.y);
}

static void rails_and_corners(void)
{
    const int32_t low    = POOL_BALL_RADIUS * POOL_ONE;
    const int32_t high_x = (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE;
    const int32_t high_y = (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE;
    for (unsigned int side = 0U; side < 4U; ++side) {
        pool_ball_t ball = {
            {132 * POOL_ONE, 132 * POOL_ONE},
            {             0,              0}
        };
        int32_t* position  = side < 2U ? &ball.position.x : &ball.position.y;
        int32_t* velocity  = side < 2U ? &ball.velocity.x : &ball.velocity.y;
        const int32_t high = side < 2U ? high_x : high_y;
        const int32_t sign = (side & 1U) == 0U ? -1 : 1;
        *position          = sign < 0 ? low : high;
        *velocity          = sign * 10 * POOL_ONE;
        pool_physics_step(&ball);
        assert(*velocity == -sign * (10 * POOL_ONE * 85 / 100 - POOL_FRICTION));
        bounds(ball);
        /* Already departing a rail must not receive another impulse. */
        *position               = sign < 0 ? low : high;
        const int32_t departing = *velocity;
        pool_physics_step(&ball);
        assert(*velocity == departing + sign * POOL_FRICTION);
    }
    for (unsigned int corner = 0U; corner < 4U; ++corner) {
        const int32_t sx = (corner & 1U) == 0U ? -1 : 1;
        const int32_t sy = (corner & 2U) == 0U ? -1 : 1;
        pool_ball_t ball = {
            {sx < 0 ? low : high_x, sy < 0 ? low : high_y},
            {           sx * 28000,            sy * 28000}
        };
        pool_physics_step(&ball);
        bounds(ball);
        assert(ball.velocity.x * sx < 0 && ball.velocity.y * sy < 0);
        assert(ball.velocity.x * sx == ball.velocity.y * sy);
    }
    /* Closed side-pocket mouth; a shallow approach keeps its tangent apart
     * from the common rolling loss. Mirror positions/velocities exactly. */
    pool_ball_t left = {
        {264 * POOL_ONE,    low},
        {         10000, -20000}
    };
    pool_ball_t right = {
        {264 * POOL_ONE, high_y},
        {         10000,  20000}
    };
    pool_physics_step(&left);
    pool_physics_step(&right);
    assert(left.velocity.x == right.velocity.x && left.velocity.y == -right.velocity.y);
    assert(left.position.y + right.position.y == POOL_TABLE_HEIGHT * POOL_ONE);
    assert(left.velocity.y > 0 && left.velocity.x > 9900);
}

static pool_game_t replay(unsigned int cadence_ms)
{
    pool_game_t game;
    pool_game_reset(&game);
    game.angle = 517U;
    game.power = 100U;
    assert(pool_game_shoot(&game));
    pool_clock_t clock = {0};
    for (unsigned int time = 0U; time < 2000U;) {
        const unsigned int elapsed = 2000U - time < cadence_ms ? 2000U - time : cadence_ms;
        const unsigned int ticks   = pool_clock_advance(&clock, elapsed);
        for (unsigned int i = 0U; i < ticks; ++i) {
            game.shot_set = pool_physics_world_step(game.balls, POOL_BALL_COUNT);
        }
        time += elapsed;
    }
    assert(clock.accumulator == 0U && clock.discarded_ms == 0U);
    return game;
}

static void clocks_and_pause(void)
{
    const pool_game_t baseline    = replay(1U);
    const unsigned int cadences[] = {7U, 16U, 17U, 33U, 50U, 100U};
    for (unsigned int i = 0U; i < sizeof(cadences) / sizeof(cadences[0]); ++i) {
        const pool_game_t other = replay(cadences[i]);
        assert(memcmp(baseline.balls, other.balls, sizeof(baseline.balls)) == 0);
        assert(baseline.shot_set == other.shot_set && baseline.angle == other.angle && baseline.power == other.power);
    }
    pool_clock_t clock = {0};
    assert(pool_clock_advance(&clock, 5000U) == 6U);
    assert(clock.discarded_ms == 4900U && clock.accumulator == 0U);
    assert(pool_clock_wait(&clock, 0U) == 17U);
    assert(pool_clock_wait(&clock, 8U) == 9U);
    assert(pool_clock_wait(&clock, 100U) == 0U);
    (void) pool_clock_advance(&clock, UINT64_MAX);
    assert(clock.discarded_ms == UINT64_MAX);
    pool_game_t game         = baseline;
    game.paused              = true;
    const pool_game_t paused = game;
    for (unsigned int i = 0U; i < 600U; ++i) {
        assert(!pool_game_step(&game));
    }
    assert(memcmp(&game, &paused, sizeof(game)) == 0);
    game.paused = false;
    assert(pool_game_step(&game));
    pool_game_reset(&game);
    pool_input_t input             = {0};
    const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SPACE};
    assert(pool_input_event(&input, &game, &down));
    while (game.shot_set) {
        (void) pool_game_step(&game);
    }
    if (game.placement) {
        assert(pool_game_confirm_placement(&game));
    }
    assert(!pool_input_event(&input, &game, &down));
    assert(pool_game_can_aim(&game));
    const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_SPACE};
    (void) pool_input_event(&input, &game, &up);
    assert(pool_input_event(&input, &game, &down));
}

int main(void)
{
    trajectories();
    friction_and_remainders();
    rails_and_corners();
    clocks_and_pause();
    puts("Pool motion, friction, closed rails, exact rest and clock tests passed");
    return 0;
}
