#ifndef TABOS_POSIX_POWER_ADMISSION_H
#define TABOS_POSIX_POWER_ADMISSION_H

#include <stdbool.h>
#include <stdatomic.h>

/* Backend-local admission. An acquisition retains its count until resource close.
 * Freeze only succeeds at zero; it never waits or cancels an existing owner. */
enum {
    POSIX_POWER_FROZEN = 1U << 30U
};
static inline bool posix_power_acquire(atomic_uint* state)
{
    unsigned int value = atomic_load(state);
    do {
        if (value >= POSIX_POWER_FROZEN - 1U) {
            return false;
        }
    } while (!atomic_compare_exchange_weak(state, &value, value + 1U));
    return true;
}
static inline void posix_power_release(atomic_uint* state)
{
    atomic_fetch_sub(state, 1U);
}
static inline bool posix_power_suspend(atomic_uint* state)
{
    unsigned int expected = 0U;
    return atomic_compare_exchange_strong(state, &expected, POSIX_POWER_FROZEN) || expected == POSIX_POWER_FROZEN;
}
static inline void posix_power_resume(atomic_uint* state)
{
    atomic_fetch_and(state, ~(unsigned int) POSIX_POWER_FROZEN);
}
#endif
