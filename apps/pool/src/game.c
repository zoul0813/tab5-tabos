#include <pool/game.h>
#include <string.h>

static const int16_t sine_quarter[1025] = {
#include "../assets/sine_q12.inc"
};

static int32_t sine(unsigned int angle)
{
    angle                     %= POOL_ANGLE_COUNT;
    const unsigned int offset  = angle % 1024U;
    switch (angle / 1024U) {
        case 0U: return sine_quarter[offset];
        case 1U: return sine_quarter[1024U - offset];
        case 2U: return -sine_quarter[offset];
        default: return -sine_quarter[1024U - offset];
    }
}

pool_vec_t pool_direction(unsigned int angle)
{
    angle %= POOL_ANGLE_COUNT;
    return (pool_vec_t) {sine(angle + 1024U), sine(angle)};
}

void pool_game_reset(pool_game_t* game)
{
    memset(game, 0, sizeof(*game));
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        game->balls[i].position = (pool_vec_t) {pool_initial_balls[i].x * POOL_ONE, pool_initial_balls[i].y * POOL_ONE};
    }
    game->power = POOL_DEFAULT_POWER;
    pool_rules_reset(&game->rules);
}

void pool_game_new_rack(pool_game_t* game)
{
    const bool computer = game->computer;
    const bool muted    = game->muted;
    pool_game_reset(game);
    game->computer = computer;
    game->muted    = muted;
}

bool pool_game_computer_turn(const pool_game_t* game)
{
    return game->computer && game->rules.player == 1U && !game->rules.complete;
}

bool pool_game_can_aim(const pool_game_t* game)
{
    if (game->title || game->help || game->restart_pending || game->rules.complete || game->paused || game->shot_set ||
        game->placement || (game->potted & 1U) != 0U) {
        return false;
    }
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        if ((game->potted & (1U << i)) == 0U && (game->balls[i].velocity.x != 0 || game->balls[i].velocity.y != 0)) {
            return false;
        }
    }
    return true;
}

bool pool_game_can_place(const pool_game_t* game)
{
    return !game->title && !game->help && game->placement && !game->paused && !game->shot_set &&
           !game->restart_pending && !game->rules.complete;
}

bool pool_game_placement_valid(const pool_game_t* game)
{
    return pool_game_can_place(game) &&
           pool_physics_placement_valid(game->balls, game->potted, game->placement_position);
}

bool pool_game_confirm_placement(pool_game_t* game)
{
    if (!pool_game_placement_valid(game)) {
        return false;
    }
    game->balls[0].position  = game->placement_position;
    game->balls[0].velocity  = (pool_vec_t) {0, 0};
    game->potted            &= (uint16_t) ~1U;
    game->placement          = false;
    return true;
}

static int32_t placement_axis(int32_t value, int direction, int32_t step, int32_t high)
{
    value += ((direction > 0) - (direction < 0)) * step;
    if (value < POOL_BALL_RADIUS * POOL_ONE) {
        value = POOL_BALL_RADIUS * POOL_ONE;
    } else if (value > high) {
        value = high;
    }
    return value;
}

