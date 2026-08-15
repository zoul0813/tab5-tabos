#include <tabos/platform/platform.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    HOST_RV32_RAM_SIZE = 1024 * 1024,
};

#define MINI_RV32_RAM_SIZE HOST_RV32_RAM_SIZE
#define MINIRV32_RAM_IMAGE_OFFSET 0U
#define MINIRV32_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvariadic-macros"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <mini-rv32ima.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static const uint32_t HOST_RV32_CONSOLE_WRITE = UINT32_C(0xffff0000);
static const uint32_t HOST_RV32_REQUEST_EXIT = UINT32_C(0xffff0004);
static const uint32_t HOST_RV32_RETURN = UINT32_C(0xffff0008);

struct platform_riscv32_context {
    uint8_t *memory;
    struct MiniRV32IMAState state;
    tabos_elf_api_t api;
};

static void write_u32(uint8_t *memory, uint32_t address, uint32_t value)
{
    memory[address] = (uint8_t)value;
    memory[address + 1U] = (uint8_t)(value >> 8U);
    memory[address + 2U] = (uint8_t)(value >> 16U);
    memory[address + 3U] = (uint8_t)(value >> 24U);
}

static const char *guest_string(const uint8_t *memory, uint32_t address)
{
    if (address >= HOST_RV32_RAM_SIZE ||
        memchr(memory + address, '\0', HOST_RV32_RAM_SIZE - address) == NULL) {
        return NULL;
    }
    return (const char *)memory + address;
}

void *platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void *platform_executable_prepare(void *memory, size_t size)
{
    if (memory == NULL || size == 0U) return NULL;
    __builtin___clear_cache((char *)memory, (char *)memory + size);
    return memory;
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
    return true;
}

platform_riscv32_context_t *platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api)
{
    if (entry == NULL || memory == NULL || memory_size == 0U || api == NULL ||
        memory_size > HOST_RV32_RAM_SIZE ||
        minimum_address > HOST_RV32_RAM_SIZE - memory_size) {
        return NULL;
    }

    const uintptr_t entry_offset = (uintptr_t)entry - (uintptr_t)memory;
    if (entry_offset >= memory_size) return NULL;

    platform_riscv32_context_t *context = calloc(1U, sizeof(*context));
    if (context == NULL) return NULL;
    context->memory = calloc(1U, HOST_RV32_RAM_SIZE);
    if (context->memory == NULL) {
        free(context);
        return NULL;
    }
    memcpy(context->memory + minimum_address, memory, memory_size);

    const uint32_t image_end = minimum_address + (uint32_t)memory_size;
    const uint32_t api_address = (image_end + 15U) & ~15U;
    if (api_address > HOST_RV32_RAM_SIZE - 16U) {
        platform_riscv32_destroy(context);
        return NULL;
    }
    write_u32(context->memory, api_address, api->abi_version);
    write_u32(context->memory, api_address + 4U, HOST_RV32_CONSOLE_WRITE);
    write_u32(context->memory, api_address + 8U, HOST_RV32_REQUEST_EXIT);

    context->api = *api;
    context->state.pc = minimum_address + (uint32_t)entry_offset;
    context->state.regs[2] = HOST_RV32_RAM_SIZE - 16U;
    context->state.regs[10] = api_address;
    context->state.regs[1] = HOST_RV32_RETURN;
    return context;
}

platform_riscv32_result_t platform_riscv32_step(
    platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status)
{
    if (context == NULL || instruction_budget == 0U || returned_status == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    for (unsigned int count = 0U; count < instruction_budget; ++count) {
        if (context->state.pc == HOST_RV32_RETURN) {
            *returned_status = (int)context->state.regs[10];
            return PLATFORM_RISCV32_RETURNED;
        }
        if (context->state.pc == HOST_RV32_CONSOLE_WRITE) {
            const char *text = guest_string(context->memory, context->state.regs[10]);
            if (text == NULL || context->api.console_write == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            context->api.console_write(text);
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_REQUEST_EXIT) {
            if (context->api.request_exit == NULL) return PLATFORM_RISCV32_FAULT;
            context->api.request_exit((int)context->state.regs[10]);
            context->state.pc = context->state.regs[1];
            continue;
        }
        (void)MiniRV32IMAStep(&context->state, context->memory, 0U, 0U, 1);
        if (context->state.mcause != 0U) return PLATFORM_RISCV32_FAULT;
    }
    return PLATFORM_RISCV32_YIELDED;
}

void platform_riscv32_destroy(platform_riscv32_context_t *context)
{
    if (context != NULL) {
        free(context->memory);
        free(context);
    }
}
