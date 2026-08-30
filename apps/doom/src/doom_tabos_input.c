#include "doom_tabos_input.h"

#include <string.h>

enum {
    DOOM_KEY_ESCAPE       = 27,
    DOOM_KEY_ENTER        = 13,
    DOOM_KEY_TAB          = 9,
    DOOM_KEY_BACKSPACE    = 0x7f,
    DOOM_KEY_F1           = 0xbb,
    DOOM_KEY_F2           = 0xbc,
    DOOM_KEY_F3           = 0xbd,
    DOOM_KEY_F4           = 0xbe,
    DOOM_KEY_F5           = 0xbf,
    DOOM_KEY_F6           = 0xc0,
    DOOM_KEY_F7           = 0xc1,
    DOOM_KEY_F8           = 0xc2,
    DOOM_KEY_F9           = 0xc3,
    DOOM_KEY_F10          = 0xc4,
    DOOM_KEY_F11          = 0xd7,
    DOOM_KEY_F12          = 0xd8,
    DOOM_KEY_STRAFE_LEFT  = 0xa0,
    DOOM_KEY_STRAFE_RIGHT = 0xa1,
    DOOM_KEY_USE          = 0xa2,
    DOOM_KEY_FIRE         = 0xa3,
    DOOM_KEY_LEFT         = 0xac,
    DOOM_KEY_UP           = 0xad,
    DOOM_KEY_RIGHT        = 0xae,
    DOOM_KEY_DOWN         = 0xaf,
    DOOM_KEY_SHIFT        = 0xb6,
};

static unsigned char mapped_key(tabos_key_t key)
{
    if (key >= TABOS_KEY_1 && key <= TABOS_KEY_9) {
        return (unsigned char) ('1' + (key - TABOS_KEY_1));
    }
    if (key == TABOS_KEY_0) {
        return '0';
    }
    if (key >= TABOS_KEY_F1 && key <= TABOS_KEY_F10) {
        return (unsigned char) (DOOM_KEY_F1 + (key - TABOS_KEY_F1));
    }

    switch (key) {
        case TABOS_KEY_W: return DOOM_KEY_UP;
        case TABOS_KEY_S: return DOOM_KEY_DOWN;
        case TABOS_KEY_A: return DOOM_KEY_STRAFE_LEFT;
        case TABOS_KEY_D: return DOOM_KEY_STRAFE_RIGHT;
        case TABOS_KEY_J: return DOOM_KEY_FIRE;
        case TABOS_KEY_E:
        case TABOS_KEY_SPACE: return DOOM_KEY_USE;
        case TABOS_KEY_Y: return 'y';
        case TABOS_KEY_N: return 'n';
        case TABOS_KEY_LEFT: return DOOM_KEY_LEFT;
        case TABOS_KEY_RIGHT: return DOOM_KEY_RIGHT;
        case TABOS_KEY_UP: return DOOM_KEY_UP;
        case TABOS_KEY_DOWN: return DOOM_KEY_DOWN;
        case TABOS_KEY_ESCAPE: return DOOM_KEY_ESCAPE;
        case TABOS_KEY_ENTER: return DOOM_KEY_ENTER;
        case TABOS_KEY_BACKSPACE: return DOOM_KEY_BACKSPACE;
        case TABOS_KEY_TAB: return DOOM_KEY_TAB;
        case TABOS_KEY_MINUS: return '-';
        case TABOS_KEY_EQUALS: return '=';
        case TABOS_KEY_F11: return DOOM_KEY_F11;
        case TABOS_KEY_F12: return DOOM_KEY_F12;
        default: return 0U;
    }
}

static void begin_recovery(doom_tabos_input_t* input)
{
    memset(input->queue, 0, sizeof(input->queue));
    memset(input->source_down, 0, sizeof(input->source_down));
    memset(input->logical_down, 0, sizeof(input->logical_down));
    input->queue_head   = 0U;
    input->queue_count  = 0U;
    input->recovery_key = 0U;
    input->control_down = false;
    input->shift_down   = false;
    input->always_run   = false;
    input->recovering   = true;
}

static bool enqueue(doom_tabos_input_t* input, bool pressed, unsigned char key)
{
    if (input->queue_count == DOOM_TABOS_INPUT_QUEUE_CAPACITY) {
        begin_recovery(input);
        return false;
    }

    const size_t tail          = (input->queue_head + input->queue_count) % DOOM_TABOS_INPUT_QUEUE_CAPACITY;
    input->queue[tail].pressed = pressed;
    input->queue[tail].key     = key;
    ++input->queue_count;
    input->logical_down[key] = pressed;
    return true;
}

