#include <tabos/internal/input.h>

#include <tabos/config/input.h>
#include <tabos/platform/platform.h>

#include <stdatomic.h>
#if TABOS_ENABLE_KEYBOARD_DIAGNOSTICS
#include <stdio.h>
#endif
#include <string.h>

enum { INPUT_QUEUE_CAPACITY = 64 };

static tabos_input_event_t event_queue[INPUT_QUEUE_CAPACITY];
static size_t queue_head;
static size_t queue_count;
static atomic_flag queue_lock = ATOMIC_FLAG_INIT;

#if TABOS_ENABLE_KEYBOARD_DIAGNOSTICS
static const char *key_name(tabos_key_t key)
{
    static const char *const letters[] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    };
    static const char *const digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

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
    default: return "UNKNOWN";
    }
}

static void append_modifier(char *line, size_t line_size, const char *name, bool *first)
{
    const size_t used = strlen(line);
    (void)snprintf(line + used, line_size - used, "%s%s", *first ? "" : "+", name);
    *first = false;
}

static void log_input_event(const tabos_input_event_t *event)
{
    char line[192];
    if (event->type == TABOS_INPUT_TEXT) {
        char escaped[(TABOS_INPUT_TEXT_MAX_BYTES * 4U) + 1U];
        size_t output = 0U;
        for (size_t input = 0U; event->text[input] != '\0' && output + 4U < sizeof(escaped); ++input) {
            const unsigned char byte = (unsigned char)event->text[input];
            if (byte == '\n' || byte == '\r' || byte == '\t' || byte == '\\' || byte == '"') {
                escaped[output++] = '\\';
                escaped[output++] = byte == '\n' ? 'n' : byte == '\r' ? 'r' :
                    byte == '\t' ? 't' : (char)byte;
            } else if (byte >= 0x20U && byte != 0x7fU) {
                escaped[output++] = (char)byte;
            } else {
                output += (size_t)snprintf(
                    escaped + output, sizeof(escaped) - output, "\\x%02X", byte);
            }
        }
        escaped[output] = '\0';
        (void)snprintf(line, sizeof(line), "keyboard: TEXT text=\"%s\"", escaped);
    } else {
        (void)snprintf(line, sizeof(line), "keyboard: KEY_%s key=%s usage=%u modifiers=",
            event->type == TABOS_INPUT_KEY_DOWN ? "DOWN" : "UP", key_name(event->key),
            (unsigned int)event->key);
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
        if (first) {
            append_modifier(line, sizeof(line), "NONE", &first);
        }
        const size_t used = strlen(line);
        (void)snprintf(line + used, sizeof(line) - used, " repeat=%s",
            event->repeat ? "yes" : "no");
    }
    tab_platform_log(line);
}
#endif

static void lock_queue(void)
{
    while (atomic_flag_test_and_set_explicit(&queue_lock, memory_order_acquire)) {
    }
}

static void unlock_queue(void)
{
    atomic_flag_clear_explicit(&queue_lock, memory_order_release);
}

void tab_input_init(void)
{
    lock_queue();
    queue_head = 0U;
    queue_count = 0U;
    unlock_queue();
}

void tab_input_shutdown(void)
{
    tab_input_init();
}

bool tab_input_submit(const tabos_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    lock_queue();
    if (queue_count == INPUT_QUEUE_CAPACITY) {
        queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
        --queue_count;
    }
    const size_t tail = (queue_head + queue_count) % INPUT_QUEUE_CAPACITY;
    event_queue[tail] = *event;
    ++queue_count;
    unlock_queue();
#if TABOS_ENABLE_KEYBOARD_DIAGNOSTICS
    log_input_event(event);
#endif
    return true;
}

static bool pop_event(tabos_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    lock_queue();
    if (queue_count == 0U) {
        unlock_queue();
        return false;
    }
    *event = event_queue[queue_head];
    queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
    --queue_count;
    unlock_queue();
    return true;
}

bool tabos_input_poll(tabos_input_event_t *event)
{
    return pop_event(event);
}

bool tabos_input_wait(tabos_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    while (!pop_event(event)) {
        tab_platform_input_wait();
    }
    return true;
}

size_t tab_input_text_from_hid(uint8_t usage, uint8_t modifiers, char *text, size_t text_size)
{
    if (text == NULL || text_size < 2U ||
        (modifiers & (TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_ALT | TABOS_MODIFIER_GUI)) != 0U) {
        return 0U;
    }

    const bool shifted = (modifiers & TABOS_MODIFIER_SHIFT) != 0U;
    char character = '\0';
    if (usage >= TABOS_KEY_A && usage <= TABOS_KEY_Z) {
        character = (char)((shifted ? 'A' : 'a') + (usage - TABOS_KEY_A));
    } else if (usage >= TABOS_KEY_1 && usage <= TABOS_KEY_0) {
        static const char plain[] = "1234567890";
        static const char shift[] = "!@#$%^&*()";
        character = shifted ? shift[usage - TABOS_KEY_1] : plain[usage - TABOS_KEY_1];
    } else {
        switch (usage) {
        case TABOS_KEY_ENTER: character = '\n'; break;
        case TABOS_KEY_TAB: character = '\t'; break;
        case TABOS_KEY_SPACE: character = ' '; break;
        case TABOS_KEY_MINUS: character = shifted ? '_' : '-'; break;
        case TABOS_KEY_EQUALS: character = shifted ? '+' : '='; break;
        case TABOS_KEY_LEFT_BRACKET: character = shifted ? '{' : '['; break;
        case TABOS_KEY_RIGHT_BRACKET: character = shifted ? '}' : ']'; break;
        case TABOS_KEY_BACKSLASH: character = shifted ? '|' : '\\'; break;
        case TABOS_KEY_SEMICOLON: character = shifted ? ':' : ';'; break;
        case TABOS_KEY_APOSTROPHE: character = shifted ? '"' : '\''; break;
        case TABOS_KEY_GRAVE: character = shifted ? '~' : '`'; break;
        case TABOS_KEY_COMMA: character = shifted ? '<' : ','; break;
        case TABOS_KEY_PERIOD: character = shifted ? '>' : '.'; break;
        case TABOS_KEY_SLASH: character = shifted ? '?' : '/'; break;
        default: break;
        }
    }
    if (character == '\0') {
        return 0U;
    }
    text[0] = character;
    text[1] = '\0';
    return 1U;
}
