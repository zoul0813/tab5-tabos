#ifndef TABOS_PLATFORM_APPLICATION_ADMISSION_H
#define TABOS_PLATFORM_APPLICATION_ADMISSION_H

#include <stdbool.h>
#include <stdatomic.h>

/* One atomic word linearizes freeze against operation entry. No lock is held
 * across a gate, driver call, or a parked task. The execution owner alone may
 * acknowledge a safe point; a zero operation count is not that acknowledgement. */
enum {
    APPLICATION_ADMISSION_FROZEN     = 1U << 30U,
    APPLICATION_ADMISSION_PARKED     = 1U << 29U,
    APPLICATION_ADMISSION_COUNT_MASK = APPLICATION_ADMISSION_PARKED - 1U
};

typedef struct {
        atomic_uint state;
} platform_application_admission_t;

static inline bool platform_application_admit(platform_application_admission_t* admission)
{
    unsigned int state = atomic_load(&admission->state);
    do {
        if ((state & APPLICATION_ADMISSION_FROZEN) != 0U ||
            (state & APPLICATION_ADMISSION_COUNT_MASK) == APPLICATION_ADMISSION_COUNT_MASK) {
            return false;
        }
    } while (!atomic_compare_exchange_weak(&admission->state, &state, state + 1U));
    return true;
}

static inline void platform_application_release(platform_application_admission_t* admission)
{
    atomic_fetch_sub(&admission->state, 1U);
}

static inline void platform_application_freeze(platform_application_admission_t* admission, bool frozen)
{
    if (frozen) {
        atomic_fetch_or(&admission->state, APPLICATION_ADMISSION_FROZEN);
    } else {
        atomic_fetch_and(&admission->state, APPLICATION_ADMISSION_COUNT_MASK);
    }
}

static inline bool platform_application_acknowledge(platform_application_admission_t* admission)
{
    unsigned int expected = APPLICATION_ADMISSION_FROZEN;
    return atomic_compare_exchange_strong(&admission->state, &expected,
                                          APPLICATION_ADMISSION_FROZEN | APPLICATION_ADMISSION_PARKED) ||
           expected == (APPLICATION_ADMISSION_FROZEN | APPLICATION_ADMISSION_PARKED);
}

static inline bool platform_application_parked(const platform_application_admission_t* admission)
{
    return atomic_load(&admission->state) == (APPLICATION_ADMISSION_FROZEN | APPLICATION_ADMISSION_PARKED);
}

#endif
