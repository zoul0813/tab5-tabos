#ifndef TABOS_INTERNAL_WALL_CLOCK_H
#define TABOS_INTERNAL_WALL_CLOCK_H

#include <tabos/clock.h>

#include <stdbool.h>
#include <stdint.h>

bool wall_clock_datetime_valid(const tabos_datetime_t* datetime);
bool wall_clock_datetime_to_epoch(const tabos_datetime_t* datetime, int64_t* seconds);
bool wall_clock_epoch_to_datetime(int64_t seconds, tabos_datetime_t* datetime);

#endif
