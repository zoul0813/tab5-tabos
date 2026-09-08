#include <tabos/platform/platform.h>

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>

typedef struct {
        uint32_t delay_ms;
        platform_runtime_events_t events;
        bool stop;
} delayed_notification_t;

static int signal_after_delay(void* data)
{
    SDL_Delay(20U);
    platform_signal_notify(data);
    return 0;
}

static int notify_after_delay(void* data)
{
    const delayed_notification_t* notification = data;
    SDL_Delay(notification->delay_ms);
    if (notification->stop) {
        platform_stop_run_loop();
    } else {
        platform_runtime_notify_from_isr(notification->events);
    }
    return 0;
}

int main(void)
{
    assert(platform_init(true));

    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) == PLATFORM_RUNTIME_EVENT_INPUT);

    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_INPUT);
    assert(platform_runtime_wait_until(platform_time_ms()) ==
           (PLATFORM_RUNTIME_EVENT_INPUT | PLATFORM_RUNTIME_EVENT_DEADLINE));

    const uint64_t deadline = platform_time_ms() + 20U;
    (void) deadline;
    assert(platform_runtime_wait_until(deadline) == PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(platform_time_ms() >= deadline);

    const delayed_notification_t notification = {
        .delay_ms = 20U,
        .events   = PLATFORM_RUNTIME_EVENT_APPLICATION,
    };
    SDL_Thread* thread = SDL_CreateThread(notify_after_delay, "runtime-wake-test", (void*) &notification);
    assert(thread != NULL);
    const uint64_t wait_started = platform_time_ms();
    (void) wait_started;
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) == PLATFORM_RUNTIME_EVENT_APPLICATION);
    assert(platform_time_ms() - wait_started >= notification.delay_ms);
    assert(platform_time_ms() - wait_started < 500U);
    SDL_WaitThread(thread, NULL);

    const delayed_notification_t earlier_deadline = {
        .delay_ms = 20U,
        .events   = PLATFORM_RUNTIME_EVENT_DEADLINE,
    };
    thread = SDL_CreateThread(notify_after_delay, "runtime-deadline-test", (void*) &earlier_deadline);
    assert(thread != NULL);
    const uint64_t long_deadline = platform_time_ms() + 1000U;
    (void) long_deadline;
    assert(platform_runtime_wait_until(long_deadline) == PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(platform_time_ms() < long_deadline);
    SDL_WaitThread(thread, NULL);

    const delayed_notification_t shutdown = {
        .delay_ms = 20U,
        .stop     = true,
    };
    thread = SDL_CreateThread(notify_after_delay, "runtime-shutdown-test", (void*) &shutdown);
    assert(thread != NULL);
    assert(platform_runtime_wait_until(PLATFORM_RUNTIME_DEADLINE_NONE) == PLATFORM_RUNTIME_EVENT_SHUTDOWN);
    SDL_WaitThread(thread, NULL);

    platform_signal_t* signal = platform_signal_create();
    assert(signal != NULL);
    // Notification before blocking must be retained; repeated notifications coalesce.
    platform_signal_notify(signal);
    platform_signal_notify(signal);
    platform_signal_wait(signal, UINT32_MAX);
    uint64_t started = platform_time_ms();
    platform_signal_wait(signal, 20U);
    assert(platform_time_ms() - started >= 20U);
    thread = SDL_CreateThread(signal_after_delay, "keyboard-signal-test", signal);
    assert(thread != NULL);
    started = platform_time_ms();
    platform_signal_wait(signal, UINT32_MAX);
    assert(platform_time_ms() - started < 500U);
    SDL_WaitThread(thread, NULL);
    platform_signal_destroy(signal);
    platform_shutdown();
    return 0;
}
