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

    input_init();
    const tabos_input_event_t key = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key  = TABOS_KEY_A,
    };
    if (!input_submit(&key)) {
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

    input_init();
    const tabos_input_event_t held = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key  = TABOS_KEY_W,
    };
    if (!input_submit(&held) || !tabos_input_poll(&received)) {
        return 1;
    }
    const tabos_input_event_t held_text = {
        .type = TABOS_INPUT_TEXT,
        .text = "w",
    };
    if (!input_submit(&held_text) || !tabos_input_poll(&received)) {
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
        strcmp(received.text, "w") != 0 || !received.repeat) {
        return 1;
    }

    const tabos_input_event_t released = {
        .type = TABOS_INPUT_KEY_UP,
        .key  = TABOS_KEY_W,
    };
    if (!input_submit(&released) || !tabos_input_poll(&received)) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_KEY_REPEAT_INTERVAL_MS);
    input_update();
    return tabos_input_poll(&received) ? 1 : 0;
}
