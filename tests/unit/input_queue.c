#include <tabos/internal/input.h>

#include <string.h>

int main(void)
{
    char text[4];
    if (tab_input_text_from_hid(TABOS_KEY_A, 0U, text, sizeof(text)) != 1U ||
        strcmp(text, "a") != 0 ||
        tab_input_text_from_hid(TABOS_KEY_A, TABOS_MODIFIER_SHIFT, text, sizeof(text)) != 1U ||
        strcmp(text, "A") != 0 ||
        tab_input_text_from_hid(TABOS_KEY_1, TABOS_MODIFIER_SHIFT, text, sizeof(text)) != 1U ||
        strcmp(text, "!") != 0 ||
        tab_input_text_from_hid(TABOS_KEY_A, TABOS_MODIFIER_CONTROL, text, sizeof(text)) != 0U) {
        return 1;
    }

    tab_input_init();
    const tabos_input_event_t key = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key = TABOS_KEY_A,
    };
    if (!tab_input_submit(&key)) {
        return 1;
    }
    tabos_input_event_t received;
    if (!tabos_input_poll(&received) || received.type != TABOS_INPUT_KEY_DOWN ||
        received.key != TABOS_KEY_A || tabos_input_poll(&received)) {
        return 1;
    }

    for (unsigned int index = 0U; index < 70U; ++index) {
        tabos_input_event_t event = {
            .type = TABOS_INPUT_KEY_DOWN,
            .key = TABOS_KEY_B,
            .modifiers = (uint8_t)index,
        };
        if (!tab_input_submit(&event)) {
            return 1;
        }
    }
    for (unsigned int index = 6U; index < 70U; ++index) {
        if (!tabos_input_poll(&received) || received.modifiers != (uint8_t)index) {
            return 1;
        }
    }
    return tabos_input_poll(&received) ? 1 : 0;
}
