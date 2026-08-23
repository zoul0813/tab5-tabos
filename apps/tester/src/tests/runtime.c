#include <tester/test.h>

#include <tabos/runtime_time.h>
#include <tabos/system.h>

#include <string.h>

void tester_test_runtime(tester_context_t* context)
{
    const uint64_t before = tabos_monotonic_ms();
    tester_expect(context, tabos_sleep_ms(1U) == 0, "sleep completes");
    tester_expect(context, tabos_monotonic_ms() >= before, "monotonic time advances");

    tabos_runtime_timer_t timer;
    tabos_runtime_timer_start(&timer, 0U, 0U);
    tester_expect(context, tabos_runtime_timer_poll(&timer), "one-shot timer fires");
    tester_expect(context, !tabos_runtime_timer_poll(&timer), "one-shot timer stops");

    tabos_system_info_t info;
    tester_expect(context, tabos_system_info(&info) == 0, "system information available");
    tester_expect(context, info.target[0] != '\0', "target name available");
    tester_expect(context, info.device[0] != '\0', "device name available");
    tester_expect(context, info.display_width > 0U && info.display_height > 0U, "display dimensions available");
    tester_expect(context, info.cpu_cores > 0U, "processor core count available");
}
