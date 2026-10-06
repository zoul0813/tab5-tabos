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
