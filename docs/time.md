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
