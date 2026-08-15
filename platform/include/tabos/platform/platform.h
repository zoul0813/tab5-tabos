#ifndef TABOS_PLATFORM_PLATFORM_H
#define TABOS_PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/elf_api.h>

enum {
    TABOS_DISPLAY_WIDTH = 1280,
    TABOS_DISPLAY_HEIGHT = 720,
};

typedef uint16_t tab_pixel_t;
typedef void (*tab_platform_update_fn)(void);
typedef struct tab_platform_riscv32_context tab_platform_riscv32_context_t;

typedef enum {
    TAB_PLATFORM_RISCV32_YIELDED = 0,
    TAB_PLATFORM_RISCV32_RETURNED,
    TAB_PLATFORM_RISCV32_FAULT,
} tab_platform_riscv32_result_t;

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
    const char *keyboard_name;
    bool keyboard_present;
} tab_platform_diagnostics_t;

bool tab_platform_init(bool headless);
int tab_platform_run(tab_platform_update_fn update);
void tab_platform_shutdown(void);
const char *tab_platform_name(void);
const char *tab_platform_display_name(void);
bool tab_platform_get_diagnostics(tab_platform_diagnostics_t *diagnostics);
void tab_platform_log(const char *message);
uint64_t tab_platform_time_ms(void);
void *tab_platform_executable_alloc(size_t size);
void *tab_platform_executable_prepare(void *memory, size_t size);
const void *tab_platform_executable_data_pointer(const void *memory);
void tab_platform_executable_free(void *memory);
bool tab_platform_can_execute_riscv32(void);
tab_platform_riscv32_context_t *tab_platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api);
tab_platform_riscv32_result_t tab_platform_riscv32_step(
    tab_platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status);
void tab_platform_riscv32_destroy(tab_platform_riscv32_context_t *context);
void tab_platform_input_wait(void);

bool tab_platform_display_init(tab_framebuffer_t *framebuffer);
bool tab_platform_display_present(const tab_framebuffer_t *framebuffer);
void tab_platform_display_shutdown(void);

#endif
