#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct platform_mutex {
    unsigned int unused;
};

static platform_pixel_t pixels[TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT];
static uint64_t monotonic_ms;
static char last_log[256];

bool platform_display_init(platform_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    *framebuffer = (platform_framebuffer_t){
        .pixels = pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool platform_display_present(const platform_framebuffer_t *framebuffer)
{
    return framebuffer != NULL && framebuffer->pixels == pixels;
}

void platform_display_shutdown(void)
{
}

const char *platform_name(void)
{
    return "test";
}

const char *platform_display_name(void)
{
    return "test display";
}

bool platform_get_diagnostics(platform_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    *diagnostics = (platform_diagnostics_t){
        .device_name = "TEST DEVICE",
        .cpu_cores = 2U,
        .cpu_frequency_mhz = 100U,
        .memory_total_bytes = 1024U,
        .memory_free_bytes = 512U,
        .memory_free_known = true,
        .keyboard_name = "TEST KEYBOARD",
        .keyboard_present = true,
    };
    return true;
}

void platform_log(const char *message)
{
    (void)snprintf(last_log, sizeof(last_log), "%s", message != NULL ? message : "");
}

const char *test_platform_last_log(void)
{
    return last_log;
}

void test_platform_clear_log(void)
{
    last_log[0] = '\0';
}

uint64_t platform_time_ms(void)
{
    return monotonic_ms;
}

void *platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void *platform_executable_prepare(void *memory, size_t size)
{
    return memory != NULL && size > 0U ? memory : NULL;
}

bool platform_executable_finalize(void *memory, size_t size)
{
    return memory != NULL && size > 0U;
}

const void *platform_executable_data_pointer(const void *memory)
{
    return memory;
}

void platform_executable_free(void *memory)
{
    free(memory);
}

bool platform_can_execute_riscv32(void)
{
    return false;
}

platform_riscv32_context_t *platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api,
    size_t argc,
    const char *const *argv,
    void *user_data)
{
    (void)entry;
    (void)memory;
    (void)memory_size;
    (void)minimum_address;
    (void)api;
    (void)argc;
    (void)argv;
    (void)user_data;
    return NULL;
}

platform_riscv32_result_t platform_riscv32_step(
    platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status)
{
    (void)context;
    (void)instruction_budget;
    (void)returned_status;
    return PLATFORM_RISCV32_FAULT;
}

void platform_riscv32_destroy(platform_riscv32_context_t *context)
{
    (void)context;
}

void *platform_riscv32_current_user_data(void)
{
    return NULL;
}

void test_platform_set_time_ms(uint64_t time_ms)
{
    monotonic_ms = time_ms;
}

void test_platform_advance_time_ms(uint64_t elapsed_ms)
{
    monotonic_ms += elapsed_ms;
}

void platform_input_wait(void)
{
}

platform_mutex_t *platform_mutex_create(void)
{
    return calloc(1U, sizeof(platform_mutex_t));
}

void platform_mutex_destroy(platform_mutex_t *mutex)
{
    free(mutex);
}

void platform_mutex_lock(platform_mutex_t *mutex)
{
    (void)mutex;
}

void platform_mutex_unlock(platform_mutex_t *mutex)
{
    (void)mutex;
}
