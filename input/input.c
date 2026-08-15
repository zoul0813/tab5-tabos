#include <tabos/internal/input.h>

#include <tabos/platform/platform.h>

#include <stdatomic.h>
#include <string.h>

enum { INPUT_QUEUE_CAPACITY = 64 };

static tabos_input_event_t event_queue[INPUT_QUEUE_CAPACITY];
static size_t queue_head;
static size_t queue_count;
static atomic_flag queue_lock = ATOMIC_FLAG_INIT;

static void lock_queue(void)
{
    while (atomic_flag_test_and_set_explicit(&queue_lock, memory_order_acquire)) {
    }
}

static void unlock_queue(void)
{
    atomic_flag_clear_explicit(&queue_lock, memory_order_release);
}

void input_init(void)
{
    lock_queue();
    queue_head = 0U;
    queue_count = 0U;
    unlock_queue();
}

void input_shutdown(void)
{
    input_init();
}

bool input_submit(const tabos_input_event_t *event)
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
    input_diagnostic_log(event);
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
        platform_input_wait();
    }
    return true;
}

size_t input_text_from_hid(uint8_t usage, uint8_t modifiers, char *text, size_t text_size)
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
