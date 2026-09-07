#include "power_test.h"
#include <stdatomic.h>
static atomic_uint_fast64_t offset_ms;
static atomic_uint wakes;
static atomic_int failure;
static atomic_uchar brightness;
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
    atomic_store(&brightness, 100U);
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
    if (percent > 100U || take(HOST_POWER_FAIL_BRIGHTNESS)) {
        return false;
    }
    atomic_store(&brightness, percent);
    return true;
}
bool platform_power_prepare_sleep(void)
{
    return !take(HOST_POWER_FAIL_PREPARE);
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
