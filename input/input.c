#include <tabos/internal/input.h>
#include <tabos/internal/time.h>

#include <tabos/config/input.h>
#include <tabos/platform/platform.h>

#include <string.h>

enum {
    INPUT_QUEUE_CAPACITY = 64
};

static tabos_input_event_t event_queue[INPUT_QUEUE_CAPACITY];
static size_t queue_head;
static size_t queue_count;
static tabos_key_t held_key;
static uint8_t held_modifiers;
static char held_text[TABOS_INPUT_TEXT_MAX_BYTES + 1U];
static uint8_t held_text_modifiers;
static tabos_timer_t repeat_timer;
static platform_mutex_t* queue_mutex;
static platform_signal_t* queue_signal;

static bool modifier_key(tabos_key_t key)
{
    return key >= TABOS_KEY_CTRL && key <= TABOS_KEY_SYM;
}

static bool lock_queue(void)
{
    if (queue_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(queue_mutex);
    return true;
}

static void unlock_queue(void)
{
    platform_mutex_unlock(queue_mutex);
}

bool input_init(void)
{
    if (queue_mutex == NULL) {
        queue_mutex = platform_mutex_create();
        if (queue_mutex == NULL) {
            return false;
        }
    }
    if (queue_signal == NULL) {
        queue_signal = platform_signal_create();
        if (queue_signal == NULL) {
            platform_mutex_destroy(queue_mutex);
            queue_mutex = NULL;
            return false;
        }
    }
    (void) lock_queue();
    queue_head          = 0U;
    queue_count         = 0U;
    held_key            = TABOS_KEY_UNKNOWN;
    held_modifiers      = 0U;
    held_text[0]        = '\0';
    held_text_modifiers = 0U;
    tabos_timer_cancel(&repeat_timer);
    unlock_queue();
    return true;
}

void input_shutdown(void)
{
    if (!lock_queue()) {
        return;
    }
    queue_head          = 0U;
    queue_count         = 0U;
    held_key            = TABOS_KEY_UNKNOWN;
    held_modifiers      = 0U;
    held_text[0]        = '\0';
    held_text_modifiers = 0U;
    tabos_timer_cancel(&repeat_timer);
    unlock_queue();
    platform_mutex_destroy(queue_mutex);
    queue_mutex = NULL;
    platform_signal_destroy(queue_signal);
    queue_signal = NULL;
}

bool input_submit(const tabos_input_event_t* event)
{
    if (event == NULL) {
        return false;
    }
    /* Platform repeat timing differs. TabOS generates one portable repeat stream. */
    if (event->repeat) {
        return true;
    }
    if (!lock_queue()) {
        return false;
    }
    if (event->type == TABOS_INPUT_KEY_DOWN && !modifier_key(event->key)) {
        held_key            = event->key;
        held_modifiers      = event->modifiers;
        held_text[0]        = '\0';
        held_text_modifiers = 0U;
        tabos_timer_start(&repeat_timer, TABOS_KEY_REPEAT_DELAY_MS, TABOS_KEY_REPEAT_INTERVAL_MS);
    } else if (event->type == TABOS_INPUT_KEY_UP && event->key == held_key) {
        held_key            = TABOS_KEY_UNKNOWN;
        held_modifiers      = 0U;
        held_text[0]        = '\0';
        held_text_modifiers = 0U;
        tabos_timer_cancel(&repeat_timer);
    } else if (event->type == TABOS_INPUT_TEXT && held_key != TABOS_KEY_UNKNOWN) {
        (void) strncpy(held_text, event->text, sizeof(held_text) - 1U);
        held_text[sizeof(held_text) - 1U] = '\0';
        held_text_modifiers               = event->modifiers;
    }
    if (queue_count == INPUT_QUEUE_CAPACITY) {
        queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
        --queue_count;
    }
    const size_t tail = (queue_head + queue_count) % INPUT_QUEUE_CAPACITY;
    event_queue[tail] = *event;
    ++queue_count;
    unlock_queue();
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    input_wake_waiter();
    input_diagnostic_log(event);
    return true;
}

void input_update(void)
{
    if (!lock_queue()) {
        return;
    }
    if (held_key == TABOS_KEY_UNKNOWN || !tabos_timer_poll(&repeat_timer)) {
        unlock_queue();
        return;
    }

    tabos_input_event_t key_event = {
        .type      = TABOS_INPUT_KEY_DOWN,
        .key       = held_key,
        .modifiers = held_modifiers,
        .repeat    = true,
    };
    if (queue_count == INPUT_QUEUE_CAPACITY) {
        queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
        --queue_count;
    }
    size_t tail       = (queue_head + queue_count) % INPUT_QUEUE_CAPACITY;
    event_queue[tail] = key_event;
    ++queue_count;

    bool text_repeated             = false;
    tabos_input_event_t text_event = {0};
    if (held_text[0] != '\0') {
        text_event = (tabos_input_event_t) {
            .type      = TABOS_INPUT_TEXT,
            .modifiers = held_text_modifiers,
            .repeat    = true,
        };
        (void) strncpy(text_event.text, held_text, sizeof(text_event.text) - 1U);
        text_event.text[sizeof(text_event.text) - 1U] = '\0';
        if (queue_count == INPUT_QUEUE_CAPACITY) {
            queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
            --queue_count;
        }
        tail              = (queue_head + queue_count) % INPUT_QUEUE_CAPACITY;
        event_queue[tail] = text_event;
        ++queue_count;
        text_repeated = true;
    }
    unlock_queue();
    input_wake_waiter();
    input_diagnostic_log(&key_event);
    if (text_repeated) {
        input_diagnostic_log(&text_event);
    }
}

uint64_t input_next_deadline(void)
{
    if (!lock_queue()) {
        return TIME_DEADLINE_NONE;
    }
    const uint64_t deadline = held_key != TABOS_KEY_UNKNOWN ? time_timer_deadline(&repeat_timer) : TIME_DEADLINE_NONE;
    unlock_queue();
    return deadline;
}

static bool pop_event(tabos_input_event_t* event)
{
    if (event == NULL || !lock_queue()) {
        return false;
    }
    if (queue_count == 0U) {
        unlock_queue();
        return false;
    }
    *event     = event_queue[queue_head];
    queue_head = (queue_head + 1U) % INPUT_QUEUE_CAPACITY;
    --queue_count;
    unlock_queue();
    return true;
}

bool tabos_input_poll(tabos_input_event_t* event)
{
    return pop_event(event);
}

bool tabos_input_wait(tabos_input_event_t* event)
{
    if (event == NULL) {
        return false;
    }
    while (!pop_event(event)) {
        platform_input_wait();
    }
    return true;
}

size_t input_text_from_hid(uint8_t usage, uint8_t modifiers, char* text, size_t text_size)
{
    if (text == NULL || text_size < 2U ||
        (modifiers & (TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_ALT | TABOS_MODIFIER_GUI)) != 0U) {
        return 0U;
    }

    const bool shifted = (modifiers & TABOS_MODIFIER_SHIFT) != 0U;
    char character     = '\0';
    if (usage >= TABOS_KEY_A && usage <= TABOS_KEY_Z) {
        character = (char) ((shifted ? 'A' : 'a') + (usage - TABOS_KEY_A));
    } else if (usage >= TABOS_KEY_1 && usage <= TABOS_KEY_0) {
        static const char plain[] = "1234567890";
        static const char shift[] = "!@#$%^&*()";
        character                 = shifted ? shift[usage - TABOS_KEY_1] : plain[usage - TABOS_KEY_1];
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

bool input_pending(void)
{
    if (!lock_queue()) {
        return false;
    }
    const bool pending = queue_count != 0U;
    unlock_queue();
    return pending;
}
void input_wait_ready(uint32_t timeout_ms)
{
    platform_signal_wait(queue_signal, timeout_ms);
}
void input_wake_waiter(void)
{
    platform_signal_notify(queue_signal);
}
