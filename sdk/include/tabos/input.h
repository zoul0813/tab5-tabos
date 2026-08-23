#ifndef TABOS_INPUT_H
#define TABOS_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TABOS_INPUT_KEY_DOWN,
    TABOS_INPUT_KEY_UP,
    TABOS_INPUT_TEXT,
} tabos_input_event_type_t;

typedef enum {
    TABOS_KEY_UNKNOWN = 0,
    TABOS_KEY_A = 4,
    TABOS_KEY_B,
    TABOS_KEY_C,
    TABOS_KEY_D,
    TABOS_KEY_E,
    TABOS_KEY_F,
    TABOS_KEY_G,
    TABOS_KEY_H,
    TABOS_KEY_I,
    TABOS_KEY_J,
    TABOS_KEY_K,
    TABOS_KEY_L,
    TABOS_KEY_M,
    TABOS_KEY_N,
    TABOS_KEY_O,
    TABOS_KEY_P,
    TABOS_KEY_Q,
    TABOS_KEY_R,
    TABOS_KEY_S,
    TABOS_KEY_T,
    TABOS_KEY_U,
    TABOS_KEY_V,
    TABOS_KEY_W,
    TABOS_KEY_X,
    TABOS_KEY_Y,
    TABOS_KEY_Z,
    TABOS_KEY_1,
    TABOS_KEY_2,
    TABOS_KEY_3,
    TABOS_KEY_4,
    TABOS_KEY_5,
    TABOS_KEY_6,
    TABOS_KEY_7,
    TABOS_KEY_8,
    TABOS_KEY_9,
    TABOS_KEY_0,
    TABOS_KEY_ENTER,
    TABOS_KEY_ESCAPE,
    TABOS_KEY_BACKSPACE,
    TABOS_KEY_TAB,
    TABOS_KEY_SPACE,
    TABOS_KEY_MINUS,
    TABOS_KEY_EQUALS,
    TABOS_KEY_LEFT_BRACKET,
    TABOS_KEY_RIGHT_BRACKET,
    TABOS_KEY_BACKSLASH,
    TABOS_KEY_SEMICOLON = 51,
    TABOS_KEY_APOSTROPHE,
    TABOS_KEY_GRAVE,
    TABOS_KEY_COMMA,
    TABOS_KEY_PERIOD,
    TABOS_KEY_SLASH,
    TABOS_KEY_CAPS_LOCK,
    TABOS_KEY_F1,
    TABOS_KEY_F2,
    TABOS_KEY_F3,
    TABOS_KEY_F4,
    TABOS_KEY_F5,
    TABOS_KEY_F6,
    TABOS_KEY_F7,
    TABOS_KEY_F8,
    TABOS_KEY_F9,
    TABOS_KEY_F10,
    TABOS_KEY_F11,
    TABOS_KEY_F12,
    TABOS_KEY_INSERT = 73,
    TABOS_KEY_HOME,
    TABOS_KEY_PAGE_UP,
    TABOS_KEY_DELETE,
    TABOS_KEY_END,
    TABOS_KEY_PAGE_DOWN,
    TABOS_KEY_RIGHT,
    TABOS_KEY_LEFT,
    TABOS_KEY_DOWN,
    TABOS_KEY_UP,
    TABOS_KEY_CTRL = 0x100,
    TABOS_KEY_SHIFT,
    TABOS_KEY_ALT,
    TABOS_KEY_GUI,
} tabos_key_t;

enum {
    TABOS_MODIFIER_CONTROL = 1U << 0U,
    TABOS_MODIFIER_SHIFT = 1U << 1U,
    TABOS_MODIFIER_ALT = 1U << 2U,
    TABOS_MODIFIER_GUI = 1U << 3U,
};

enum { TABOS_INPUT_TEXT_MAX_BYTES = 9 };

typedef struct {
    tabos_input_event_type_t type;
    tabos_key_t key;
    uint8_t modifiers;
    bool repeat;
    char text[TABOS_INPUT_TEXT_MAX_BYTES + 1U];
} tabos_input_event_t;

bool tabos_input_poll(tabos_input_event_t *event);
bool tabos_input_wait(tabos_input_event_t *event);

#endif
