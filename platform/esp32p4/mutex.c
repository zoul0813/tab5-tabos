#include <tabos/platform/platform.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stdlib.h>

struct platform_mutex {
        SemaphoreHandle_t native;
};

platform_mutex_t* platform_mutex_create(void)
{
    platform_mutex_t* mutex = calloc(1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    mutex->native = xSemaphoreCreateMutex();
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
    vSemaphoreDelete(mutex->native);
    free(mutex);
}

void platform_mutex_lock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        (void) xSemaphoreTake(mutex->native, portMAX_DELAY);
    }
}

void platform_mutex_unlock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        (void) xSemaphoreGive(mutex->native);
    }
}
