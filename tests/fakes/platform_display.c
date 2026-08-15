#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdlib.h>

static tab_pixel_t pixels[TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT];
static uint64_t monotonic_ms;

bool tab_platform_display_init(tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    *framebuffer = (tab_framebuffer_t){
        .pixels = pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool tab_platform_display_present(const tab_framebuffer_t *framebuffer)
{
    return framebuffer != NULL && framebuffer->pixels == pixels;
}

void tab_platform_display_shutdown(void)
{
}

const char *tab_platform_name(void)
{
    return "test";
}

const char *tab_platform_display_name(void)
{
    return "test display";
}

bool tab_platform_get_diagnostics(tab_platform_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    *diagnostics = (tab_platform_diagnostics_t){
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

void tab_platform_log(const char *message)
{
    (void)message;
}

uint64_t tab_platform_time_ms(void)
{
    return monotonic_ms;
}

void *tab_platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void *tab_platform_executable_prepare(void *memory, size_t size)
{
    return memory != NULL && size > 0U ? memory : NULL;
}

const void *tab_platform_executable_data_pointer(const void *memory)
{
    return memory;
}

void tab_platform_executable_free(void *memory)
{
    free(memory);
}

bool tab_platform_can_execute_riscv32(void)
{
    return false;
}

tab_platform_riscv32_context_t *tab_platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api)
{
    (void)entry;
    (void)memory;
    (void)memory_size;
    (void)minimum_address;
    (void)api;
    return NULL;
}

tab_platform_riscv32_result_t tab_platform_riscv32_step(
    tab_platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status)
{
    (void)context;
    (void)instruction_budget;
    (void)returned_status;
    return TAB_PLATFORM_RISCV32_FAULT;
}

void tab_platform_riscv32_destroy(tab_platform_riscv32_context_t *context)
{
    (void)context;
}

void tab_test_platform_set_time_ms(uint64_t time_ms)
{
    monotonic_ms = time_ms;
}

void tab_test_platform_advance_time_ms(uint64_t elapsed_ms)
{
    monotonic_ms += elapsed_ms;
}

void tab_platform_input_wait(void)
{
}
