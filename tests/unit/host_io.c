#include "../../platform/posix/host_io.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static bool release_workers;
static unsigned int finished;
static unsigned int discarded;

typedef struct {
        char input[16];
        int result;
} request_t;

static void work(void* data)
{
    request_t* request = data;
    pthread_mutex_lock(&mutex);
    while (!release_workers) {
        pthread_cond_wait(&changed, &mutex);
    }
    request->result = strcmp(request->input, "copied input") == 0 ? 42 : -1;
    ++finished;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
}

static void dispose(void* data, bool delivered)
{
    const request_t* request = data;
    assert(request->result == 42);
    pthread_mutex_lock(&mutex);
    if (!delivered) {
        ++discarded;
    }
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
}

int main(void)
{
    host_io_scope_t scopes[17] = {0};
    request_t request          = {.input = "copied input"};
    for (unsigned int index = 0U; index < 16U; ++index) {
        host_io_enter(&scopes[index]);
        assert(host_io_call(&request, sizeof(request), work, dispose) == 0);
        assert(scopes[index].pending);
        host_io_leave();
        host_io_cancel(&scopes[index]);
        host_io_cancel(&scopes[index]); /* Cancellation is idempotent. */
    }
    host_io_enter(&scopes[16]);
    assert(host_io_call(&request, sizeof(request), work, dispose) == -1);
    assert(!scopes[16].pending); /* Exhaustion returns an error, never blocks. */
    host_io_leave();
    memset(&request, 0, sizeof(request)); /* No worker borrows caller memory. */
    pthread_mutex_lock(&mutex);
    release_workers = true;
    pthread_cond_broadcast(&changed);
    while (finished != 16U || discarded != 16U) {
        pthread_cond_wait(&changed, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    request = (request_t) {.input = "copied input"};
    host_io_enter(&scopes[0]);
    int result = host_io_call(&request, sizeof(request), work, dispose);
    assert(result == 0);
    host_io_leave();
    while (result == 0) {
        const struct timespec delay = {.tv_nsec = 1000000};
        nanosleep(&delay, NULL);
        host_io_enter(&scopes[0]);
        result = host_io_call(&request, sizeof(request), work, dispose);
        host_io_leave();
    }
    assert(result == 1 && request.result == 42 && scopes[0].job == NULL);
    assert(discarded == 16U);
    return 0;
}
