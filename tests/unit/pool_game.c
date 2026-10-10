#include <pool/game.h>
#include <pool/input.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool event(pool_input_t* input, pool_game_t* game, tabos_key_t key, bool down)
{
    const tabos_input_event_t value = {.type = down ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP, .key = key};
    return pool_input_event(input, game, &value);
}

static void tap(pool_input_t* input, pool_game_t* game, tabos_key_t key)
{
    (void) event(input, game, key, true);
    (void) event(input, game, key, false);
    if (key == TABOS_KEY_R) {
        (void) event(input, game, TABOS_KEY_ENTER, true);
        (void) event(input, game, TABOS_KEY_ENTER, false);
    }
}

static void directions_and_shots(void)
{
    const pool_vec_t cardinal[] = {
        { POOL_ONE,         0},
        {        0,  POOL_ONE},
        {-POOL_ONE,         0},
        {        0, -POOL_ONE}
    };
    for (unsigned int a = 0U; a < POOL_ANGLE_COUNT; ++a) {
        const pool_vec_t direction = pool_direction(a);
        const int64_t norm         = (int64_t) direction.x * direction.x + (int64_t) direction.y * direction.y;
        assert(llabs(norm - (int64_t) POOL_ONE * POOL_ONE) < 6000);
        if (a % 1024U == 0U) {
            assert(direction.x == cardinal[a / 1024U].x && direction.y == cardinal[a / 1024U].y);
        }
        const pool_vec_t opposite = pool_direction(a + 2048U);
        assert(direction.x == -opposite.x && direction.y == -opposite.y);
        pool_game_t game;
        pool_game_reset(&game);
        const pool_ball_t before[POOL_BALL_COUNT] = {
            [0] = game.balls[0],
        };
        game.angle = a;
        game.power = 100U;
        assert(pool_game_shoot(&game));
        assert(game.shot_set && !pool_game_can_aim(&game));
        assert(game.balls[0].velocity.x == direction.x * 10);
        assert(game.balls[0].velocity.y == direction.y * 10);
        assert(game.balls[0].position.x == before[0].position.x && game.balls[0].position.y == before[0].position.y);
        const pool_vec_t velocity = game.balls[0].velocity;
        assert(!pool_game_shoot(&game) && !pool_game_adjust(&game, 1, 1, false));
        assert(game.balls[0].velocity.x == velocity.x && game.balls[0].velocity.y == velocity.y);
        for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
            assert(game.balls[i].velocity.x == 0 && game.balls[i].velocity.y == 0);
            assert(game.balls[i].position.x == pool_initial_balls[i].x * POOL_ONE);
            assert(game.balls[i].position.y == pool_initial_balls[i].y * POOL_ONE);
        }
    }
    assert(pool_direction(UINT_MAX).x == pool_direction(UINT_MAX % POOL_ANGLE_COUNT).x);
    int32_t previous_speed = 0;
    for (unsigned int power = 1U; power <= 100U; ++power) {
        pool_game_t game;
        pool_game_reset(&game);
        game.power = power;
        assert(pool_game_shoot(&game));
        assert(game.balls[0].velocity.y == 0 && game.balls[0].velocity.x > previous_speed);
        previous_speed = game.balls[0].velocity.x;
        if (power == 1U) {
            assert(previous_speed == POOL_ONE / 2);
        }
    }
}

