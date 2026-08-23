#include <tabos/internal/input.h>

#include <tabos/config/input.h>
#include <tabos/platform/platform.h>

#if TABOS_ENABLE_KEYBOARD_DIAGNOSTICS
#include <stdio.h>
#include <string.h>

static const char* key_name(tabos_key_t key)
{
    static const char* const letters[] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    };
    static const char* const digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    if (key >= TABOS_KEY_A && key <= TABOS_KEY_Z) {
        return letters[key - TABOS_KEY_A];
    }
    if (key >= TABOS_KEY_1 && key <= TABOS_KEY_0) {
        return digits[key - TABOS_KEY_1];
    }
    switch (key) {
        case TABOS_KEY_ENTER: return "ENTER";
        case TABOS_KEY_ESCAPE: return "ESCAPE";
        case TABOS_KEY_BACKSPACE: return "BACKSPACE";
        case TABOS_KEY_TAB: return "TAB";
        case TABOS_KEY_SPACE: return "SPACE";
        case TABOS_KEY_MINUS: return "MINUS";
        case TABOS_KEY_EQUALS: return "EQUALS";
        case TABOS_KEY_LEFT_BRACKET: return "LEFT_BRACKET";
        case TABOS_KEY_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TABOS_KEY_BACKSLASH: return "BACKSLASH";
        case TABOS_KEY_SEMICOLON: return "SEMICOLON";
        case TABOS_KEY_APOSTROPHE: return "APOSTROPHE";
        case TABOS_KEY_GRAVE: return "GRAVE";
        case TABOS_KEY_COMMA: return "COMMA";
        case TABOS_KEY_PERIOD: return "PERIOD";
        case TABOS_KEY_SLASH: return "SLASH";
        case TABOS_KEY_CAPS_LOCK: return "CAPS_LOCK";
        case TABOS_KEY_F1: return "F1";
        case TABOS_KEY_F2: return "F2";
        case TABOS_KEY_F3: return "F3";
        case TABOS_KEY_F4: return "F4";
        case TABOS_KEY_F5: return "F5";
        case TABOS_KEY_F6: return "F6";
        case TABOS_KEY_F7: return "F7";
        case TABOS_KEY_F8: return "F8";
        case TABOS_KEY_F9: return "F9";
        case TABOS_KEY_F10: return "F10";
        case TABOS_KEY_F11: return "F11";
        case TABOS_KEY_F12: return "F12";
        case TABOS_KEY_INSERT: return "INSERT";
        case TABOS_KEY_HOME: return "HOME";
        case TABOS_KEY_PAGE_UP: return "PAGE_UP";
        case TABOS_KEY_DELETE: return "DELETE";
        case TABOS_KEY_END: return "END";
        case TABOS_KEY_PAGE_DOWN: return "PAGE_DOWN";
        case TABOS_KEY_RIGHT: return "RIGHT";
        case TABOS_KEY_LEFT: return "LEFT";
        case TABOS_KEY_DOWN: return "DOWN";
        case TABOS_KEY_UP: return "UP";
        case TABOS_KEY_CTRL: return "CTRL";
        case TABOS_KEY_SHIFT: return "SHIFT";
        case TABOS_KEY_ALT: return "ALT";
        case TABOS_KEY_GUI: return "GUI";
        case TABOS_KEY_SYM: return "SYM";
        default: return "UNKNOWN";
    }
}

static void append_modifier(char* line, size_t line_size, const char* name, bool* first)
{
    const size_t used = strlen(line);
    (void) snprintf(line + used, line_size - used, "%s%s", *first ? "" : "+", name);
    *first = false;
}

void input_diagnostic_log(const tabos_input_event_t* event)
{
    char line[192];
    if (event->type == TABOS_INPUT_TEXT) {
        char escaped[(TABOS_INPUT_TEXT_MAX_BYTES * 4U) + 1U];
        size_t output = 0U;
        for (size_t input = 0U; event->text[input] != '\0' && output + 4U < sizeof(escaped); ++input) {
            const unsigned char byte = (unsigned char) event->text[input];
            if (byte == '\n' || byte == '\r' || byte == '\t' || byte == '\\' || byte == '"') {
                escaped[output++] = '\\';
                escaped[output++] = byte == '\n' ? 'n' : byte == '\r' ? 'r' : byte == '\t' ? 't' : (char) byte;
            } else if (byte >= 0x20U && byte != 0x7fU) {
                escaped[output++] = (char) byte;
            } else {
                output += (size_t) snprintf(escaped + output, sizeof(escaped) - output, "\\x%02X", byte);
            }
        }
        escaped[output] = '\0';
        (void) snprintf(line, sizeof(line), "keyboard: TEXT text=\"%s\"", escaped);
    } else {
        (void) snprintf(line, sizeof(line), "keyboard: KEY_%s key=%s usage=%u modifiers=",
                        event->type == TABOS_INPUT_KEY_DOWN ? "DOWN" : "UP", key_name(event->key),
                        (unsigned int) event->key);
        bool first = true;
        if ((event->modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            append_modifier(line, sizeof(line), "CTRL", &first);
        }
        if ((event->modifiers & TABOS_MODIFIER_SHIFT) != 0U) {
            append_modifier(line, sizeof(line), "SHIFT", &first);
        }
        if ((event->modifiers & TABOS_MODIFIER_ALT) != 0U) {
            append_modifier(line, sizeof(line), "ALT", &first);
        }
        if ((event->modifiers & TABOS_MODIFIER_GUI) != 0U) {
            append_modifier(line, sizeof(line), "GUI", &first);
        }
        if ((event->modifiers & TABOS_MODIFIER_SYM) != 0U) {
            append_modifier(line, sizeof(line), "SYM", &first);
        }
        if (first) {
            append_modifier(line, sizeof(line), "NONE", &first);
        }
        const size_t used = strlen(line);
        (void) snprintf(line + used, sizeof(line) - used, " repeat=%s", event->repeat ? "yes" : "no");
    }
    platform_log(line);
}
#else
void input_diagnostic_log(const tabos_input_event_t* event)
{
    (void) event;
}
#endif
