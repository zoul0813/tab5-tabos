#ifndef TABOS_INTERNAL_SERVICE_ADMISSION_H
#define TABOS_INTERNAL_SERVICE_ADMISSION_H

#include <stdbool.h>
#include <stdatomic.h>

/* One word linearizes freeze against entry and supplies a coherent diagnostic
 * snapshot. Counters include admitted callers waiting for a service mutex. */
enum {
    SERVICE_ADMISSION_MASK     = 0x7fffU,
    SERVICE_ADMISSION_MUTATION = 1U << 15U,
    SERVICE_ADMISSION_FROZEN   = 1U << 30U
};
typedef struct {
        atomic_uint state;
} service_admission_t;

static inline bool service_admission_enter(service_admission_t* admission, bool mutation)
{
    unsigned int state           = atomic_load(&admission->state);
    const unsigned int increment = 1U + (mutation ? SERVICE_ADMISSION_MUTATION : 0U);
    do {
        if ((state & SERVICE_ADMISSION_FROZEN) != 0U || (state & SERVICE_ADMISSION_MASK) == SERVICE_ADMISSION_MASK) {
            return false;
        }
    } while (!atomic_compare_exchange_weak(&admission->state, &state, state + increment));
    return true;
}

static inline void service_admission_leave(service_admission_t* admission, bool mutation)
{
    atomic_fetch_sub(&admission->state, 1U + (mutation ? SERVICE_ADMISSION_MUTATION : 0U));
}

static inline void service_admission_freeze(service_admission_t* admission, bool frozen)
{
    if (frozen) {
        atomic_fetch_or(&admission->state, SERVICE_ADMISSION_FROZEN);
    } else {
        atomic_fetch_and(&admission->state, ~(unsigned int) SERVICE_ADMISSION_FROZEN);
    }
}

#endif
