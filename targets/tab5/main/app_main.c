#include <tabos/internal/runtime.h>
#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_log.h>

static const char *const TAG = TABOS_SYSTEM_LOG_TAG;

void app_main(void)
{
    if (!kernel_runtime_init()) {
        ESP_LOGE(TAG, "%s runtime initialization failed", TABOS_SYSTEM_NAME);
        return;
    }

    if (!platform_init(false)) {
        ESP_LOGE(TAG, "%s platform initialization failed", TABOS_SYSTEM_NAME);
        kernel_runtime_shutdown();
        return;
    }

    if (!kernel_runtime_start()) {
        ESP_LOGE(TAG, "%s display initialization failed", TABOS_SYSTEM_NAME);
        kernel_runtime_shutdown();
        platform_shutdown();
        return;
    }

    ESP_LOGI(
        TAG,
        "%s %s bootstrapped on %s",
        TABOS_SYSTEM_NAME,
        kernel_runtime_version(),
        platform_name()
    );

    (void)platform_run(kernel_runtime_update);
}
