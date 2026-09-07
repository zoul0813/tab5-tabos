#include <tabos/platform/platform.h>

#include <SDL3/SDL_mutex.h>

#include <stdlib.h>

struct platform_mutex {
        SDL_Mutex* native;
};

platform_mutex_t* platform_mutex_create(void)
{
    platform_mutex_t* mutex = calloc(1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    mutex->native = SDL_CreateMutex();
    if (mutex->native == NULL) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

void platform_mutex_destroy(platform_mutex_t* mutex)
{
    if (mutex == NULL) {
        return;
    }
    SDL_DestroyMutex(mutex->native);
    free(mutex);
}

void platform_mutex_lock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        SDL_LockMutex(mutex->native);
    }
}

void platform_mutex_unlock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        SDL_UnlockMutex(mutex->native);
    }
}

struct platform_signal {
        SDL_Mutex* mutex;
        SDL_Condition* condition;
        bool pending;
};
platform_signal_t* platform_signal_create(void)
{
    platform_signal_t* signal = calloc(1U, sizeof(*signal));
    if (signal == NULL) {
        return NULL;
    }
    signal->mutex     = SDL_CreateMutex();
    signal->condition = SDL_CreateCondition();
    if (signal->mutex == NULL || signal->condition == NULL) {
        platform_signal_destroy(signal);
        return NULL;
    }
    return signal;
}
void platform_signal_destroy(platform_signal_t* signal)
{
    if (signal != NULL) {
        SDL_DestroyCondition(signal->condition);
        SDL_DestroyMutex(signal->mutex);
        free(signal);
    }
}
void platform_signal_notify(platform_signal_t* signal)
{
    if (signal != NULL) {
        SDL_LockMutex(signal->mutex);
        signal->pending = true;
        SDL_SignalCondition(signal->condition);
        SDL_UnlockMutex(signal->mutex);
    }
}
void platform_signal_wait(platform_signal_t* signal, uint32_t timeout_ms)
{
    if (signal != NULL) {
        SDL_LockMutex(signal->mutex);
        if (!signal->pending) {
            const Sint32 timeout = timeout_ms > INT32_MAX ? INT32_MAX : (Sint32) timeout_ms;
            (void) SDL_WaitConditionTimeout(signal->condition, signal->mutex, timeout_ms == UINT32_MAX ? -1 : timeout);
        }
        signal->pending = false;
        SDL_UnlockMutex(signal->mutex);
    }
}
