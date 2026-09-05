#include "keyboard_interrupt.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
        uint8_t status[4];
        uint8_t count[2];
        uint8_t events[40];
        uint8_t submitted[40];
        size_t status_index;
        size_t count_index;
        size_t event_index;
        size_t submitted_count;
        size_t clear_count;
        bool asserted[2];
        size_t asserted_index;
        bool fail_read;
} fake_keyboard_t;

static bool read_status(void* context, uint8_t* status)
{
    fake_keyboard_t* keyboard = context;
    if (keyboard->fail_read) {
        return false;
    }
    *status = keyboard->status[keyboard->status_index++];
    return true;
}

static bool read_count(void* context, uint8_t* count)
{
    fake_keyboard_t* keyboard = context;
    *count                    = keyboard->count[keyboard->count_index++];
    return true;
}

static bool read_event(void* context, uint8_t* event)
{
    fake_keyboard_t* keyboard = context;
    *event                    = keyboard->events[keyboard->event_index++];
    return true;
}

static bool clear_status(void* context)
{
    fake_keyboard_t* keyboard = context;
    ++keyboard->clear_count;
    return true;
}

static bool interrupt_asserted(void* context)
{
    fake_keyboard_t* keyboard = context;
    return keyboard->asserted[keyboard->asserted_index++];
}

static void submit_event(void* context, uint8_t event)
{
    fake_keyboard_t* keyboard                        = context;
    keyboard->submitted[keyboard->submitted_count++] = event;
}

static tab5_keyboard_interrupt_ops_t operations(fake_keyboard_t* keyboard)
{
    return (tab5_keyboard_interrupt_ops_t) {
        .context            = keyboard,
        .read_status        = read_status,
        .read_count         = read_count,
        .read_event         = read_event,
        .clear_status       = clear_status,
        .interrupt_asserted = interrupt_asserted,
        .submit_event       = submit_event,
    };
}

static void test_single_interrupt(void)
{
    fake_keyboard_t keyboard = {
        .status = {1U, 0U},
        .count  = {2U},
        .events = {0x80U, 0x00U},
    };
    const tab5_keyboard_interrupt_ops_t ops = operations(&keyboard);
    bool pending                            = true;
    assert(tab5_keyboard_interrupt_drain(&ops, 4U, &pending));
    assert(!pending);
    assert(keyboard.clear_count == 1U);
    assert(keyboard.submitted_count == 2U);
    assert(keyboard.submitted[0] == 0x80U);
    assert(keyboard.submitted[1] == 0x00U);
}

static void test_reasserted_interrupt_is_drained(void)
{
    fake_keyboard_t keyboard = {
        .status   = {1U, 0U, 1U, 0U},
        .count    = {1U, 1U},
        .events   = {0x81U, 0x01U},
        .asserted = {true, false},
    };
    const tab5_keyboard_interrupt_ops_t ops = operations(&keyboard);
    bool pending                            = false;
    assert(tab5_keyboard_interrupt_drain(&ops, 4U, &pending));
    assert(!pending);
    assert(keyboard.clear_count == 2U);
    assert(keyboard.submitted_count == 2U);
    assert(keyboard.submitted[0] == 0x81U);
    assert(keyboard.submitted[1] == 0x01U);
}

static void test_bounded_drain_reschedules(void)
{
    fake_keyboard_t keyboard = {
        .status   = {1U, 1U},
        .count    = {0U},
        .asserted = {true},
    };
    const tab5_keyboard_interrupt_ops_t ops = operations(&keyboard);
    bool pending                            = false;
    assert(tab5_keyboard_interrupt_drain(&ops, 1U, &pending));
    assert(pending);
    assert(keyboard.clear_count == 1U);
}

static void test_event_count_is_bounded(void)
{
    fake_keyboard_t keyboard = {
        .status = {1U, 0U},
        .count  = {40U},
    };
    for (size_t index = 0U; index < 40U; ++index) {
        keyboard.events[index] = (uint8_t) index;
    }
    const tab5_keyboard_interrupt_ops_t ops = operations(&keyboard);
    bool pending                            = false;
    assert(tab5_keyboard_interrupt_drain(&ops, 1U, &pending));
    assert(!pending);
    assert(keyboard.event_index == 32U);
    assert(keyboard.submitted_count == 32U);
}

static void test_io_failure(void)
{
    fake_keyboard_t keyboard                = {.fail_read = true};
    const tab5_keyboard_interrupt_ops_t ops = operations(&keyboard);
    bool pending                            = false;
    assert(!tab5_keyboard_interrupt_drain(&ops, 1U, &pending));
}

int main(void)
{
    test_single_interrupt();
    test_reasserted_interrupt_is_drained();
    test_bounded_drain_reschedules();
    test_event_count_is_bounded();
    test_io_failure();
    return 0;
}
