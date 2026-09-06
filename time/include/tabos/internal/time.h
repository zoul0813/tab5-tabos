#ifndef TABOS_INTERNAL_TIME_H
#define TABOS_INTERNAL_TIME_H

#include <stdint.h>

#include <tabos/time.h>

#define TIME_DEADLINE_NONE   UINT64_MAX
#define TIME_DEADLINE_LATEST (UINT64_MAX - 1U)

uint64_t time_deadline_after(uint64_t now_ms, uint64_t delay_ms);
uint64_t time_deadline_advance(uint64_t deadline_ms, uint64_t interval_ms, uint64_t now_ms);
uint64_t time_timer_deadline(const tabos_timer_t* timer);

#endif