bool pool_game_adjust(pool_game_t* game, int aim, int power, bool fine)
{
    if (pool_game_can_place(game)) {
        const pool_vec_t before = game->placement_position;
        const int32_t step      = fine ? POOL_ONE / 4 : 2 * POOL_ONE;
        game->placement_position.x =
            placement_axis(before.x, aim, step, (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE);
        game->placement_position.y =
            placement_axis(before.y, -power, step, (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE);
        return before.x != game->placement_position.x || before.y != game->placement_position.y;
    }
    if (!pool_game_can_aim(game)) {
        return false;
    }
    const unsigned int previous_angle = game->angle;
    const unsigned int previous_power = game->power;
    const int direction               = (aim > 0) - (aim < 0);
    const int step                    = fine ? POOL_FINE_STEP : POOL_AIM_STEP;
    game->angle = (unsigned int) ((int) game->angle + POOL_ANGLE_COUNT + direction * step) % POOL_ANGLE_COUNT;
    if (power > 0 && game->power < 100U) {
        ++game->power;
    } else if (power < 0 && game->power > 1U) {
        --game->power;
    }
    return previous_angle != game->angle || previous_power != game->power;
}

bool pool_game_shoot(pool_game_t* game)
{
    if (!pool_game_can_aim(game)) {
        return false;
    }
    const pool_vec_t direction = pool_direction(game->angle);
    /* 1% = 30 px/s, 100% = 600 px/s. Products use 64 bits; the maximum
     * resulting component is 10 * ONE. Integration is a separate fixed tick. */
    const int32_t speed       = POOL_ONE / 2 + (int32_t) (game->power - 1U) * (19 * POOL_ONE / 2) / 99;
    game->balls[0].velocity.x = (int32_t) ((int64_t) direction.x * speed / POOL_ONE);
    game->balls[0].velocity.y = (int32_t) ((int64_t) direction.y * speed / POOL_ONE);
    game->cue_flash           = 8U;
    game->cue_origin          = game->balls[0].position;
    game->cue_angle           = game->angle;
    game->shot_set            = true;
    game->shot_pots.count     = 0U;
    game->contacts            = (pool_contact_state_t) {0};
    pool_rules_begin(&game->rules, game->potted);
    return true;
}

bool pool_game_step(pool_game_t* game)
{
    if (game->title || game->help || game->restart_pending || game->rules.complete || game->paused || !game->shot_set) {
        return false;
    }
    pool_pot_events_t events;
    game->shot_set = pool_physics_match_step(game->balls, POOL_BALL_COUNT, &game->potted, &events, &game->contacts);
    for (unsigned int i = 0U; i < events.count; ++i) {
        const unsigned int index              = game->shot_pots.count++;
        game->shot_pots.ball[index]           = events.ball[i];
        game->shot_pots.pocket[index]         = events.pocket[i];
        game->pocket_for_ball[events.ball[i]] = events.pocket[i];
        game->pot_flash[events.pocket[i]]     = 18U;
    }
    pool_rule_result_t result = POOL_RULE_IDLE;
    if (!game->shot_set) {
        result = pool_rules_finish(&game->rules, game->potted, &game->shot_pots, &game->contacts);
    }
    if (result == POOL_RERACK) {
        const unsigned int breaker = game->rules.player;
        pool_game_new_rack(game);
        game->rules.player   = breaker;
        game->rules.reracked = true;
        return true;
    }
    if (result == POOL_BALL_IN_HAND) {
        game->potted            |= 1U;
        game->balls[0].velocity  = (pool_vec_t) {0, 0};
    }
    if (!game->shot_set && !game->rules.complete && (game->potted & 1U) != 0U) {
        game->placement = true;
        game->placement_position =
            (pool_vec_t) {pool_initial_balls[0].x * POOL_ONE, pool_initial_balls[0].y * POOL_ONE};
    }
    return true;
}

pool_vec_t pool_game_aim_end(const pool_game_t* game)
{
    const pool_vec_t start     = game->balls[0].position;
    const pool_vec_t direction = pool_direction(game->angle);
    int64_t distance           = 128 * POOL_ONE;
    const int32_t origin[]     = {start.x, start.y};
    const int32_t delta[]      = {direction.x, direction.y};
    const int32_t high[]       = {(POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE,
                                  (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE};
    for (unsigned int axis = 0U; axis < 2U; ++axis) {
        if (delta[axis] != 0) {
            const int32_t boundary  = delta[axis] > 0 ? high[axis] : POOL_BALL_RADIUS * POOL_ONE;
            const int64_t candidate = (int64_t) (boundary - origin[axis]) * POOL_ONE / delta[axis];
            if (candidate < distance) {
                distance = candidate;
            }
        }
    }
    if (distance < 0) {
        distance = 0;
    }
    return (pool_vec_t) {start.x + (int32_t) (direction.x * distance / POOL_ONE),
                         start.y + (int32_t) (direction.y * distance / POOL_ONE)};
}

bool pool_game_animating(const pool_game_t* game)
{
    if (game->title || game->help || game->paused || game->restart_pending || game->rules.complete) {
        return false;
    }
    if (game->cue_flash != 0U) {
        return true;
    }
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        if (game->pot_flash[i] != 0U) {
            return true;
        }
    }
    return false;
}

bool pool_game_effects_step(pool_game_t* game)
{
    if (!pool_game_animating(game)) {
        return false;
    }
    if (game->cue_flash != 0U) {
        --game->cue_flash;
    }
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        if (game->pot_flash[i] != 0U) {
            --game->pot_flash[i];
        }
    }
    return true;
}
