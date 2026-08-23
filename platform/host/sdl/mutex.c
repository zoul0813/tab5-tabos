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