static void gates_and_guide(void)
{
    pool_game_t game;
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        pool_game_reset(&game);
        game.balls[i].velocity.x = 1;
        assert(!pool_game_can_aim(&game) && !pool_game_shoot(&game));
        assert(!pool_game_adjust(&game, 1, 1, false));
        game.balls[i].velocity = (pool_vec_t) {0, -1};
        assert(!pool_game_can_aim(&game));
    }
    pool_game_reset(&game);
    game.paused = true;
    assert(!pool_game_shoot(&game) && !pool_game_adjust(&game, 1, 1, false));
    pool_game_reset(&game);
    assert(pool_game_can_aim(&game) && game.power == 50U && game.angle == 0U);
    for (unsigned int corner = 0U; corner < 4U; ++corner) {
        game.balls[0].position.x =
            ((corner & 1U) != 0U ? POOL_TABLE_WIDTH - POOL_BALL_RADIUS : POOL_BALL_RADIUS) * POOL_ONE;
        game.balls[0].position.y =
            ((corner & 2U) != 0U ? POOL_TABLE_HEIGHT - POOL_BALL_RADIUS : POOL_BALL_RADIUS) * POOL_ONE;
        for (unsigned int a = 0U; a < POOL_ANGLE_COUNT; ++a) {
            game.angle           = a;
            const pool_vec_t end = pool_game_aim_end(&game);
            assert(end.x >= POOL_BALL_RADIUS * POOL_ONE && end.x <= (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE);
            assert(end.y >= POOL_BALL_RADIUS * POOL_ONE && end.y <= (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE);
        }
    }
}

static void controls(void)
{
    pool_game_t game;
    pool_game_reset(&game);
    pool_input_t input = {0};
    tap(&input, &game, TABOS_KEY_LEFT);
    assert(game.angle == POOL_ANGLE_COUNT - POOL_AIM_STEP);
    tap(&input, &game, TABOS_KEY_D);
    assert(game.angle == 0U && !pool_input_active(&input, &game));
    assert(event(&input, &game, TABOS_KEY_RIGHT, true));
    assert(!event(&input, &game, TABOS_KEY_D, true));
    assert(!event(&input, &game, TABOS_KEY_RIGHT, true));
    const tabos_input_event_t repeat = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_RIGHT, .repeat = true};
    assert(!pool_input_event(&input, &game, &repeat));
    (void) event(&input, &game, TABOS_KEY_RIGHT, false);
    assert(pool_input_active(&input, &game));
    for (unsigned int i = 0U; i < 60U; ++i) {
        assert(pool_input_step(&input, &game));
    }
    assert(game.angle == 61U * POOL_AIM_STEP);
    (void) event(&input, &game, TABOS_KEY_LEFT, true);
    assert(!pool_input_active(&input, &game) && !pool_input_step(&input, &game));
    tap(&input, &game, TABOS_KEY_R);
    assert(!pool_input_active(&input, &game));
    (void) event(&input, &game, TABOS_KEY_D, false);
    (void) event(&input, &game, TABOS_KEY_LEFT, false);
    (void) event(&input, &game, TABOS_KEY_SHIFT, true);
    tap(&input, &game, TABOS_KEY_D);
    assert(game.angle == POOL_FINE_STEP);
    (void) event(&input, &game, TABOS_KEY_SHIFT, false);
    tap(&input, &game, TABOS_KEY_D);
    assert(game.angle == POOL_FINE_STEP + POOL_AIM_STEP);
    tap(&input, &game, TABOS_KEY_W);
    assert(game.power == 51U);
    tap(&input, &game, TABOS_KEY_DOWN);
    assert(game.power == 50U);
    (void) event(&input, &game, TABOS_KEY_UP, true);
    (void) event(&input, &game, TABOS_KEY_W, true);
    (void) event(&input, &game, TABOS_KEY_UP, false);
    for (unsigned int i = 0U; i < 150U; ++i) {
        (void) pool_input_step(&input, &game);
    }
    assert(game.power == 100U);
    (void) event(&input, &game, TABOS_KEY_W, false);
    (void) event(&input, &game, TABOS_KEY_S, true);
    for (unsigned int i = 0U; i < 150U; ++i) {
        (void) pool_input_step(&input, &game);
    }
    assert(game.power == 1U);
    (void) event(&input, &game, TABOS_KEY_S, false);
    (void) event(&input, &game, TABOS_KEY_A, true);
    const unsigned int angle = game.angle;
    tap(&input, &game, TABOS_KEY_P);
    assert(game.paused && !pool_input_step(&input, &game));
    tap(&input, &game, TABOS_KEY_SPACE);
    assert(!game.shot_set);
    tap(&input, &game, TABOS_KEY_P);
    assert(!game.paused && !pool_input_active(&input, &game));
    assert(!pool_input_step(&input, &game) && game.angle == angle);
    (void) event(&input, &game, TABOS_KEY_A, false);
    (void) event(&input, &game, TABOS_KEY_SPACE, true);
    assert(game.shot_set);
    tap(&input, &game, TABOS_KEY_R);
    assert(!game.shot_set);
    (void) event(&input, &game, TABOS_KEY_K, true);
    assert(!game.shot_set); /* Space still held, K cannot rearm it. */
    (void) event(&input, &game, TABOS_KEY_SPACE, false);
    (void) event(&input, &game, TABOS_KEY_K, false);
    tap(&input, &game, TABOS_KEY_K);
    assert(game.shot_set);
    tap(&input, &game, TABOS_KEY_R);
    game.balls[12].velocity.y = 1;
    (void) event(&input, &game, TABOS_KEY_RIGHT, true);
    tap(&input, &game, TABOS_KEY_SPACE);
    game.balls[12].velocity.y = 0;
    assert(!pool_input_active(&input, &game) && !game.shot_set);
    (void) event(&input, &game, TABOS_KEY_RIGHT, false);
    tap(&input, &game, TABOS_KEY_RIGHT);
    assert(game.angle == POOL_AIM_STEP);
    tap(&input, &game, TABOS_KEY_ESCAPE);
    assert(input.quit);
}

