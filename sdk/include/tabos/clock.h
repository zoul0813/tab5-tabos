#ifndef TABOS_CLOCK_H
#define TABOS_CLOCK_H

#include <stdint.h>

typedef struct {
        int32_t year;
        uint8_t month;
        uint8_t day;
        uint8_t weekday;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
} tabos_datetime_t;

int tabos_clock_get(tabos_datetime_t* datetime);
int tabos_clock_set(const tabos_datetime_t* datetime);
int tabos_clock_get_epoch(int64_t* seconds);
int tabos_clock_set_epoch(int64_t seconds);

#endif
