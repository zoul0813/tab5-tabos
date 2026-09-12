#include "power_test.h"

#include <assert.h>

static uint64_t activity_time;

static void record_activity(void* context, uint64_t now_ms)
{
    (void) context;
    activity_time = now_ms;
}

int main(void)
{
    assert(platform_init(true));
    const uint64_t start = platform_time_ms();
    host_power_test_advance_time(250U);
    assert(platform_time_ms() >= start + 250U);
    assert((platform_runtime_wait_until(platform_time_ms()) & PLATFORM_RUNTIME_EVENT_POWER) != 0U);
    host_power_test_activity(record_activity, NULL);
    assert(activity_time >= start + 250U);
    assert(platform_power_set_brightness(42U));
    assert(host_power_test_brightness() == 42U);
    platform_framebuffer_t framebuffer;
    assert(platform_display_init(&framebuffer));
    framebuffer.pixels[0] = 0x1234U;
    assert(platform_display_present(&framebuffer));
    assert(platform_power_set_brightness(0U));
    assert(host_power_test_brightness() == 0U);
    assert(framebuffer.pixels[0] == 0x1234U);
    framebuffer.pixels[0] = 0x5678U;
    assert(platform_display_present(&framebuffer));
    assert(host_power_test_brightness() == 0U);
    assert(platform_power_set_brightness(42U));
    assert(framebuffer.pixels[0] == 0x5678U);
    assert(host_power_test_brightness() == 42U);
    platform_display_shutdown();
    host_power_test_fail(HOST_POWER_FAIL_PREPARE);
    assert(!platform_power_prepare_sleep());
    assert(platform_power_prepare_sleep());
    host_power_test_wake(PLATFORM_POWER_WAKE_KEYBOARD | PLATFORM_POWER_WAKE_POINTER);
    assert(platform_power_collect_wake_causes() == (PLATFORM_POWER_WAKE_KEYBOARD | PLATFORM_POWER_WAKE_POINTER));
    assert(platform_power_collect_wake_causes() == PLATFORM_POWER_WAKE_NONE);
    platform_shutdown();
    return 0;
}
