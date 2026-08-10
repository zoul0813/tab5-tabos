#include <tabos/time.h>

#include "platform_test.h"

#include <assert.h>

int main(void)
{
    tabos_timer_t timer = {0};

    tab_test_platform_set_time_ms(100U);
    assert(tabos_time_monotonic_ms() == 100U);
    assert(!tabos_timer_is_active(&timer));

    tabos_timer_start(&timer, 50U, 0U);
    assert(tabos_timer_is_active(&timer));
    tab_test_platform_advance_time_ms(49U);
    assert(!tabos_timer_poll(&timer));
    tab_test_platform_advance_time_ms(1U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_is_active(&timer));
    assert(!tabos_timer_poll(&timer));

    tabos_timer_start(&timer, 10U, 20U);
    tab_test_platform_advance_time_ms(10U);
    assert(tabos_timer_poll(&timer));
    assert(tabos_timer_is_active(&timer));
    tab_test_platform_advance_time_ms(99U);
    assert(tabos_timer_poll(&timer));
    assert(!tabos_timer_poll(&timer));
    tab_test_platform_advance_time_ms(1U);
    assert(tabos_timer_poll(&timer));

    tabos_timer_cancel(&timer);
    assert(!tabos_timer_is_active(&timer));
    return 0;
}
