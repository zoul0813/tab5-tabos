#include <tabos/internal/elf_api.h>
#include <tabos/runtime_time.h>
#include <tabos/system.h>

#include <errno.h>
#include <sched.h>
#include <string.h>

extern const tabos_elf_api_t *tabos_runtime_api;

uint64_t tabos_monotonic_ms(void)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->monotonic_ms != NULL
        ? tabos_runtime_api->monotonic_ms() : 0U;
}

int tabos_sleep_ms(uint32_t milliseconds)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->monotonic_ms == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const uint64_t deadline = tabos_monotonic_ms() + milliseconds;
    while (tabos_monotonic_ms() < deadline) {
        if (sched_yield() != 0) return -1;
    }
    return 0;
}

void tabos_runtime_timer_start(tabos_runtime_timer_t *timer, uint32_t delay_ms,
                               uint32_t interval_ms)
{
    if (timer == NULL) return;
    *timer = (tabos_runtime_timer_t){
        .deadline_ms = tabos_monotonic_ms() + delay_ms,
        .interval_ms = interval_ms,
        .active = true,
    };
}

void tabos_runtime_timer_cancel(tabos_runtime_timer_t *timer)
{
    if (timer != NULL) timer->active = false;
}

bool tabos_runtime_timer_poll(tabos_runtime_timer_t *timer)
{
    if (timer == NULL || !timer->active || tabos_monotonic_ms() < timer->deadline_ms) return false;
    if (timer->interval_ms == 0U) timer->active = false;
    else timer->deadline_ms = tabos_monotonic_ms() + timer->interval_ms;
    return true;
}

int tabos_system_info(tabos_system_info_t *info)
{
    if (info == NULL) { errno = EINVAL; return -1; }
    if (tabos_runtime_api == NULL || tabos_runtime_api->system_info == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_system_info_t source;
    const int result = tabos_runtime_api->system_info(&source);
    if (result < 0) { errno = -result; return -1; }
    memset(info, 0, sizeof(*info));
    memcpy(info->target, source.target, sizeof(info->target));
    memcpy(info->device, source.device, sizeof(info->device));
    memcpy(info->display, source.display, sizeof(info->display));
    info->display_width = source.display_width;
    info->display_height = source.display_height;
    info->cpu_cores = source.cpu_cores;
    info->cpu_frequency_mhz = source.cpu_frequency_mhz;
    info->memory_total_bytes = (uint64_t)source.memory_total_low |
        (uint64_t)source.memory_total_high << 32U;
    info->external_memory_total_bytes = (uint64_t)source.external_memory_total_low |
        (uint64_t)source.external_memory_total_high << 32U;
    return 0;
}
