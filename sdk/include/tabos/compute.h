#ifndef TABOS_COMPUTE_H
#define TABOS_COMPUTE_H
#include <stdint.h>

typedef void (*tabos_compute_fn)(void* data);

/* Experimental: one outstanding bounded pure-compute job per application.
 * Results are zero or negative errno; ENOTSUP permits synchronous fallback.
 * Data/bytes must describe storage within the application's committed heap.
 * Callback and all referenced storage must remain valid until wait returns.
 * Use only private/preallocated storage and stateless C operations: no
 * allocation, stdio, SDK calls, locks or shared mutable application state.
 * SDK gate attempts abort the job with -EPERM. Wait acquires callback writes.
 * Teardown stops the worker before releasing memory. Native stack: 16 KiB.
 * Host RV32 returns -ENOTSUP. This contract does not provide memory isolation. */
int tabos_compute_submit(tabos_compute_fn callback, void* data, uint32_t bytes);
/* 1 when idle/complete, 0 while running, or negative errno. Poll does not
 * consume completion: call wait before reusing results or submitting again. */
int tabos_compute_poll(void);
int tabos_compute_wait(void);
#endif
