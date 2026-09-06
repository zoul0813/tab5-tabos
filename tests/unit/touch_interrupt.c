#include "touch_interrupt.h"

#include <assert.h>
#include <stddef.h>

typedef enum {
    FAKE_GT911,
    FAKE_ST712X,
} fake_controller_kind_t;

typedef struct {
        tab5_touch_point_t points[TABOS_POINTER_MAX_CONTACTS];
        uint8_t count;
} fake_report_t;

typedef struct {
        tabos_pointer_event_type_t type;
        uint32_t contact;
        uint16_t x;
        uint16_t y;
} fake_event_t;

typedef struct {
        fake_controller_kind_t kind;
        fake_report_t reports[8];
        bool asserted[8];
        fake_event_t events[32];
        size_t report_index;
        size_t asserted_index;
        size_t event_count;
        size_t fail_report;
} fake_touch_t;

static bool read_points(void* context, tab5_touch_point_t* points, uint8_t* point_count, uint8_t maximum_points)
{
    fake_touch_t* touch = context;
    if (touch->report_index == touch->fail_report) {
        return false;
    }
    const fake_report_t* report = &touch->reports[touch->report_index++];
    assert(report->count <= maximum_points);
    for (uint8_t point = 0U; point < report->count; ++point) {
        points[point] = report->points[point];
    }
    *point_count = report->count;
    return true;
}

static bool interrupt_asserted(void* context)
{
    fake_touch_t* touch = context;
    return touch->asserted[touch->asserted_index++];
}

static void submit_contact(void* context, tabos_pointer_event_type_t type, uint32_t contact, uint16_t x, uint16_t y)
{
    fake_touch_t* touch                 = context;
    touch->events[touch->event_count++] = (fake_event_t) {
        .type    = type,
        .contact = contact,
        .x       = x,
        .y       = y,
    };
}

static tab5_touch_interrupt_ops_t operations(fake_touch_t* touch)
{
    return (tab5_touch_interrupt_ops_t) {
        .context            = touch,
        .read_points        = read_points,
        .interrupt_asserted = interrupt_asserted,
        .submit_contact     = submit_contact,
    };
}

static void drain_once(tab5_touch_interrupt_state_t* state, fake_touch_t* touch)
{
    const tab5_touch_interrupt_ops_t ops = operations(touch);
    bool pending                         = true;
    assert(tab5_touch_interrupt_drain(state, &ops, 4U, &pending));
    assert(!pending);
}

static void test_controller_contract(fake_controller_kind_t kind)
{
    fake_touch_t touch = {
        .kind = kind,
        .reports =
            {
                      {.points = {{.x = 100U, .y = 200U}, {.x = 500U, .y = 600U}}, .count = 2U},
                      {.points = {{.x = 100U, .y = 200U}, {.x = 500U, .y = 600U}}, .count = 2U},
                      {.points = {{.x = 503U, .y = 604U}, {.x = 104U, .y = 202U}}, .count = 2U},
                      {.count = 0U},
                      {.points = {{.x = 900U, .y = 700U}}, .count = 1U},
                      },
        .fail_report = SIZE_MAX,
    };
    tab5_touch_interrupt_state_t state;
    tab5_touch_interrupt_state_init(&state);

    drain_once(&state, &touch);
    assert(touch.report_index == 1U);
    assert(touch.event_count == 2U);
    assert(touch.events[0].type == TABOS_POINTER_DOWN && touch.events[0].contact == 0U);
    assert(touch.events[1].type == TABOS_POINTER_DOWN && touch.events[1].contact == 1U);

    drain_once(&state, &touch);
    assert(touch.event_count == 2U);

    drain_once(&state, &touch);
    assert(touch.events[2].type == TABOS_POINTER_MOVE && touch.events[2].contact == 1U);
    assert(touch.events[3].type == TABOS_POINTER_MOVE && touch.events[3].contact == 0U);

    drain_once(&state, &touch);
    assert(touch.events[4].type == TABOS_POINTER_UP && touch.events[4].contact == 0U);
    assert(touch.events[5].type == TABOS_POINTER_UP && touch.events[5].contact == 1U);

    drain_once(&state, &touch);
    assert(touch.events[6].type == TABOS_POINTER_DOWN && touch.events[6].contact == 0U);
    const tab5_touch_interrupt_ops_t ops = operations(&touch);
    tab5_touch_interrupt_cancel(&state, &ops);
    assert(touch.events[7].type == TABOS_POINTER_CANCEL && touch.events[7].contact == 0U);
}

static void test_arrival_during_drain(void)
{
    fake_touch_t touch = {
        .reports =
            {
                      {.points = {{.x = 20U, .y = 30U}}, .count = 1U},
                      {.count = 0U},
                      },
        .asserted    = {true, false},
        .fail_report = SIZE_MAX,
    };
    tab5_touch_interrupt_state_t state;
    tab5_touch_interrupt_state_init(&state);
    const tab5_touch_interrupt_ops_t ops = operations(&touch);
    bool pending                         = true;
    assert(tab5_touch_interrupt_drain(&state, &ops, 4U, &pending));
    assert(!pending);
    assert(touch.report_index == 2U);
    assert(touch.events[0].type == TABOS_POINTER_DOWN);
    assert(touch.events[1].type == TABOS_POINTER_UP);
}

static void test_bounded_drain_reschedules(void)
{
    fake_touch_t touch = {
        .reports     = {{.count = 0U}},
        .asserted    = {true},
        .fail_report = SIZE_MAX,
    };
    tab5_touch_interrupt_state_t state;
    tab5_touch_interrupt_state_init(&state);
    const tab5_touch_interrupt_ops_t ops = operations(&touch);
    bool pending                         = false;
    assert(tab5_touch_interrupt_drain(&state, &ops, 1U, &pending));
    assert(pending);
}

static void test_fault_cancels_contacts(void)
{
    fake_touch_t touch = {
        .reports =
            {
                      {.points = {{.x = 40U, .y = 50U}}, .count = 1U},
                      },
        .fail_report = 1U,
    };
    tab5_touch_interrupt_state_t state;
    tab5_touch_interrupt_state_init(&state);
    drain_once(&state, &touch);
    const tab5_touch_interrupt_ops_t ops = operations(&touch);
    bool pending                         = false;
    assert(!tab5_touch_interrupt_drain(&state, &ops, 1U, &pending));
    assert(touch.events[1].type == TABOS_POINTER_CANCEL && touch.events[1].contact == 0U);
}

int main(void)
{
    test_controller_contract(FAKE_GT911);
    test_controller_contract(FAKE_ST712X);
    test_arrival_during_drain();
    test_bounded_drain_reschedules();
    test_fault_cancels_contacts();
    return 0;
}
