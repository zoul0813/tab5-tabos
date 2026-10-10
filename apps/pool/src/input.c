#include <pool/input.h>

enum {
    LEFT    = 3U,
    RIGHT   = 12U,
    UP      = 48U,
    DOWN    = 192U,
    FINE    = 256U,
    FIRE    = 1536U,
    PAUSE   = 2048U,
    RESET   = 4096U,
    CONFIRM = 8192U,
    HELP    = 16384U,
    MUTE    = 32768U
};

static unsigned int key_bit(tabos_key_t key)
{
    switch (key) {
        case TABOS_KEY_LEFT: return 1U;
        case TABOS_KEY_A: return 2U;
        case TABOS_KEY_RIGHT: return 4U;
        case TABOS_KEY_D: return 8U;
        case TABOS_KEY_UP: return 16U;
        case TABOS_KEY_W: return 32U;
        case TABOS_KEY_DOWN: return 64U;
        case TABOS_KEY_S: return 128U;
        case TABOS_KEY_SHIFT: return FINE;
        case TABOS_KEY_SPACE: return 512U;
        case TABOS_KEY_K: return 1024U;
        case TABOS_KEY_P: return PAUSE;
        case TABOS_KEY_R: return RESET;
        case TABOS_KEY_ENTER: return CONFIRM;
        case TABOS_KEY_H: return HELP;
        case TABOS_KEY_M: return MUTE;
        default: return 0U;
    }
}

static int axis(unsigned int keys, unsigned int positive, unsigned int negative)
{
    return ((keys & positive) != 0U) - ((keys & negative) != 0U);
}

bool pool_input_active(const pool_input_t* input, const pool_game_t* game)
{
    const unsigned int keys = input->held & ~input->blocked;
    return !pool_game_computer_turn(game) && (pool_game_can_aim(game) || pool_game_can_place(game)) &&
           (axis(keys, RIGHT, LEFT) != 0 || axis(keys, UP, DOWN) != 0);
}

bool pool_input_step(const pool_input_t* input, pool_game_t* game)
{
    if (pool_game_computer_turn(game)) {
        return false;
    }
    const unsigned int keys = input->held & ~input->blocked;
    return pool_game_adjust(game, axis(keys, RIGHT, LEFT), axis(keys, UP, DOWN), (keys & FINE) != 0U);
}

bool pool_input_event(pool_input_t* input, pool_game_t* game, const tabos_input_event_t* event)
{
    const unsigned int bit = key_bit(event->key);
    if (event->type == TABOS_INPUT_KEY_UP) {
        input->held    &= ~bit;
        input->blocked &= ~bit;
        return false;
    }
    if (event->type != TABOS_INPUT_KEY_DOWN || event->repeat) {
        return false;
    }
    if (event->key == TABOS_KEY_ESCAPE && game->help) {
        game->help     = false;
        input->blocked = input->held;
        return true;
    }
    if (event->key == TABOS_KEY_ESCAPE && game->restart_pending) {
        game->restart_pending = false;
        input->blocked        = input->held;
        return true;
    }
    if (event->key == TABOS_KEY_Q || event->key == TABOS_KEY_ESCAPE) {
        input->quit = true;
        return false;
    }
    if (bit == 0U || (input->held & bit) != 0U) {
        return false;
    }
    const unsigned int previous  = input->held & ~input->blocked;
    const unsigned int physical  = input->held;
    input->held                 |= bit;
    if (bit == MUTE) {
        game->muted    = !game->muted;
        input->blocked = input->held;
        return true;
    }
    if (bit == HELP && !game->restart_pending) {
        game->help     = !game->help;
        input->blocked = input->held;
        return true;
    }
    if (game->help) {
        if (bit == CONFIRM) {
            game->help = false;
        }
        input->blocked = input->held;
        return bit == CONFIRM;
    }
    if (game->title) {
        if ((bit & (LEFT | RIGHT)) != 0U) {
            game->computer = !game->computer;
        } else if (bit == CONFIRM) {
            pool_game_new_rack(game);
        }
        input->blocked = input->held;
        return bit == CONFIRM || (bit & (LEFT | RIGHT)) != 0U;
    }
    if (bit == RESET) {
        game->restart_pending = !game->restart_pending;
        input->blocked        = input->held;
        return true;
    }
    if (game->restart_pending || game->rules.complete) {
        if (bit == CONFIRM) {
            pool_game_new_rack(game);
            input->blocked = input->held;
            return true;
        }
        input->blocked |= bit;
        return false;
    }
    if (bit == PAUSE) {
        game->paused   = !game->paused;
        input->blocked = input->held;
        return true;
    }
    if (pool_game_computer_turn(game)) {
        input->blocked |= bit;
        return false;
    }
    if (bit == CONFIRM) {
        const bool confirmed = pool_game_confirm_placement(game);
        input->blocked       = input->held;
        return confirmed;
    }
    if (pool_game_can_place(game)) {
        if ((bit & FIRE) != 0U) {
            input->blocked |= bit;
            return false;
        }
    } else if (!pool_game_can_aim(game)) {
        input->blocked |= bit;
        return false;
    }
    if ((bit & FIRE) != 0U) {
        /* Either alias counts as one trigger. A held Space must not rearm K
         * across pause/reset, nor fire automatically when the table settles. */
        if ((physical & FIRE) == 0U && pool_game_shoot(game)) {
            input->blocked = input->held;
            return true;
        }
        return false;
    }
    const unsigned int keys = input->held & ~input->blocked;
    int aim                 = axis(keys, RIGHT, LEFT);
    int power               = axis(keys, UP, DOWN);
    if (aim == axis(previous, RIGHT, LEFT)) {
        aim = 0;
    }
    if (power == axis(previous, UP, DOWN)) {
        power = 0;
    }
    /* An initial press acts immediately, including down/up in one event drain.
     * Held movement thereafter is tick-driven, independent of OS repeats. */
    return pool_game_adjust(game, aim, power, (keys & FINE) != 0U);
}
