#include <tabos/time.h>

#include <tabos/internal/time.h>

#include "platform_test.h"

#include <assert.h>

int main(void)
{
    tabos_timer_t timer = {0};

    test_platform_set_time_ms(100U);
    assert(tabos_time_monotonic_ms() == 100U);
    assert(!tabos_timer_is_active(&timer));
    assert(time_timer_deadline(&timer) == UINT64_MAX);

    tabos_timer_start(&timer, 50U, 0U);
    assert(tabos_timer_is_active(&timer));
    assert(time_timer_deadline(&timer) == 150U);
    test_platform_advance_time_ms(49U);
    assert(!tabos_timer_poll(&timer));
    test_platform_advance_time_ms(1U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_is_active(&timer));
    assert(!tabos_timer_poll(&timer));

    tabos_timer_start(&timer, 10U, 20U);
    assert(time_timer_deadline(&timer) == 160U);
    test_platform_advance_time_ms(10U);
    assert(tabos_timer_poll(&timer));
    assert(tabos_timer_is_active(&timer));
    assert(time_timer_deadline(&timer) == 180U);
    test_platform_advance_time_ms(99U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_poll(&timer));
    assert(time_timer_deadline(&timer) == 260U);
    test_platform_advance_time_ms(1U);
    assert(tabos_timer_poll(&timer));
    assert(time_timer_deadline(&timer) == 280U);

    tabos_timer_cancel(&timer);
    assert(!tabos_timer_is_active(&timer));
    assert(time_timer_deadline(&timer) == UINT64_MAX);

    test_platform_set_time_ms(TIME_DEADLINE_LATEST - 5U);
    tabos_timer_start(&timer, 10U, 0U);
    assert(time_timer_deadline(&timer) == TIME_DEADLINE_LATEST);
    test_platform_advance_time_ms(4U);
    assert(!tabos_timer_poll(&timer));
    test_platform_advance_time_ms(1U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_is_active(&timer));

    test_platform_set_time_ms(TIME_DEADLINE_LATEST - 5U);
    tabos_timer_start(&timer, 5U, 20U);
    test_platform_advance_time_ms(5U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_is_active(&timer));
    assert(time_deadline_advance(TIME_DEADLINE_NONE, 10U, 20U) == TIME_DEADLINE_NONE);
    return 0;
}
