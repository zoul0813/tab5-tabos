#ifndef TABOS_INTERNAL_TIME_H
#define TABOS_INTERNAL_TIME_H

#include <stdint.h>

#include <tabos/time.h>

uint64_t time_timer_deadline(const tabos_timer_t* timer);

#endif
