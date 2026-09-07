#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host-only continuation state. Backend workers never borrow guest/service memory. */
typedef struct host_io_job host_io_job_t;
typedef struct {
        host_io_job_t* job;
        bool pending;
        bool retrying;
} host_io_scope_t;

void host_io_enter(host_io_scope_t* scope);
void host_io_leave(void);
bool host_io_active(void);
bool host_io_retrying(void);
void host_io_retry(void);
void host_io_cancel(host_io_scope_t* scope);

/* 1: completed/copied back, 0: suspended, -1: allocation/thread limit failure.
 * dispose runs exactly once on worker-owned data; delivered transfers outputs. */
int host_io_call(void* data, size_t size, void (*work)(void*), void (*dispose)(void*, bool));
