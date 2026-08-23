#include <tabos/graphics.h>
#include <tabos/runtime_time.h>

#include <stdio.h>

static void report_phase(const char *name, unsigned int frames, uint64_t elapsed)
{
    const uint64_t fps_tenths = elapsed != 0U
        ? (uint64_t)frames * 10000U / elapsed : 0U;
    printf("%s: %u frames in %llu ms (%llu.%llu FPS)\n", name, frames,
           (unsigned long long)elapsed,
           (unsigned long long)(fps_tenths / 10U),
           (unsigned long long)(fps_tenths % 10U));
}

int main(void)
{
    tabos_graphics_t graphics = {0};
    if (tabos_graphics_open(&graphics) != 0) return 1;
    const uint32_t capabilities = tabos_graphics_capabilities(&graphics);
    const unsigned int phase_frames = 60U;
    uint64_t started = tabos_monotonic_ms();
    for (unsigned int frame = 0U; frame < phase_frames; ++frame) {
        if (tabos_graphics_present(&graphics) != 0) return 2;
    }
    const uint64_t present_only_ms = tabos_monotonic_ms() - started;

    started = tabos_monotonic_ms();
    for (unsigned int frame = 0U; frame < phase_frames; ++frame) {
        (void)tabos_graphics_clear(&graphics, TABOS_RGB565(frame * 3U, 12, 24));
        if (tabos_graphics_present(&graphics) != 0) return 2;
    }
    const uint64_t clear_present_ms = tabos_monotonic_ms() - started;

    uint64_t queue_ms = 0U;
    uint64_t present_ms = 0U;
    const unsigned int frames = 120U;
    started = tabos_monotonic_ms();
    for (unsigned int frame = 0U; frame < frames; ++frame) {
        const uint64_t queue_started = tabos_monotonic_ms();
        (void)tabos_graphics_clear(&graphics, TABOS_RGB565(8, 12, 24));
        for (unsigned int index = 0U; index < 64U; ++index) {
            const int32_t x = (int32_t)((index * 37U + frame * 3U) % graphics.width);
            const int32_t y = (int32_t)((index * 19U + frame * 2U) % graphics.height);
            (void)tabos_graphics_fill_rect(&graphics, x, y, 48U, 32U,
                TABOS_RGB565(index * 4U, 255U - index * 3U, index * 7U));
        }
        queue_ms += tabos_monotonic_ms() - queue_started;
        const uint64_t present_started = tabos_monotonic_ms();
        if (tabos_graphics_present(&graphics) != 0) return 2;
        present_ms += tabos_monotonic_ms() - present_started;
    }
    const uint64_t elapsed = tabos_monotonic_ms() - started;
    (void)tabos_graphics_close(&graphics);
    report_phase("Present only", phase_frames, present_only_ms);
    report_phase("Clear + present", phase_frames, clear_present_ms);
    report_phase("Scene", frames, elapsed);
    printf("Queue: %llu ms; present/drain: %llu ms; %s acceleration\n",
           (unsigned long long)queue_ms, (unsigned long long)present_ms,
           (capabilities & TABOS_GRAPHICS_CAP_HARDWARE_ACCELERATED) != 0U
               ? "hardware" : "software");
    return 0;
}
