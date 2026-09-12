#ifndef TABOS_NATIVE_CANCEL_H
#define TABOS_NATIVE_CANCEL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

/* Runtime issues owner cancellation; each serialized worker publishes its caller.
 * An old cancellation must never cancel the next process using that worker. */
typedef struct {
        atomic_uintptr_t owner;
        atomic_uintptr_t cancelled_owner;
} native_cancel_t;

static inline void native_cancel_begin(native_cancel_t* state, const void* owner)
{
    atomic_store_explicit(&state->cancelled_owner, 0U, memory_order_release);
    atomic_store_explicit(&state->owner, (uintptr_t) owner, memory_order_release);
}

static inline void native_cancel_request(native_cancel_t* state, const void* owner)
{
    const uintptr_t target = owner != NULL ? (uintptr_t) owner : UINTPTR_MAX;
    atomic_store_explicit(&state->cancelled_owner, target, memory_order_release);
}

static inline bool native_cancel_pending(const native_cancel_t* state)
{
    const uintptr_t target = atomic_load_explicit(&state->cancelled_owner, memory_order_acquire);
    return target == UINTPTR_MAX ||
           (target != 0U && target == atomic_load_explicit(&state->owner, memory_order_acquire));
}

#endif