static void set_desired(doom_tabos_input_t* input, const bool* desired, bool pressed, unsigned char skip_one,
                        unsigned char skip_two)
{
    for (size_t key = 1U; key < DOOM_TABOS_INPUT_KEY_COUNT; ++key) {
        if (key == skip_one || key == skip_two || desired[key] != pressed || input->logical_down[key] == pressed) {
            continue;
        }
        if (!enqueue(input, pressed, (unsigned char) key)) {
            return;
        }
    }
}

static void update_logical_keys(doom_tabos_input_t* input)
{
    bool desired[DOOM_TABOS_INPUT_KEY_COUNT] = {false};
    for (size_t key = 1U; key < DOOM_TABOS_SOURCE_KEY_COUNT; ++key) {
        if (!input->source_down[key]) {
            continue;
        }
        const unsigned char mapped = mapped_key((tabos_key_t) key);
        if (mapped != 0U) {
            desired[mapped] = true;
        }
    }
    desired[DOOM_KEY_FIRE]  = desired[DOOM_KEY_FIRE] || input->control_down;
    desired[DOOM_KEY_SHIFT] = input->always_run || input->shift_down;

    set_desired(input, desired, false, DOOM_KEY_FIRE, DOOM_KEY_SHIFT);
    if (input->recovering) {
        return;
    }
    if (!desired[DOOM_KEY_FIRE] && input->logical_down[DOOM_KEY_FIRE] && !enqueue(input, false, DOOM_KEY_FIRE)) {
        return;
    }
    if (!desired[DOOM_KEY_SHIFT] && input->logical_down[DOOM_KEY_SHIFT] && !enqueue(input, false, DOOM_KEY_SHIFT)) {
        return;
    }
    if (desired[DOOM_KEY_FIRE] && !input->logical_down[DOOM_KEY_FIRE] && !enqueue(input, true, DOOM_KEY_FIRE)) {
        return;
    }
    if (desired[DOOM_KEY_SHIFT] && !input->logical_down[DOOM_KEY_SHIFT] && !enqueue(input, true, DOOM_KEY_SHIFT)) {
        return;
    }
    set_desired(input, desired, true, DOOM_KEY_FIRE, DOOM_KEY_SHIFT);
}

void doom_tabos_input_init(doom_tabos_input_t* input)
{
    if (input != NULL) {
        memset(input, 0, sizeof(*input));
    }
}

void doom_tabos_input_feed(doom_tabos_input_t* input, const tabos_input_event_t* event)
{
    if (input == NULL || event == NULL || input->recovering ||
        (event->type != TABOS_INPUT_KEY_DOWN && event->type != TABOS_INPUT_KEY_UP)) {
        return;
    }

    const bool down     = event->type == TABOS_INPUT_KEY_DOWN;
    input->control_down = (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U;
    input->shift_down   = (event->modifiers & TABOS_MODIFIER_SHIFT) != 0U;
    if (event->key == TABOS_KEY_CTRL) {
        input->control_down = down;
    } else if (event->key == TABOS_KEY_SHIFT) {
        input->shift_down = down;
    }

    if (!(down && event->repeat) && (unsigned int) event->key < (unsigned int) DOOM_TABOS_SOURCE_KEY_COUNT) {
        input->source_down[(unsigned int) event->key] = down;
        if (event->key == TABOS_KEY_R && down) {
            input->always_run = !input->always_run;
        }
    }
    update_logical_keys(input);
}

bool doom_tabos_input_pop(doom_tabos_input_t* input, int* pressed, unsigned char* key)
{
    if (input == NULL || pressed == NULL || key == NULL) {
        return false;
    }

    if (input->recovering) {
        while (input->recovery_key < DOOM_TABOS_INPUT_KEY_COUNT) {
            const size_t recovering_key = input->recovery_key++;
            if (!input->delivered_down[recovering_key]) {
                continue;
            }
            input->delivered_down[recovering_key] = false;
            *pressed                              = 0;
            *key                                  = (unsigned char) recovering_key;
            return true;
        }
        input->recovering = false;
        return false;
    }

    if (input->queue_count == 0U) {
        return false;
    }
    const doom_tabos_key_event_t event = input->queue[input->queue_head];
    input->queue_head                  = (input->queue_head + 1U) % DOOM_TABOS_INPUT_QUEUE_CAPACITY;
    --input->queue_count;
    input->delivered_down[event.key] = event.pressed;
    *pressed                         = event.pressed ? 1 : 0;
    *key                             = event.key;
    return true;
}