static void presentation(void)
{
    pool_game_t game;
    pool_game_reset(&game);
    pool_input_t input = {0};
    game.title         = true;
    game.computer      = true;
    assert(!pool_game_can_aim(&game));
    tap(&input, &game, TABOS_KEY_LEFT);
    assert(!game.computer && game.title);
    tap(&input, &game, TABOS_KEY_H);
    assert(game.help && game.title);
    tap(&input, &game, TABOS_KEY_SPACE);
    assert(!game.shot_set && game.help);
    tap(&input, &game, TABOS_KEY_ESCAPE);
    assert(!game.help && game.title && !input.quit);
    tap(&input, &game, TABOS_KEY_M);
    assert(game.muted);
    (void) event(&input, &game, TABOS_KEY_ENTER, true);
    assert(!game.title && game.muted && !game.computer && !game.shot_set);
    assert(!event(&input, &game, TABOS_KEY_ENTER, true));
    (void) event(&input, &game, TABOS_KEY_ENTER, false);
    assert(pool_game_shoot(&game));
    assert(game.cue_flash == 8U && pool_game_animating(&game));
    game.pot_flash[2]     = 18U;
    const pool_ball_t cue = game.balls[0];
    tap(&input, &game, TABOS_KEY_H);
    assert(!pool_game_step(&game) && !pool_game_effects_step(&game));
    assert(game.cue_flash == 8U && game.pot_flash[2] == 18U);
    assert(game.balls[0].position.x == cue.position.x);
    tap(&input, &game, TABOS_KEY_H);
    for (unsigned int i = 0U; i < 18U; ++i) {
        assert(pool_game_effects_step(&game));
    }
    assert(!pool_game_animating(&game) && !pool_game_effects_step(&game));
    assert(memcmp(&cue, &game.balls[0], sizeof(cue)) == 0);
    tap(&input, &game, TABOS_KEY_P);
    tap(&input, &game, TABOS_KEY_H);
    tap(&input, &game, TABOS_KEY_ENTER);
    assert(!game.help && game.paused);
    tap(&input, &game, TABOS_KEY_R);
    assert(!game.paused && game.muted && !game.shot_set);
}

int main(void)
{
    directions_and_shots();
    gates_and_guide();
    controls();
    presentation();
    puts("Pool aim, power, velocity assignment, guide and input gates passed");
    return 0;
}
