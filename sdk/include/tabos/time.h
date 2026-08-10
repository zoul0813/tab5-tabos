#ifndef TABOS_TIME_H
#define TABOS_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t deadline_ms;
    uint64_t interval_ms;
    bool active;
} tabos_timer_t;

uint64_t tabos_time_monotonic_ms(void);
void tabos_timer_start(tabos_timer_t *timer, uint64_t delay_ms, uint64_t interval_ms);
void tabos_timer_cancel(tabos_timer_t *timer);
bool tabos_timer_is_active(const tabos_timer_t *timer);
bool tabos_timer_poll(tabos_timer_t *timer);

#endif
