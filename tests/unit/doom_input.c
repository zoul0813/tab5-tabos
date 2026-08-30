#include "doom_tabos_input.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    DOOM_KEY_ESCAPE       = 27,
    DOOM_KEY_ENTER        = 13,
    DOOM_KEY_F1           = 0xbb,
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

static tabos_input_event_t key_event(tabos_input_event_type_t type, tabos_key_t key, uint8_t modifiers, bool repeat)
{
    const tabos_input_event_t event = {
        .type      = type,
        .key       = key,
        .modifiers = modifiers,
        .repeat    = repeat,
    };
    return event;
}

static bool expect(doom_tabos_input_t* input, bool expected_pressed, unsigned char expected_key)
{
    int pressed       = -1;
    unsigned char key = 0U;
    return doom_tabos_input_pop(input, &pressed, &key) && pressed == (expected_pressed ? 1 : 0) && key == expected_key;
}

static bool expect_empty(doom_tabos_input_t* input)
{
    int pressed;
    unsigned char key;
    return !doom_tabos_input_pop(input, &pressed, &key);
}

static int test_mappings(void)
{
    static const struct {
            tabos_key_t source;
            unsigned char doom;
    } mappings[] = {
        {     TABOS_KEY_W,           DOOM_KEY_UP},
        {     TABOS_KEY_S,         DOOM_KEY_DOWN},
        {     TABOS_KEY_A,  DOOM_KEY_STRAFE_LEFT},
        {     TABOS_KEY_D, DOOM_KEY_STRAFE_RIGHT},
        {     TABOS_KEY_J,         DOOM_KEY_FIRE},
        {     TABOS_KEY_E,          DOOM_KEY_USE},
        { TABOS_KEY_SPACE,          DOOM_KEY_USE},
        {  TABOS_KEY_LEFT,         DOOM_KEY_LEFT},
        { TABOS_KEY_RIGHT,        DOOM_KEY_RIGHT},
        {TABOS_KEY_ESCAPE,       DOOM_KEY_ESCAPE},
        { TABOS_KEY_ENTER,        DOOM_KEY_ENTER},
        {     TABOS_KEY_Y,                   'y'},
        {     TABOS_KEY_N,                   'n'},
        {     TABOS_KEY_1,                   '1'},
        {     TABOS_KEY_0,                   '0'},
        {    TABOS_KEY_F1,           DOOM_KEY_F1},
        {   TABOS_KEY_F12,          DOOM_KEY_F12},
    };
    doom_tabos_input_t input;
    doom_tabos_input_init(&input);

    for (size_t index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        tabos_input_event_t event = key_event(TABOS_INPUT_KEY_DOWN, mappings[index].source, 0U, false);
        doom_tabos_input_feed(&input, &event);
        if (!expect(&input, true, mappings[index].doom)) {
            return 1;
        }
        event = key_event(TABOS_INPUT_KEY_UP, mappings[index].source, 0U, false);
        doom_tabos_input_feed(&input, &event);
        if (!expect(&input, false, mappings[index].doom)) {
            return 1;
        }
    }
    return expect_empty(&input) ? 0 : 1;
}

static int test_modifiers_and_aliases(void)
{
    doom_tabos_input_t input;
    doom_tabos_input_init(&input);

    tabos_input_event_t event =
        key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_W, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_FIRE) || !expect(&input, true, DOOM_KEY_SHIFT) ||
        !expect(&input, true, DOOM_KEY_UP)) {
        return 1;
    }

    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_J, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect_empty(&input)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_UP, TABOS_KEY_J, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect_empty(&input)) {
        return 1;
    }

    event = key_event(TABOS_INPUT_KEY_UP, TABOS_KEY_W, 0U, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, false, DOOM_KEY_UP) || !expect(&input, false, DOOM_KEY_FIRE) ||
        !expect(&input, false, DOOM_KEY_SHIFT) || !expect_empty(&input)) {
        return 1;
    }

    doom_tabos_input_init(&input);
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_CTRL, 0U, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_FIRE)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_SHIFT, TABOS_MODIFIER_CONTROL, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_SHIFT)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_UP, TABOS_KEY_CTRL, TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, false, DOOM_KEY_FIRE)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_UP, TABOS_KEY_SHIFT, 0U, false);
    doom_tabos_input_feed(&input, &event);
    return expect(&input, false, DOOM_KEY_SHIFT) && expect_empty(&input) ? 0 : 1;
}

static int test_repeat_and_always_run(void)
{
    doom_tabos_input_t input;
    doom_tabos_input_init(&input);

    tabos_input_event_t event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_W, 0U, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_UP)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_W, 0U, true);
    doom_tabos_input_feed(&input, &event);
    if (!expect_empty(&input)) {
        return 1;
    }

    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_R, 0U, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_SHIFT)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_R, 0U, true);
    doom_tabos_input_feed(&input, &event);
    if (!expect_empty(&input)) {
        return 1;
    }
    event = key_event(TABOS_INPUT_KEY_UP, TABOS_KEY_R, 0U, false);
    doom_tabos_input_feed(&input, &event);
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_R, 0U, false);
    doom_tabos_input_feed(&input, &event);
    return expect(&input, false, DOOM_KEY_SHIFT) && expect_empty(&input) ? 0 : 1;
}

static int test_overflow_recovery(void)
{
    doom_tabos_input_t input;
    doom_tabos_input_init(&input);

    tabos_input_event_t event =
        key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_W, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);
    if (!expect(&input, true, DOOM_KEY_FIRE) || !expect(&input, true, DOOM_KEY_SHIFT) ||
        !expect(&input, true, DOOM_KEY_UP)) {
        return 1;
    }

    for (size_t index = 0U; index < DOOM_TABOS_INPUT_QUEUE_CAPACITY; ++index) {
        const tabos_input_event_type_t type = index % 2U == 0U ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP;
        event = key_event(type, TABOS_KEY_E, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
        doom_tabos_input_feed(&input, &event);
    }
    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_A, TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_SHIFT, false);
    doom_tabos_input_feed(&input, &event);

    bool released_fire  = false;
    bool released_up    = false;
    bool released_shift = false;
    int pressed;
    unsigned char key;
    while (doom_tabos_input_pop(&input, &pressed, &key)) {
        if (pressed != 0) {
            return 1;
        }
        released_fire  = released_fire || key == DOOM_KEY_FIRE;
        released_up    = released_up || key == DOOM_KEY_UP;
        released_shift = released_shift || key == DOOM_KEY_SHIFT;
    }
    if (!released_fire || !released_up || !released_shift) {
        return 1;
    }

    event = key_event(TABOS_INPUT_KEY_DOWN, TABOS_KEY_W, 0U, false);
    doom_tabos_input_feed(&input, &event);
    return expect(&input, true, DOOM_KEY_UP) && expect_empty(&input) ? 0 : 1;
}

int main(void)
{
    if (test_mappings() != 0) {
        return 1;
    }
    if (test_modifiers_and_aliases() != 0) {
        return 2;
    }
    if (test_repeat_and_always_run() != 0) {
        return 3;
    }
    return test_overflow_recovery() != 0 ? 4 : 0;
}
