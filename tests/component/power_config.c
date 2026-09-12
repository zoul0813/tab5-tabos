#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/internal/power_config.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/input.h>
#include <tabos/filesystem.h>
#include "platform_test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool platform_battery_status(platform_battery_status_t* status)
{
    *status = (platform_battery_status_t) {0};
    return false;
}

bool platform_battery_set_charging(bool enabled)
{
    (void) enabled;
    return false;
}

bool platform_battery_set_fast_charging(bool enabled)
{
    (void) enabled;
    return false;
}

static void save_config(const char* text)
{
    const tabos_fd_t fd = tabos_fs_open(POWER_CONFIG_PATH, TABOS_O_CREAT | TABOS_O_WRONLY | TABOS_O_TRUNC, 0644U);
    assert(fd >= 0);
    assert(tabos_fs_write(fd, text, strlen(text)) == (tabos_ssize_t) strlen(text));
    assert(tabos_fs_close(fd) == 0);
}

static void start(void)
{
    assert(kernel_runtime_init());
    assert(kernel_runtime_start(false));
}

int main(void)
{
    char root[] = "/tmp/tabos-power-config.XXXXXX";
    assert(mkdtemp(root) != NULL);
    assert(setenv("TABOS_HOST_ROOTFS", root, 1) == 0);
    start();
    assert(test_platform_brightness() == 75U);
    power_policy_t policy;
    assert(power_config_load(&policy) == POWER_CONFIG_NOT_FOUND);
    assert(tabos_fs_mkdir("T:/etc", 0755U) == 0);
    save_config("version=1\n[display]\ndim_seconds=2\nbacklight_off_seconds=4\npanel_off_seconds=6\n"
                "normal_brightness=55\ndim_brightness=12\n");
    /* Editing the file does not hot-reload or create periodic storage work. */
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_NONE);
    assert(test_platform_brightness() == 75U);
    kernel_runtime_shutdown();
    start();
    assert(test_platform_brightness() == 55U && test_platform_panel_enabled());
    test_platform_advance_time_ms(1999U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(test_platform_brightness() == 55U);
    test_platform_advance_time_ms(1U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(test_platform_brightness() == 12U);
    test_platform_advance_time_ms(2000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(test_platform_brightness() == 0U && test_platform_panel_enabled());
    test_platform_advance_time_ms(2000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(test_platform_brightness() == 0U && !test_platform_panel_enabled());
    const tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
    assert(input_submit(&key));
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT);
    assert(test_platform_brightness() == 55U && test_platform_panel_enabled());
    save_config("version=1\n[display]\nnormal_brightness=0\n");
    kernel_runtime_shutdown();
    start();
    assert(test_platform_brightness() == 75U);
    assert(power_config_load(&policy) == POWER_CONFIG_INVALID); /* Invalid file was not rewritten. */
    save_config("version=1\n[display]\nnormal_brightness=5\ndim_brightness=20\n");
    kernel_runtime_shutdown();
    start();
    assert(test_platform_brightness() == 5U);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(test_platform_brightness() == 5U); /* Dimming must never increase brightness. */
    assert(tabos_fs_unlink(POWER_CONFIG_PATH) == 0);
    assert(tabos_fs_rmdir("T:/etc") == 0);
    kernel_runtime_shutdown();
    char path[256];
    (void) snprintf(path, sizeof(path), "%s/T", root);
    assert(rmdir(path) == 0);
    (void) snprintf(path, sizeof(path), "%s/A", root);
    assert(rmdir(path) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
