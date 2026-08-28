#include <tester/test.h>

#include <tabos/clock.h>
#include <tabos/runtime_time.h>
#include <tabos/system.h>

#include <sys/time.h>
#include <time.h>
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

    tabos_datetime_t datetime;
    int64_t epoch_seconds = 0;
    struct timeval wall_time;
    struct timespec realtime;
    struct timespec monotonic;
    const time_t standard_time = time(NULL);
    tester_expect(context, tabos_clock_get_epoch(&epoch_seconds) == 0 && epoch_seconds >= 946684800,
                  "wall clock returns plausible Unix time");
    tester_expect(context,
                  tabos_clock_get(&datetime) == 0 && datetime.year >= 2000 && datetime.month >= 1U &&
                      datetime.month <= 12U && datetime.day >= 1U && datetime.day <= 31U,
                  "RTC returns a valid UTC calendar value");
    tester_expect(context, gettimeofday(&wall_time, NULL) == 0 && (int64_t) wall_time.tv_sec >= epoch_seconds,
                  "gettimeofday reads wall clock");
    tester_expect(context, clock_gettime(CLOCK_REALTIME, &realtime) == 0 && realtime.tv_sec >= wall_time.tv_sec,
                  "CLOCK_REALTIME reads wall clock");
    tester_expect(context, clock_gettime(CLOCK_MONOTONIC, &monotonic) == 0 && monotonic.tv_sec >= 0,
                  "CLOCK_MONOTONIC reads elapsed time");
    tester_expect(context, standard_time >= (time_t) epoch_seconds - 1 && standard_time <= (time_t) epoch_seconds + 1,
                  "time reads wall clock");
    int64_t unchanged_epoch = 0;
    tester_expect(context, tabos_clock_get_epoch(&unchanged_epoch) == 0 && tabos_clock_set_epoch(unchanged_epoch) == 0,
                  "RTC accepts unchanged wall-clock value");
}
