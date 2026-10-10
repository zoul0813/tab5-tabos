#include "display_completion.h"

#include <tabos/config/identity.h>
#include <esp_log.h>

#include <stdlib.h>

enum {
    DISPLAY_COMPLETION_TIMEOUT_MS = 2000U,
};

bool tab5_display_wait_completion(SemaphoreHandle_t completion, const char* operation)
{
    if (xSemaphoreTake(completion, pdMS_TO_TICKS(DISPLAY_COMPLETION_TIMEOUT_MS)) == pdTRUE) {
        return true;
    }
    ESP_LOGE(TABOS_PLATFORM_LOG_TAG, "%s completion timed out; DMA ownership is uncertain", operation);
    // The pinned drivers cannot cancel an accepted transaction. Never return to
    // software fallback, guest code, or teardown while DMA can access its buffers.
    abort();
}

bool tab5_display_scanout_finish(tab5_display_scanout_t* scanout, SemaphoreHandle_t completion)
{
    if (!scanout->pending) {
        return true;
    }
    if (!tab5_display_wait_completion(completion, "VSYNC")) {
        return false;
    }
    uint16_t* presented = scanout->back;
    scanout->back       = scanout->front;
    scanout->front      = presented;
    scanout->pending    = false;
    return true;
}

bool tab5_display_work_finish(tab5_display_work_t* work, SemaphoreHandle_t completion)
{
    if (!work->pending) {
        return true;
    }
    (void) tab5_display_wait_completion(completion, "display worker");
    work->pending = false;
    return work->succeeded;
}
