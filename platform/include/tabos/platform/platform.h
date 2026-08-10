#ifndef TABOS_PLATFORM_PLATFORM_H
#define TABOS_PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TABOS_DISPLAY_WIDTH = 1280,
    TABOS_DISPLAY_HEIGHT = 720,
};

typedef uint16_t tab_pixel_t;

typedef struct {
    tab_pixel_t *pixels;
    size_t width;
    size_t height;
    size_t stride_pixels;
} tab_framebuffer_t;

typedef struct {
    const char *device_name;
    unsigned int cpu_cores;
    unsigned int cpu_frequency_mhz;
    uint64_t memory_total_bytes;
    uint64_t memory_free_bytes;
    bool memory_free_known;
    uint64_t external_memory_total_bytes;
    uint64_t external_memory_free_bytes;
    bool external_memory_present;
    uint64_t flash_capacity_bytes;
    uint64_t storage_total_bytes;
    uint64_t storage_free_bytes;
    bool storage_mounted;
} tab_platform_diagnostics_t;

bool tab_platform_init(bool headless);
int tab_platform_run(void);
void tab_platform_shutdown(void);
const char *tab_platform_name(void);
const char *tab_platform_display_name(void);
bool tab_platform_get_diagnostics(tab_platform_diagnostics_t *diagnostics);
void tab_platform_log(const char *message);

bool tab_platform_display_init(tab_framebuffer_t *framebuffer);
bool tab_platform_display_present(const tab_framebuffer_t *framebuffer);
void tab_platform_display_shutdown(void);

#endif
