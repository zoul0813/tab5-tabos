#include "host_io.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum {
    HOST_IO_JOB_LIMIT = 16
};

struct host_io_job {
        pthread_mutex_t mutex;
        bool done;
        bool abandoned;
        void (*work)(void*);
        void (*dispose)(void*, bool);
        void* data;
};

static _Thread_local host_io_scope_t* current_scope;
static atomic_uint job_count;

static void job_free(host_io_job_t* job, bool delivered)
{
    if (job->dispose != NULL) {
        job->dispose(job->data, delivered);
    }
    free(job->data);
    pthread_mutex_destroy(&job->mutex);
    free(job);
    atomic_fetch_sub_explicit(&job_count, 1U, memory_order_relaxed);
}

static void* job_run(void* argument)
{
    host_io_job_t* job = argument;
    job->work(job->data);
    pthread_mutex_lock(&job->mutex);
    job->done            = true;
    const bool abandoned = job->abandoned;
    pthread_mutex_unlock(&job->mutex);
    if (abandoned) {
        job_free(job, false);
    }
    return NULL;
}

void host_io_enter(host_io_scope_t* scope)
{
    current_scope  = scope;
    scope->pending = false;
}

void host_io_leave(void)
{
    current_scope = NULL;
}

bool host_io_active(void)
{
    return current_scope != NULL;
}

bool host_io_retrying(void)
{
    return current_scope != NULL && current_scope->retrying;
}

void host_io_retry(void)
{
    if (current_scope != NULL) {
        current_scope->pending = true;
    }
}

void host_io_cancel(host_io_scope_t* scope)
{
    host_io_job_t* job = scope->job;
    scope->job         = NULL;
    if (job == NULL) {
        return;
    }
    pthread_mutex_lock(&job->mutex);
    const bool done = job->done;
    job->abandoned  = true;
    pthread_mutex_unlock(&job->mutex);
    if (done) {
        job_free(job, false);
    }
}

int host_io_call(void* data, size_t size, void (*work)(void*), void (*dispose)(void*, bool))
{
    if (current_scope == NULL) {
        work(data);
        return 1;
    }
    host_io_job_t* job = current_scope->job;
    if (job != NULL) {
        pthread_mutex_lock(&job->mutex);
        const bool done = job->done;
        pthread_mutex_unlock(&job->mutex);
        if (!done) {
            host_io_retry();
            return 0;
        }
        memcpy(data, job->data, size);
        current_scope->job = NULL;
        job_free(job, true);
        return 1;
    }
    if (atomic_fetch_add_explicit(&job_count, 1U, memory_order_relaxed) >= HOST_IO_JOB_LIMIT) {
        atomic_fetch_sub_explicit(&job_count, 1U, memory_order_relaxed);
        return -1;
    }
    job = calloc(1U, sizeof(*job));
    if (job == NULL) {
        atomic_fetch_sub_explicit(&job_count, 1U, memory_order_relaxed);
        return -1;
    }
    job->data = malloc(size);
    if (job->data == NULL || pthread_mutex_init(&job->mutex, NULL) != 0) {
        free(job->data);
        free(job);
        atomic_fetch_sub_explicit(&job_count, 1U, memory_order_relaxed);
        return -1;
    }
    memcpy(job->data, data, size);
    job->work    = work;
    job->dispose = dispose;
    pthread_t thread;
    if (pthread_create(&thread, NULL, job_run, job) != 0) {
        job_free(job, false);
        return -1;
    }
    pthread_detach(thread);
    current_scope->job = job;
    host_io_retry();
    return 0;
}
