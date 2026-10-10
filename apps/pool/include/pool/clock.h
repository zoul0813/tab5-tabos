#ifndef POOL_CLOCK_H
#define POOL_CLOCK_H

#include <stdint.h>

typedef struct {
        uint32_t accumulator;  /* milliseconds * 60; always less than 1000. */
        uint64_t discarded_ms; /* Saturating diagnostic count of overload time. */
} pool_clock_t;

/* Caller clears accumulator on idle, pause, reset and shot transitions. */
unsigned int pool_clock_advance(pool_clock_t* clock, uint64_t elapsed_ms);
uint32_t pool_clock_wait(const pool_clock_t* clock, uint64_t present_ms);

#endif
