#include "power_test.h"
#include <tabos/filesystem.h>
#include <tabos/network.h>

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
    assert(host_power_test_panel_enabled());
    host_power_test_fail(HOST_POWER_FAIL_PANEL);
    assert(!platform_power_set_panel_enabled(false));
    assert(host_power_test_panel_enabled());
    assert(platform_power_set_panel_enabled(false));
    assert(!host_power_test_panel_enabled());
    assert(framebuffer.pixels[0] == 0x1234U);
    framebuffer.pixels[0] = 0x5678U;
    assert(platform_display_present(&framebuffer));
    assert(host_power_test_brightness() == 0U);
    assert(platform_power_set_panel_enabled(true));
    assert(host_power_test_panel_enabled());
    assert(platform_power_set_brightness(42U));
    assert(framebuffer.pixels[0] == 0x5678U);
    assert(host_power_test_brightness() == 42U);
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        assert(platform_display_power_suspend() == 0);
        assert(platform_display_power_suspend() == 0);
        assert(!host_power_test_panel_enabled() && host_power_test_brightness() == 0U);
        assert(framebuffer.pixels[0] == 0x5678U);
        assert(platform_display_power_resume() == 0);
        assert(platform_display_power_resume() == 0);
        assert(host_power_test_panel_enabled() && host_power_test_brightness() == 42U);
    }
    host_power_test_fail(HOST_POWER_FAIL_PANEL);
    assert(platform_display_power_suspend() == -TABOS_EIO);
    host_power_test_fail(HOST_POWER_FAIL_BRIGHTNESS);
    assert(platform_display_power_resume() == -TABOS_EIO);
    assert(platform_display_power_resume() == 0);
    assert(platform_display_power_suspend() == 0);
    platform_display_shutdown();
    assert(platform_display_init(&framebuffer));
    assert(platform_power_set_brightness(37U));
    assert(platform_display_power_suspend() == 0);
    assert(host_power_test_brightness() == 0U);
    assert(platform_display_power_resume() == 0);
    assert(host_power_test_brightness() == 37U);
    platform_display_shutdown();
    assert(platform_network_init("power-test", NULL));
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        const int socket = platform_network_socket_open(4U, TABOS_SOCKET_UDP);
        assert(socket >= 0);
        assert(platform_network_power_suspend() == -TABOS_EBUSY);
        assert(platform_network_socket_close(socket) == 0);
        assert(platform_network_power_suspend() == 0);
        assert(platform_network_socket_open(4U, TABOS_SOCKET_UDP) == -TABOS_EBUSY);
        assert(platform_tls_connect("unused.test", 443U) == -TABOS_EBUSY);
        assert(platform_network_power_resume() == 0);
    }
    platform_network_socket_operations_shutdown();
    platform_network_shutdown();
    host_power_test_fail(HOST_POWER_FAIL_PREPARE);
    assert(!platform_power_prepare_sleep());
    assert(platform_power_prepare_sleep());
    host_power_test_wake(PLATFORM_POWER_WAKE_KEYBOARD | PLATFORM_POWER_WAKE_POINTER);
    assert(platform_power_collect_wake_causes() == (PLATFORM_POWER_WAKE_KEYBOARD | PLATFORM_POWER_WAKE_POINTER));
    assert(platform_power_collect_wake_causes() == PLATFORM_POWER_WAKE_NONE);
    platform_shutdown();
    return 0;
}
