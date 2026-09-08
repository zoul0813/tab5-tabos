#include "activity.h"

#include <tabos/platform/platform.h>

#ifndef NDEBUG
#include <esp_attr.h>
#include <stdatomic.h>
#include <stdio.h>
#include <inttypes.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "ISR activity counters require lock-free unsigned atomics");
/* Boot-lifetime uint32 counters wrap; compare snapshots with unsigned subtraction.
 * Separate loads are approximate snapshots, never service synchronization. */
static DRAM_ATTR atomic_uint counters[TAB5_ACTIVITY_COUNT];

void IRAM_ATTR tab5_activity_record(tab5_activity_counter_t counter, unsigned int amount)
{
    atomic_fetch_add_explicit(&counters[counter], amount, memory_order_relaxed);
}
#endif

void platform_runtime_log_activity(void)
{
#ifndef NDEBUG
    unsigned int snapshot[TAB5_ACTIVITY_COUNT];
    for (unsigned int index = 0U; index < TAB5_ACTIVITY_COUNT; ++index) {
        snapshot[index] = atomic_load_explicit(&counters[index], memory_order_relaxed);
    }
    char message[320];
    (void) snprintf(message, sizeof(message),
                    "Platform activity: ms=%" PRIu64 " audio_chunks=%u audio_frames=%u audio_errors=%u "
                    "headphone_reads=%u headphone_errors=%u vsync=%u ppa=%u",
                    platform_time_ms(), snapshot[TAB5_ACTIVITY_AUDIO_CHUNKS], snapshot[TAB5_ACTIVITY_AUDIO_FRAMES],
                    snapshot[TAB5_ACTIVITY_AUDIO_ERRORS], snapshot[TAB5_ACTIVITY_HEADPHONE_READS],
                    snapshot[TAB5_ACTIVITY_HEADPHONE_ERRORS], snapshot[TAB5_ACTIVITY_VSYNC],
                    snapshot[TAB5_ACTIVITY_PPA]);
    platform_log(message);
#endif
}
