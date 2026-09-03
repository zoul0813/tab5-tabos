#include "internal.h"

#include <tabos/internal/input.h>
#include <tabos/platform/platform.h>

#include <stdio.h>
#include <string.h>

static char synthesized_text[TABOS_INPUT_TEXT_MAX_BYTES + 1U];
static bool screenshot_shortcut_active;
static bool sym_down;
static bool sym_latched;
static bool sym_used;

static tabos_key_t sym_key(tabos_key_t key, uint8_t* modifiers)
{
    switch (key) {
        case TABOS_KEY_GRAVE:
        case TABOS_KEY_LEFT_BRACKET:
        case TABOS_KEY_RIGHT_BRACKET:
        case TABOS_KEY_BACKSLASH:
        case TABOS_KEY_SEMICOLON:
        case TABOS_KEY_APOSTROPHE: *modifiers |= TABOS_MODIFIER_SHIFT; return key;
        case TABOS_KEY_1: *modifiers |= TABOS_MODIFIER_SHIFT; return TABOS_KEY_SLASH;
        case TABOS_KEY_8: return TABOS_KEY_SLASH;
        case TABOS_KEY_9: *modifiers |= TABOS_MODIFIER_SHIFT; return TABOS_KEY_COMMA;
        case TABOS_KEY_0: *modifiers |= TABOS_MODIFIER_SHIFT; return TABOS_KEY_PERIOD;
        case TABOS_KEY_MINUS: return TABOS_KEY_EQUALS;
        case TABOS_KEY_PERIOD: return TABOS_KEY_COMMA;
        default: return key;
    }
}

static bool handle_screenshot_shortcut(const SDL_KeyboardEvent* event)
{
    const bool is_f12 = event->scancode == SDL_SCANCODE_F12 || event->key == SDLK_F12;
    if (!is_f12) {
        return false;
    }
    if (event->type == SDL_EVENT_KEY_UP && screenshot_shortcut_active) {
        screenshot_shortcut_active = false;
        return true;
    }
    const SDL_Keymod modifiers = event->mod | SDL_GetModState();
    const bool has_gui         = (modifiers & SDL_KMOD_GUI) != 0U;
    const bool has_shift       = (modifiers & SDL_KMOD_SHIFT) != 0U;
    if (event->type != SDL_EVENT_KEY_DOWN || !has_gui || !has_shift) {
        return false;
    }
    screenshot_shortcut_active = true;
    if (!event->repeat) {
        (void) host_capture_screenshot();
    }
    return true;
}

static uint8_t input_modifiers(SDL_Keymod modifiers)
{
    uint8_t result = 0U;
    if ((modifiers & SDL_KMOD_CTRL) != 0U) {
        result |= TABOS_MODIFIER_CONTROL;
    }
    if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
        result |= TABOS_MODIFIER_SHIFT;
    }
#if defined(__APPLE__)
    if ((modifiers & SDL_KMOD_ALT) != 0U) {
        result |= TABOS_MODIFIER_SYM;
    }
    if ((modifiers & SDL_KMOD_GUI) != 0U) {
        result |= TABOS_MODIFIER_GUI;
    }
#else
    if ((modifiers & SDL_KMOD_ALT) != 0U) {
        result |= TABOS_MODIFIER_ALT;
    }
    if ((modifiers & SDL_KMOD_GUI) != 0U) {
        result |= TABOS_MODIFIER_SYM;
    }
#endif
    return result;
}

static tabos_key_t input_key(SDL_Scancode scancode)
{
    if ((scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_CAPSLOCK) ||
        (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) ||
        (scancode >= SDL_SCANCODE_INSERT && scancode <= SDL_SCANCODE_UP)) {
        return (tabos_key_t) scancode;
    }
    if (scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL) {
        return TABOS_KEY_CTRL;
    }
    if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
        return TABOS_KEY_SHIFT;
    }
