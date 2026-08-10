#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_log.h>

static const char *const TAG = TABOS_PLATFORM_LOG_TAG;

bool tab_platform_init(bool headless)
{
    (void)headless;
    ESP_LOGI(TAG, "ESP32-P4 platform placeholder initialized");
    return true;
}

int tab_platform_run(void)
{
    return 0;
}

void tab_platform_shutdown(void)
{
}

const char *tab_platform_name(void)
{
    return TABOS_TARGET_NAME_TAB5;
}
