#include <tabos/time.h>

#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdint.h>

uint64_t tabos_time_monotonic_ms(void)
{
    return platform_time_ms();
}

void tabos_timer_start(tabos_timer_t* timer, uint64_t delay_ms, uint64_t interval_ms)
{
    if (timer == NULL) {
        return;
    }
    const uint64_t now = tabos_time_monotonic_ms();
    *timer             = (tabos_timer_t) {
                    .deadline_ms = UINT64_MAX - now < delay_ms ? UINT64_MAX : now + delay_ms,
                    .interval_ms = interval_ms,
                    .active      = true,
    };
}

void tabos_timer_cancel(tabos_timer_t* timer)
{
    if (timer != NULL) {
        *timer = (tabos_timer_t) {0};
    }
}

bool tabos_timer_is_active(const tabos_timer_t* timer)
{
    return timer != NULL && timer->active;
}

bool tabos_timer_poll(tabos_timer_t* timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    const uint64_t now = tabos_time_monotonic_ms();
    if (now < timer->deadline_ms) {
        return false;
    }
    if (timer->interval_ms == 0U || timer->deadline_ms == UINT64_MAX) {
        timer->active = false;
        return true;
    }

    const uint64_t periods = ((now - timer->deadline_ms) / timer->interval_ms) + 1U;
    if (periods > (UINT64_MAX - timer->deadline_ms) / timer->interval_ms) {
        timer->deadline_ms = UINT64_MAX;
    } else {
        timer->deadline_ms += periods * timer->interval_ms;
    }
    return true;
}
