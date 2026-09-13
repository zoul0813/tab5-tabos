#include "power_test.h"
#include "internal.h"
#include <tabos/filesystem.h>
#include <stdatomic.h>
static atomic_uint_fast64_t offset_ms;
static atomic_uint wakes;
static atomic_int failure;
static atomic_uchar brightness;
static atomic_bool panel_enabled;
static bool display_suspended;
static int display_suspend_result;
static uint8_t saved_brightness;
static bool saved_panel;
static bool take(host_power_failure_t value)
{
    int expected = (int) value;
    return atomic_compare_exchange_strong(&failure, &expected, HOST_POWER_FAIL_NONE);
}
void host_power_test_reset(void)
{
    atomic_store(&offset_ms, 0U);
    atomic_store(&wakes, 0U);
    atomic_store(&failure, HOST_POWER_FAIL_NONE);
    host_power_display_reset();
}

void host_power_display_reset(void)
{
    atomic_store(&brightness, 100U);
    atomic_store(&panel_enabled, true);
    display_suspended      = false;
    display_suspend_result = 0;
}
void host_power_test_advance_time(uint64_t milliseconds)
{
    uint64_t current = atomic_load(&offset_ms);
    atomic_store(&offset_ms, UINT64_MAX - current < milliseconds ? UINT64_MAX : current + milliseconds);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
}
void host_power_test_activity(void (*inject)(void* context, uint64_t now_ms), void* context)
{
    if (inject != NULL) {
        inject(context, platform_time_ms());
    }
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
}
void host_power_test_wake(platform_power_wake_cause_t causes)
{
    atomic_fetch_or(&wakes, causes);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
}
void host_power_test_fail(host_power_failure_t value)
{
    atomic_store(&failure, value);
}
uint64_t host_power_time_offset(void)
{
    return atomic_load(&offset_ms);
}
uint8_t host_power_test_brightness(void)
{
    return atomic_load(&brightness);
}
bool platform_power_set_brightness(uint8_t percent)
{
    if (percent > 100U || take(HOST_POWER_FAIL_BRIGHTNESS) || !host_display_set_brightness(percent)) {
        return false;
    }
    atomic_store(&brightness, percent);
    return true;
}
bool platform_power_prepare_sleep(void)
{
    return !take(HOST_POWER_FAIL_PREPARE);
}

int platform_display_power_suspend(void)
{
    if (display_suspended) {
        return display_suspend_result;
    }
    saved_brightness       = atomic_load(&brightness);
    saved_panel            = atomic_load(&panel_enabled);
    display_suspended      = true;
    display_suspend_result = -TABOS_EIO;
    /* Host has no autonomous scanout worker. Retain saved state even on partial
     * failure; caller must run resume before reopening presentation admission. */
    if (!platform_power_set_brightness(0U) || !platform_power_set_panel_enabled(false)) {
        return -TABOS_EIO;
    }
    display_suspend_result = 0;
    return 0;
}

int platform_display_power_resume(void)
{
    if (!display_suspended) {
        return 0;
    }
    if (!platform_power_set_panel_enabled(saved_panel) || !platform_power_set_brightness(saved_brightness)) {
        return -TABOS_EIO;
    }
    display_suspended = false;
    return 0;
}
bool platform_power_set_panel_enabled(bool enabled)
{
    if (take(HOST_POWER_FAIL_PANEL) || !host_display_set_panel_enabled(enabled)) {
        return false;
    }
    atomic_store(&panel_enabled, enabled);
    return true;
}
bool host_power_test_panel_enabled(void)
{
    return atomic_load(&panel_enabled);
}
void platform_power_abort_sleep(void)
{
}
bool platform_power_enter_light_sleep(void)
{
    return !take(HOST_POWER_FAIL_SLEEP);
}
platform_power_wake_cause_t platform_power_collect_wake_causes(void)
{
    return atomic_exchange(&wakes, 0U);
}
bool platform_power_restore(void)
{
    return !take(HOST_POWER_FAIL_RESTORE);
}
