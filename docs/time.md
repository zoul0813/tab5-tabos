# Time and Timers

TabOS exposes a portable monotonic millisecond clock and lightweight polling timers through `<tabos/time.h>`. The same API uses SDL time on host targets and the ESP-IDF monotonic timer on Tab5.

```c
tabos_timer_t timer = {0};

tabos_timer_start(&timer, 500, 500);
if (tabos_timer_poll(&timer)) {
    /* periodic work */
}
tabos_timer_cancel(&timer);
```

The first duration is delay before first expiration. The second is repeat interval; zero creates one-shot timer. Polling late reports one expiration and advances deadline past current time, preventing catch-up bursts. Timers do not create threads or callbacks. Runtime owner must poll them.

`tabos_time_monotonic_ms()` is suitable for elapsed-time measurement and scheduling. It is not wall-clock time and has no date, timezone, or calendar meaning.

The `date` core utility displays the current UTC wall clock:

```sh
date
```

Set the Tab5 RTC or host-simulator clock offset with a UTC value:

```sh
date -s "2026-08-28 14:30:00"
```

Tab5 RTC values are limited to years 2000 through 2099. `date -u` is accepted explicitly;
UTC is always used because TabOS does not currently implement timezones or daylight-saving
rules.

## RTC and Wall Clock

Loaded applications include `<tabos/clock.h>` for UTC calendar and Unix-epoch access:

```c
tabos_datetime_t now;
if (tabos_clock_get(&now) == 0) {
    printf("%04ld-%02u-%02u %02u:%02u:%02u UTC\n",
           (long) now.year,
           now.month,
           now.day,
           now.hour,
           now.minute,
           now.second);
}
```

`tabos_clock_get_epoch()` and `tabos_clock_set_epoch()` use signed Unix seconds
since `1970-01-01 00:00:00 UTC`. `tabos_clock_set()` accepts a calendar value.
Tab5 supports RX8130CE years 2000 through 2099. Values outside that range cannot
be written to its RTC.

Standard application functions `time()`, `gettimeofday()`,
`clock_gettime(CLOCK_REALTIME)`, `clock_gettime(CLOCK_MONOTONIC)`, and
`clock_settime(CLOCK_REALTIME)` use the same services. Wall-clock precision is one
second. The kernel stores UTC and does not apply timezones, daylight-saving rules,
or locale policy.

Tab5 uses the onboard RX8130CE RTC at I2C address `0x32`. Host targets initialize
from the workstation clock. Setting time in the host simulator changes only an
in-process offset; it never changes the workstation clock. RTC absence or invalid
calendar data is nonfatal and wall-clock calls fail with `errno` set.
