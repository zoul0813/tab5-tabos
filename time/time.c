#include <tabos/time.h>

#include <tabos/internal/time.h>

#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdint.h>

uint64_t tabos_time_monotonic_ms(void)
{
    return platform_time_ms();
}

uint64_t time_deadline_after(uint64_t now_ms, uint64_t delay_ms)
{
    if (now_ms >= TIME_DEADLINE_LATEST || delay_ms > TIME_DEADLINE_LATEST - now_ms) {
        return TIME_DEADLINE_LATEST;
    }
    return now_ms + delay_ms;
}

uint64_t time_deadline_advance(uint64_t deadline_ms, uint64_t interval_ms, uint64_t now_ms)
{
    if (deadline_ms == TIME_DEADLINE_NONE || interval_ms == 0U || now_ms >= TIME_DEADLINE_LATEST) {
        return TIME_DEADLINE_NONE;
    }
    if (now_ms < deadline_ms) {
        return deadline_ms;
    }

    const uint64_t elapsed_ms = now_ms - deadline_ms;
    const uint64_t delay_ms   = interval_ms - (elapsed_ms % interval_ms);
    return time_deadline_after(now_ms, delay_ms);
}

void tabos_timer_start(tabos_timer_t* timer, uint64_t delay_ms, uint64_t interval_ms)
{
    if (timer == NULL) {
        return;
    }
    const uint64_t now = tabos_time_monotonic_ms();
    *timer             = (tabos_timer_t) {
                    .deadline_ms = time_deadline_after(now, delay_ms),
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

uint64_t time_timer_deadline(const tabos_timer_t* timer)
{
    return tabos_timer_is_active(timer) ? timer->deadline_ms : TIME_DEADLINE_NONE;
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
    if (timer->interval_ms == 0U) {
        timer->active = false;
        return true;
    }

    timer->deadline_ms = time_deadline_advance(timer->deadline_ms, timer->interval_ms, now);
    if (timer->deadline_ms == TIME_DEADLINE_NONE) {
        timer->active = false;
    }
    return true;
}
