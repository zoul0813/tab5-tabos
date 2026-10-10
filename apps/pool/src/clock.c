#include <pool/clock.h>

unsigned int pool_clock_advance(pool_clock_t* clock, uint64_t elapsed_ms)
{
    if (elapsed_ms > 100U) {
        const uint64_t discarded = elapsed_ms - 100U;
        if (discarded > UINT64_MAX - clock->discarded_ms) {
            clock->discarded_ms = UINT64_MAX;
        } else {
            clock->discarded_ms += discarded;
        }
        elapsed_ms = 100U;
    }
    clock->accumulator       += (uint32_t) elapsed_ms * 60U;
    const unsigned int ticks  = clock->accumulator / 1000U;
    clock->accumulator       %= 1000U;
    return ticks;
}

uint32_t pool_clock_wait(const pool_clock_t* clock, uint64_t present_ms)
{
    const uint32_t until = (1000U - clock->accumulator + 59U) / 60U;
    return present_ms >= until ? 0U : until - (uint32_t) present_ms;
}
