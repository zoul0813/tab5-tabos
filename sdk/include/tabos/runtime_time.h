#ifndef TABOS_RUNTIME_TIME_H
#define TABOS_RUNTIME_TIME_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
        uint64_t deadline_ms;
        uint32_t interval_ms;
        bool active;
} tabos_runtime_timer_t;

uint64_t tabos_monotonic_ms(void);
int tabos_sleep_ms(uint32_t milliseconds);
void tabos_runtime_timer_start(tabos_runtime_timer_t* timer, uint32_t delay_ms, uint32_t interval_ms);
void tabos_runtime_timer_cancel(tabos_runtime_timer_t* timer);
bool tabos_runtime_timer_poll(tabos_runtime_timer_t* timer);

#endif
