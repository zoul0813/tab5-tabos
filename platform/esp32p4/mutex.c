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

struct platform_signal {
        SemaphoreHandle_t native;
};
platform_signal_t* platform_signal_create(void)
{
    platform_signal_t* signal = calloc(1U, sizeof(*signal));
    if (signal != NULL) {
        signal->native = xSemaphoreCreateBinary();
        if (signal->native == NULL) {
            free(signal);
            return NULL;
        }
    }
    return signal;
}
void platform_signal_destroy(platform_signal_t* signal)
{
    if (signal != NULL) {
        vSemaphoreDelete(signal->native);
        free(signal);
    }
}
void platform_signal_notify(platform_signal_t* signal)
{
    if (signal != NULL) {
        (void) xSemaphoreGive(signal->native);
    }
}
void platform_signal_wait(platform_signal_t* signal, uint32_t timeout_ms)
{
    if (signal != NULL) {
        TickType_t ticks = portMAX_DELAY;
        if (timeout_ms != UINT32_MAX) {
            ticks = pdMS_TO_TICKS(timeout_ms);
            if (ticks == 0U && timeout_ms != 0U) {
                ticks = 1U;
            }
        }
        (void) xSemaphoreTake(signal->native, ticks);
    }
}
