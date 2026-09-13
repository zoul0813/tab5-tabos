#include "wifi_startup.h"

bool tab5_wifi_startup_prepare(tab5_wifi_startup_t* startup)
{
    if (startup == NULL || startup->done != NULL) {
        return false;
    }
    startup->done = xSemaphoreCreateBinary();
    if (startup->done == NULL) {
        return false;
    }
    atomic_store_explicit(&startup->cancel_requested, false, memory_order_release);
    atomic_store_explicit(&startup->running, true, memory_order_release);
    return true;
}

bool tab5_wifi_startup_running(const tab5_wifi_startup_t* startup)
{
    return startup != NULL && atomic_load_explicit(&startup->running, memory_order_acquire);
}

bool tab5_wifi_startup_cancelled(const tab5_wifi_startup_t* startup)
{
    return startup != NULL && atomic_load_explicit(&startup->cancel_requested, memory_order_acquire);
}

void tab5_wifi_startup_finish(tab5_wifi_startup_t* startup)
{
    atomic_store_explicit(&startup->running, false, memory_order_release);
    xSemaphoreGive(startup->done);
}

void tab5_wifi_startup_stop_and_join(tab5_wifi_startup_t* startup)
{
    if (startup == NULL || startup->done == NULL) {
        return;
    }
    atomic_store_explicit(&startup->cancel_requested, true, memory_order_release);
    (void) xSemaphoreTake(startup->done, portMAX_DELAY);
    vSemaphoreDelete(startup->done);
    startup->done = NULL;
    atomic_store_explicit(&startup->running, false, memory_order_release);
}
