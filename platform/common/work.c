#include <tabos/platform/platform.h>

#include <stdatomic.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <SDL3/SDL_thread.h>
#endif

struct platform_work {
        void (*callback)(void*);
        void* argument;
        platform_signal_t* completion;
        platform_mutex_t* guard;
        atomic_uint references;
        atomic_bool complete;
};

void platform_work_release(platform_work_t* work)
{
    if (work != NULL && atomic_fetch_sub(&work->references, 1U) == 1U) {
        platform_signal_destroy(work->completion);
        platform_mutex_destroy(work->guard);
        free(work);
    }
}

#ifdef ESP_PLATFORM
static void run_work(void* argument)
#else
static int run_work(void* argument)
#endif
{
    platform_work_t* work = argument;
    work->callback(work->argument);
    platform_mutex_lock(work->guard);
    atomic_store(&work->complete, true);
    platform_signal_notify(work->completion);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
    platform_mutex_unlock(work->guard);
    platform_work_release(work);
#ifdef ESP_PLATFORM
    vTaskDelete(NULL);
#else
    return 0;
#endif
}

platform_work_t* platform_work_start(void (*callback)(void*), void* argument)
{
    if (callback == NULL) {
        return NULL;
    }
    platform_work_t* work = calloc(1U, sizeof(*work));
    if (work == NULL) {
        return NULL;
    }
    work->completion = platform_signal_create();
    work->guard      = platform_mutex_create();
    if (work->completion == NULL || work->guard == NULL) {
        platform_signal_destroy(work->completion);
        platform_mutex_destroy(work->guard);
        free(work);
        return NULL;
    }
    work->callback = callback;
    work->argument = argument;
    atomic_init(&work->references, 2U);
#ifdef ESP_PLATFORM
    const bool started = xTaskCreate(run_work, "tabos-sync", 4096U, work, 4U, NULL) == pdPASS;
#else
    SDL_Thread* thread = SDL_CreateThread(run_work, "tabos-sync", work);
    const bool started = thread != NULL;
    if (started) {
        SDL_DetachThread(thread);
    }
#endif
    if (!started) {
        platform_work_release(work);
        platform_work_release(work);
        return NULL;
    }
    return work;
}

bool platform_work_complete(const platform_work_t* work)
{
    if (work == NULL) {
        return false;
    }
    platform_mutex_lock(work->guard);
    const bool complete = atomic_load(&work->complete);
    platform_mutex_unlock(work->guard);
    return complete;
}

void platform_work_wait(platform_work_t* work)
{
    while (work != NULL && !platform_work_complete(work)) {
        platform_signal_wait(work->completion, UINT32_MAX);
    }
}