#if defined(__APPLE__)
    if (scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
        return TABOS_KEY_SYM;
    }
    if (scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) {
        return TABOS_KEY_GUI;
    }
#else
    if (scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
        return TABOS_KEY_ALT;
    }
    if (scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) {
        return TABOS_KEY_SYM;
    }
#endif
    return TABOS_KEY_UNKNOWN;
}

static void dispatch_event(const SDL_Event* event)
{
    if (host_pointer_event(event)) {
        return;
    }
    if (event->type == SDL_EVENT_QUIT) {
        host_request_quit();
        return;
    }
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        if (handle_screenshot_shortcut(&event->key)) {
            return;
        }
        const tabos_key_t key                 = input_key(event->key.scancode);
        const uint8_t modifiers               = input_modifiers(event->key.mod);
        const tabos_input_event_t input_event = {
            .type      = event->type == SDL_EVENT_KEY_DOWN ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP,
            .key       = key,
            .modifiers = modifiers,
            .repeat    = event->key.repeat,
        };
        (void) input_submit(&input_event);
        if (key == TABOS_KEY_SYM && !event->key.repeat) {
            if (event->type == SDL_EVENT_KEY_DOWN) {
                sym_down = true;
                sym_used = false;
            } else {
                sym_down = false;
                if (!sym_used) {
                    sym_latched = true;
                }
            }
        }
        if (event->type == SDL_EVENT_KEY_UP) {
            synthesized_text[0] = '\0';
        } else if (key != TABOS_KEY_SYM && (sym_down || sym_latched)) {
            uint8_t cooked_modifiers       = modifiers | TABOS_MODIFIER_SYM;
            const tabos_key_t cooked_key   = sym_key(key, &cooked_modifiers);
            tabos_input_event_t text_event = {
                .type      = TABOS_INPUT_TEXT,
                .modifiers = cooked_modifiers,
            };
            if (input_text_from_hid((uint8_t) cooked_key, cooked_modifiers, text_event.text, sizeof(text_event.text)) >
                0U) {
                (void) input_submit(&text_event);
                (void) snprintf(synthesized_text, sizeof(synthesized_text), "%s", text_event.text);
            }
            if (sym_down) {
                sym_used = true;
            }
            sym_latched = false;
        } else if (event->key.repeat || key == TABOS_KEY_ENTER || key == TABOS_KEY_TAB) {
            tabos_input_event_t text_event = {
                .type      = TABOS_INPUT_TEXT,
                .modifiers = modifiers,
                .repeat    = event->key.repeat,
            };
            if (input_text_from_hid((uint8_t) key, modifiers, text_event.text, sizeof(text_event.text)) > 0U) {
                (void) input_submit(&text_event);
                (void) snprintf(synthesized_text, sizeof(synthesized_text), "%s", text_event.text);
            }
        }
        return;
    }
    if (event->type == SDL_EVENT_TEXT_INPUT) {
        if (synthesized_text[0] != '\0' && strcmp(synthesized_text, event->text.text) == 0) {
            synthesized_text[0] = '\0';
            return;
        }
        synthesized_text[0] = '\0';
        for (const unsigned char* byte = (const unsigned char*) event->text.text; *byte != '\0'; ++byte) {
            if (*byte >= 0x80U) {
                return;
            }
        }
        tabos_input_event_t input_event = {.type = TABOS_INPUT_TEXT};
        (void) snprintf(input_event.text, sizeof(input_event.text), "%s", event->text.text);
        (void) input_submit(&input_event);
    }
}

void host_input_update(bool wait)
{
    if (host_is_headless()) {
        return;
    }
    SDL_Event event;
    if (wait && SDL_WaitEventTimeout(&event, 50)) {
        dispatch_event(&event);
    }
    while (SDL_PollEvent(&event)) {
        dispatch_event(&event);
    }
}

void platform_input_wait(void)
{
    host_input_update(false);
    SDL_Delay(1);
}
