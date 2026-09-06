#include "platform_test.h"

#include <tabos/platform/platform.h>

#include <assert.h>
#include <stdint.h>

int main(void)
{
    assert(platform_runtime_wait_until(500U) == PLATFORM_RUNTIME_EVENT_NONE);
    assert(test_platform_runtime_wait_deadline() == 500U);

    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    platform_runtime_notify_from_isr(PLATFORM_RUNTIME_EVENT_CAMERA);
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) ==
           (PLATFORM_RUNTIME_EVENT_INPUT | PLATFORM_RUNTIME_EVENT_CAMERA));
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) == PLATFORM_RUNTIME_EVENT_NONE);

    test_platform_set_time_ms(500U);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    assert(platform_runtime_wait_until(500U) == (PLATFORM_RUNTIME_EVENT_INPUT | PLATFORM_RUNTIME_EVENT_DEADLINE));

    platform_stop_run_loop();
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) == PLATFORM_RUNTIME_EVENT_SHUTDOWN);
    return 0;
}
