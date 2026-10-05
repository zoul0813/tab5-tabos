#include <soccer/camera.h>
#include <stdlib.h>
static int32_t clamp(int32_t value, int32_t maximum)
{
    if (value < 0) {
        return 0;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}
static soccer_vec_t target(const soccer_game_t* game)
{
    /* Ball tracking is independent of player selection and facing changes. */
    soccer_vec_t focus  = game->ball;
    focus.x            += clamp(game->velocity.x * 4 + 64 * SOCCER_ONE, 128 * SOCCER_ONE) - 64 * SOCCER_ONE;
    focus.y            += clamp(game->velocity.y * 4 + 40 * SOCCER_ONE, 80 * SOCCER_ONE) - 40 * SOCCER_ONE;
    return (soccer_vec_t) {
        clamp(focus.x - SOCCER_VIEW_WIDTH * SOCCER_ONE / 2, (SOCCER_RIGHT + 64 - SOCCER_VIEW_WIDTH) * SOCCER_ONE),
        clamp(focus.y - SOCCER_VIEW_HEIGHT * SOCCER_ONE / 2, (SOCCER_BOTTOM + 64 - SOCCER_VIEW_HEIGHT) * SOCCER_ONE)};
}
void soccer_camera_reset(soccer_camera_t* camera, const soccer_game_t* game)
{
    camera->position = target(game);
    camera->velocity = (soccer_vec_t) {0, 0};
}
/* Ease the scrolling rate as well as the target position. The maximum
   acceleration is one world unit per tick; explicit resets remain immediate. */
static void move_axis(int32_t* position, int32_t* velocity, int32_t desired, int32_t maximum)
{
    const int32_t wanted  = clamp((desired - *position) / 12 + 8 * SOCCER_ONE, 16 * SOCCER_ONE) - 8 * SOCCER_ONE;
    *velocity            += clamp(wanted - *velocity + SOCCER_ONE, 2 * SOCCER_ONE) - SOCCER_ONE;
    const int32_t next    = *position + *velocity;
    *position             = clamp(next, maximum);
    if (*position != next) {
        *velocity = 0;
    }
    if (abs(desired - *position) < SOCCER_ONE / 8 && abs(*velocity) < SOCCER_ONE / 8) {
        *position = desired;
        *velocity = 0;
    }
}

void soccer_camera_step(soccer_camera_t* camera, const soccer_game_t* game)
{
    if (game->paused || game->mode == SOCCER_TITLE || game->mode == SOCCER_FULL_TIME) {
        return;
    }
    const soccer_vec_t desired = target(game);
    move_axis(&camera->position.x, &camera->velocity.x, desired.x,
              (SOCCER_RIGHT + 64 - SOCCER_VIEW_WIDTH) * SOCCER_ONE);
    move_axis(&camera->position.y, &camera->velocity.y, desired.y,
              (SOCCER_BOTTOM + 64 - SOCCER_VIEW_HEIGHT) * SOCCER_ONE);
}
