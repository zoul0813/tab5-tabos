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

typedef uint16_t platform_pixel_t;
typedef void (*platform_update_fn)(void);
typedef struct platform_riscv32_context platform_riscv32_context_t;
typedef struct platform_mutex platform_mutex_t;

typedef enum {
    PLATFORM_RISCV32_YIELDED = 0,
    PLATFORM_RISCV32_RETURNED,
    PLATFORM_RISCV32_FAULT,
} platform_riscv32_result_t;

typedef struct {
    platform_pixel_t *pixels;
    size_t width;
    size_t height;
    size_t stride_pixels;
} platform_framebuffer_t;

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
    const char *keyboard_name;
    bool keyboard_present;
} platform_diagnostics_t;

bool platform_init(bool headless);
int platform_run(platform_update_fn update);
void platform_shutdown(void);
const char *platform_name(void);
const char *platform_display_name(void);
bool platform_get_diagnostics(platform_diagnostics_t *diagnostics);
void platform_log(const char *message);
uint64_t platform_time_ms(void);
void *platform_executable_alloc(size_t size);
void *platform_executable_prepare(void *memory, size_t size);
const void *platform_executable_data_pointer(const void *memory);
void platform_executable_free(void *memory);
bool platform_can_execute_riscv32(void);
platform_riscv32_context_t *platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api,
    void *user_data);
platform_riscv32_result_t platform_riscv32_step(
    platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status);
void platform_riscv32_destroy(platform_riscv32_context_t *context);
void *platform_riscv32_current_user_data(void);
void platform_input_wait(void);
platform_mutex_t *platform_mutex_create(void);
void platform_mutex_destroy(platform_mutex_t *mutex);
void platform_mutex_lock(platform_mutex_t *mutex);
void platform_mutex_unlock(platform_mutex_t *mutex);

bool platform_display_init(platform_framebuffer_t *framebuffer);
bool platform_display_present(const platform_framebuffer_t *framebuffer);
void platform_display_shutdown(void);

#endif
