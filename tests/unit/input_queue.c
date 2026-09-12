#include <tabos/internal/input.h>
#include <tabos/config/input.h>

#include "platform_test.h"

#include <string.h>

int main(void)
{
    char text[4];
    if (input_text_from_hid(TABOS_KEY_A, 0U, text, sizeof(text)) != 1U || strcmp(text, "a") != 0 ||
        input_text_from_hid(TABOS_KEY_A, TABOS_MODIFIER_SHIFT, text, sizeof(text)) != 1U || strcmp(text, "A") != 0 ||
        input_text_from_hid(TABOS_KEY_1, TABOS_MODIFIER_SHIFT, text, sizeof(text)) != 1U || strcmp(text, "!") != 0 ||
        input_text_from_hid(TABOS_KEY_5, TABOS_MODIFIER_SHIFT, text, sizeof(text)) != 1U || strcmp(text, "%") != 0 ||
        input_text_from_hid(TABOS_KEY_A, TABOS_MODIFIER_CONTROL, text, sizeof(text)) != 0U) {
        return 1;
    }

    if (!input_init()) {
        return 1;
    }
    bool power_held = true;
    if (input_take_power_activity(&power_held) || power_held) {
        return 1;
    }
    const tabos_input_event_t key = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key  = TABOS_KEY_A,
    };
    if (!input_submit(&key)) {
        return 1;
    }
    if (!input_take_power_activity(&power_held) || !power_held || input_take_power_activity(&power_held)) {
        return 1;
    }
    tabos_input_event_t received;
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN || received.key != TABOS_KEY_A ||
        tabos_input_poll(&received)) {
        return 1;
    }

    for (unsigned int index = 0U; index < 70U; ++index) {
        tabos_input_event_t event = {
            .type      = TABOS_INPUT_KEY_DOWN,
            .key       = TABOS_KEY_B,
            .modifiers = (uint8_t) index,
        };
        if (!input_submit(&event)) {
            return 1;
        }
    }
    for (unsigned int index = 6U; index < 70U; ++index) {
        if (!tabos_input_poll(&received) || received.modifiers != (uint8_t) index) {
            return 1;
        }
    }
    if (tabos_input_poll(&received)) {
        return 1;
    }

    if (!input_init()) {
        return 1;
    }
    const tabos_input_event_t held = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key  = TABOS_KEY_W,
    };
    if (!input_submit(&held) || !tabos_input_poll(&received)) {
        return 1;
    }
    if (!input_take_power_activity(&power_held) || !power_held) {
        return 1;
    }
    const uint64_t first_repeat_ms = test_platform_time_ms() + TABOS_KEY_REPEAT_DELAY_MS;
    if (input_next_deadline() != first_repeat_ms) {
        return 1;
    }
    const tabos_input_event_t held_text = {
        .type = TABOS_INPUT_TEXT,
        .text = "w",
    };
    if (!input_submit(&held_text) || !tabos_input_poll(&received)) {
        return 1;
    }
    if (input_take_power_activity(&power_held) || !power_held) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_KEY_REPEAT_DELAY_MS - 1U);
    input_update();
    if (tabos_input_poll(&received)) {
        return 1;
    }
    test_platform_advance_time_ms(1U);
    input_update();
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN || received.key != TABOS_KEY_W ||
        !received.repeat || !tabos_input_poll(&received) || received.type != TABOS_INPUT_TEXT ||
        strcmp(received.text, "w") != 0 || !received.repeat || tabos_input_poll(&received) ||
        input_next_deadline() != first_repeat_ms + TABOS_KEY_REPEAT_INTERVAL_MS) {
        return 1;
    }
    if (input_take_power_activity(&power_held) || !power_held) {
        return 1;
    }

    test_platform_advance_time_ms((TABOS_KEY_REPEAT_INTERVAL_MS * 5U) + 1U);
    input_update();
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN || !received.repeat ||
        !tabos_input_poll(&received) || received.type != TABOS_INPUT_TEXT || !received.repeat ||
        tabos_input_poll(&received) || input_next_deadline() <= test_platform_time_ms()) {
        return 1;
    }

    const tabos_input_event_t released = {
        .type = TABOS_INPUT_KEY_UP,
        .key  = TABOS_KEY_W,
    };
    if (!input_submit(&released) || input_next_deadline() != UINT64_MAX || !tabos_input_poll(&received)) {
        return 1;
    }
    if (!input_take_power_activity(&power_held) || power_held) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_KEY_REPEAT_INTERVAL_MS);
    input_update();
    if (tabos_input_poll(&received) || !input_submit(&held) || input_next_deadline() == UINT64_MAX) {
        return 1;
    }

    if (!input_init()) {
        return 1;
    }
    const tabos_input_event_t shifted_a = {
        .type      = TABOS_INPUT_KEY_DOWN,
        .key       = TABOS_KEY_A,
        .modifiers = TABOS_MODIFIER_SHIFT,
    };
    const tabos_input_event_t shifted_text = {
        .type      = TABOS_INPUT_TEXT,
        .modifiers = TABOS_MODIFIER_SHIFT,
        .text      = "A",
    };
    const tabos_input_event_t backend_repeat = {
        .type      = TABOS_INPUT_TEXT,
        .modifiers = TABOS_MODIFIER_SHIFT | TABOS_MODIFIER_SYM,
        .repeat    = true,
        .text      = "?",
    };
    if (!input_submit(&shifted_a) || !tabos_input_poll(&received) || !input_submit(&backend_repeat) ||
        tabos_input_poll(&received) || !input_submit(&shifted_text) || !tabos_input_poll(&received)) {
        return 1;
    }
    const tabos_input_event_t shift_up = {
        .type = TABOS_INPUT_KEY_UP,
        .key  = TABOS_KEY_SHIFT,
    };
    if (!input_submit(&shift_up) || !tabos_input_poll(&received)) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_KEY_REPEAT_DELAY_MS);
    input_update();
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN || received.key != TABOS_KEY_A ||
        received.modifiers != 0U || !received.repeat || !tabos_input_poll(&received) ||
        received.type != TABOS_INPUT_TEXT || received.modifiers != 0U || strcmp(received.text, "a") != 0 ||
        !received.repeat || tabos_input_poll(&received)) {
        return 1;
    }
    const tabos_input_event_t shift_down = {
        .type      = TABOS_INPUT_KEY_DOWN,
        .key       = TABOS_KEY_SHIFT,
        .modifiers = TABOS_MODIFIER_SHIFT,
    };
    if (!input_submit(&shift_down) || !tabos_input_poll(&received)) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_KEY_REPEAT_INTERVAL_MS);
    input_update();
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN || received.key != TABOS_KEY_A ||
        received.modifiers != TABOS_MODIFIER_SHIFT || !received.repeat || !tabos_input_poll(&received) ||
        received.type != TABOS_INPUT_TEXT || received.modifiers != TABOS_MODIFIER_SHIFT ||
        strcmp(received.text, "A") != 0 || !received.repeat || tabos_input_poll(&received)) {
        return 1;
    }
    if (!input_init() || input_next_deadline() != UINT64_MAX) {
        return 1;
    }
    input_shutdown();
    return 0;
}
