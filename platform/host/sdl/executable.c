#include <tabos/platform/platform.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HOST_RV32_RAM_SIZE = 2 * 1024 * 1024,
};

#define MINI_RV32_RAM_SIZE        HOST_RV32_RAM_SIZE
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

static const uint32_t HOST_RV32_CONSOLE_WRITE         = UINT32_C(0xffff0000);
static const uint32_t HOST_RV32_REQUEST_EXIT          = UINT32_C(0xffff0004);
static const uint32_t HOST_RV32_RETURN                = UINT32_C(0xffff0008);
static const uint32_t HOST_RV32_CONSOLE_READ          = UINT32_C(0xffff000c);
static const uint32_t HOST_RV32_CONSOLE_CLEAR         = UINT32_C(0xffff0010);
static const uint32_t HOST_RV32_FS_GETCWD             = UINT32_C(0xffff0014);
static const uint32_t HOST_RV32_FS_CHDIR              = UINT32_C(0xffff0018);
static const uint32_t HOST_RV32_FS_LIST               = UINT32_C(0xffff001c);
static const uint32_t HOST_RV32_EXEC                  = UINT32_C(0xffff0020);
static const uint32_t HOST_RV32_YIELD                 = UINT32_C(0xffff0024);
static const uint32_t HOST_RV32_CONSOLE_WRITE_RAW     = UINT32_C(0xffff0028);
static const uint32_t HOST_RV32_FD_OPEN               = UINT32_C(0xffff002c);
static const uint32_t HOST_RV32_FD_CLOSE              = UINT32_C(0xffff0030);
static const uint32_t HOST_RV32_FD_READ               = UINT32_C(0xffff0034);
static const uint32_t HOST_RV32_FD_WRITE              = UINT32_C(0xffff0038);
static const uint32_t HOST_RV32_FD_SEEK               = UINT32_C(0xffff003c);
static const uint32_t HOST_RV32_FS_STAT               = UINT32_C(0xffff0040);
static const uint32_t HOST_RV32_FD_STAT               = UINT32_C(0xffff0044);
static const uint32_t HOST_RV32_FS_MKDIR              = UINT32_C(0xffff0048);
static const uint32_t HOST_RV32_FS_UNLINK             = UINT32_C(0xffff004c);
static const uint32_t HOST_RV32_FS_RENAME             = UINT32_C(0xffff0050);
static const uint32_t HOST_RV32_FD_GET_FLAGS          = UINT32_C(0xffff0054);
static const uint32_t HOST_RV32_FD_SET_FLAGS          = UINT32_C(0xffff0058);
static const uint32_t HOST_RV32_HEAP_SBRK             = UINT32_C(0xffff005c);
static const uint32_t HOST_RV32_FS_RMDIR              = UINT32_C(0xffff0060);
static const uint32_t HOST_RV32_MONOTONIC_MS          = UINT32_C(0xffff0064);
static const uint32_t HOST_RV32_SYSTEM_INFO           = UINT32_C(0xffff0068);
static const uint32_t HOST_RV32_GRAPHICS_OPEN         = UINT32_C(0xffff006c);
static const uint32_t HOST_RV32_GRAPHICS_CLEAR        = UINT32_C(0xffff0070);
static const uint32_t HOST_RV32_GRAPHICS_FILL_RECT    = UINT32_C(0xffff0074);
static const uint32_t HOST_RV32_GRAPHICS_BLIT         = UINT32_C(0xffff0078);
static const uint32_t HOST_RV32_GRAPHICS_PRESENT      = UINT32_C(0xffff007c);
static const uint32_t HOST_RV32_GRAPHICS_CLOSE        = UINT32_C(0xffff0080);
static const uint32_t HOST_RV32_GRAPHICS_CAPABILITIES = UINT32_C(0xffff0084);
static const uint32_t HOST_RV32_GRAPHICS_BLIT_EX      = UINT32_C(0xffff0088);
static const uint32_t HOST_RV32_TTY_GET_MODE          = UINT32_C(0xffff008c);
static const uint32_t HOST_RV32_TTY_SET_MODE          = UINT32_C(0xffff0090);
static const uint32_t HOST_RV32_INPUT_POLL            = UINT32_C(0xffff0094);

enum {
    HOST_RV32_API_SIZE = 152,
};

struct platform_riscv32_context {
        uint8_t* memory;
        struct MiniRV32IMAState state;
        tabos_elf_api_t api;
        void* user_data;
        uint32_t heap_base;
        uint32_t heap_break;
        uint32_t heap_end;
};

static void* current_user_data;

static void write_u32(uint8_t* memory, uint32_t address, uint32_t value)
{
    memory[address]      = (uint8_t) value;
    memory[address + 1U] = (uint8_t) (value >> 8U);
    memory[address + 2U] = (uint8_t) (value >> 16U);
    memory[address + 3U] = (uint8_t) (value >> 24U);
}

static uint16_t read_u16(const uint8_t* memory, uint32_t address)
{
    return (uint16_t) ((uint16_t) memory[address] | ((uint16_t) memory[address + 1U] << 8U));
}

static uint32_t read_u32(const uint8_t* memory, uint32_t address)
{
    return (uint32_t) memory[address] | ((uint32_t) memory[address + 1U] << 8U) |
           ((uint32_t) memory[address + 2U] << 16U) | ((uint32_t) memory[address + 3U] << 24U);
}

static const char* guest_string(const uint8_t* memory, uint32_t address)
{
    if (address >= HOST_RV32_RAM_SIZE || memchr(memory + address, '\0', HOST_RV32_RAM_SIZE - address) == NULL) {
        return NULL;
    }
    return (const char*) memory + address;
}

static void* guest_buffer(uint8_t* memory, uint32_t address, uint32_t capacity)
{
    return address <= HOST_RV32_RAM_SIZE && capacity <= HOST_RV32_RAM_SIZE - address ? memory + address : NULL;
}

void* platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void* platform_executable_prepare(void* memory, size_t size)
{
    if (memory == NULL || size == 0U) {
        return NULL;
    }
    __builtin___clear_cache((char*) memory, (char*) memory + size);
    return memory;
}

bool platform_executable_finalize(void* memory, size_t size)
{
    if (memory == NULL || size == 0U) {
        return false;
    }
    __builtin___clear_cache((char*) memory, (char*) memory + size);
    return true;
}

const void* platform_executable_data_pointer(const void* memory, size_t size)
{
    (void) size;
    return memory;
}

void platform_executable_free(void* memory)
{
    free(memory);
}

bool platform_can_execute_riscv32(void)
{
    return true;
}

platform_riscv32_context_t* platform_riscv32_create(const void* entry, const void* memory, size_t memory_size,
                                                    uint32_t minimum_address, const tabos_elf_api_t* api, size_t argc,
                                                    const char* const* argv, void* user_data)
{
    if (entry == NULL || memory == NULL || memory_size == 0U || api == NULL || argc > TABOS_ELF_ARG_MAX ||
        (argc > 0U && argv == NULL) || memory_size > HOST_RV32_RAM_SIZE ||
        minimum_address > HOST_RV32_RAM_SIZE - memory_size) {
        return NULL;
    }

    const uintptr_t entry_offset = (uintptr_t) entry - (uintptr_t) memory;
    if (entry_offset >= memory_size) {
        return NULL;
    }

    platform_riscv32_context_t* context = calloc(1U, sizeof(*context));
    if (context == NULL) {
        return NULL;
    }
    context->memory = calloc(1U, HOST_RV32_RAM_SIZE);
    if (context->memory == NULL) {
        free(context);
        return NULL;
    }
    memcpy(context->memory + minimum_address, memory, memory_size);

    const uint32_t image_end   = minimum_address + (uint32_t) memory_size;
    const uint32_t api_address = (image_end + 15U) & ~15U;
    if (api_address > HOST_RV32_RAM_SIZE - HOST_RV32_API_SIZE) {
        platform_riscv32_destroy(context);
        return NULL;
    }
    write_u32(context->memory, api_address, api->abi_version);
    write_u32(context->memory, api_address + 4U, HOST_RV32_CONSOLE_WRITE);
    write_u32(context->memory, api_address + 8U, HOST_RV32_REQUEST_EXIT);
    write_u32(context->memory, api_address + 12U, HOST_RV32_CONSOLE_READ);
    write_u32(context->memory, api_address + 16U, HOST_RV32_CONSOLE_CLEAR);
    write_u32(context->memory, api_address + 20U, HOST_RV32_FS_GETCWD);
    write_u32(context->memory, api_address + 24U, HOST_RV32_FS_CHDIR);
    write_u32(context->memory, api_address + 28U, HOST_RV32_FS_LIST);
    write_u32(context->memory, api_address + 32U, HOST_RV32_EXEC);
    write_u32(context->memory, api_address + 36U, HOST_RV32_YIELD);
    write_u32(context->memory, api_address + 40U, HOST_RV32_CONSOLE_WRITE_RAW);
    write_u32(context->memory, api_address + 44U, HOST_RV32_FD_OPEN);
    write_u32(context->memory, api_address + 48U, HOST_RV32_FD_CLOSE);
    write_u32(context->memory, api_address + 52U, HOST_RV32_FD_READ);
    write_u32(context->memory, api_address + 56U, HOST_RV32_FD_WRITE);
    write_u32(context->memory, api_address + 60U, HOST_RV32_FD_SEEK);
    write_u32(context->memory, api_address + 64U, HOST_RV32_FS_STAT);
    write_u32(context->memory, api_address + 68U, HOST_RV32_FD_STAT);
    write_u32(context->memory, api_address + 72U, HOST_RV32_FS_MKDIR);
    write_u32(context->memory, api_address + 76U, HOST_RV32_FS_UNLINK);
    write_u32(context->memory, api_address + 80U, HOST_RV32_FS_RENAME);
    write_u32(context->memory, api_address + 84U, HOST_RV32_FD_GET_FLAGS);
    write_u32(context->memory, api_address + 88U, HOST_RV32_FD_SET_FLAGS);
    write_u32(context->memory, api_address + 92U, HOST_RV32_HEAP_SBRK);
    write_u32(context->memory, api_address + 96U, HOST_RV32_FS_RMDIR);
    write_u32(context->memory, api_address + 100U, HOST_RV32_MONOTONIC_MS);
    write_u32(context->memory, api_address + 104U, HOST_RV32_SYSTEM_INFO);
    write_u32(context->memory, api_address + 108U, HOST_RV32_GRAPHICS_OPEN);
    write_u32(context->memory, api_address + 112U, HOST_RV32_GRAPHICS_CLEAR);
    write_u32(context->memory, api_address + 116U, HOST_RV32_GRAPHICS_FILL_RECT);
    write_u32(context->memory, api_address + 120U, HOST_RV32_GRAPHICS_BLIT);
    write_u32(context->memory, api_address + 124U, HOST_RV32_GRAPHICS_PRESENT);
    write_u32(context->memory, api_address + 128U, HOST_RV32_GRAPHICS_CLOSE);
    write_u32(context->memory, api_address + 132U, HOST_RV32_GRAPHICS_CAPABILITIES);
    write_u32(context->memory, api_address + 136U, HOST_RV32_GRAPHICS_BLIT_EX);
    write_u32(context->memory, api_address + 140U, HOST_RV32_TTY_GET_MODE);
    write_u32(context->memory, api_address + 144U, HOST_RV32_TTY_SET_MODE);
    write_u32(context->memory, api_address + 148U, HOST_RV32_INPUT_POLL);

    uint32_t argument_data_address = api_address + HOST_RV32_API_SIZE;
    uint32_t argument_data_end     = argument_data_address;
    for (size_t index = 0U; index < argc; ++index) {
        if (argv[index] == NULL) {
            platform_riscv32_destroy(context);
            return NULL;
        }
        const size_t length = strlen(argv[index]) + 1U;
        const size_t used   = (size_t) (argument_data_end - argument_data_address);
        if (length > TABOS_ELF_ARG_BYTES_MAX - used || argument_data_end > HOST_RV32_RAM_SIZE - length) {
            platform_riscv32_destroy(context);
            return NULL;
        }
        memcpy(context->memory + argument_data_end, argv[index], length);
        argument_data_end += (uint32_t) length;
    }
    const uint32_t argv_address = (argument_data_end + 3U) & ~UINT32_C(3);
    if (argv_address > HOST_RV32_RAM_SIZE - ((uint32_t) argc + 1U) * 4U) {
        platform_riscv32_destroy(context);
        return NULL;
    }
    uint32_t next_argument = argument_data_address;
    for (size_t index = 0U; index < argc; ++index) {
        write_u32(context->memory, argv_address + (uint32_t) index * 4U, next_argument);
        next_argument += (uint32_t) strlen(argv[index]) + 1U;
    }
    write_u32(context->memory, argv_address + (uint32_t) argc * 4U, 0U);
    context->heap_base  = (argv_address + ((uint32_t) argc + 1U) * 4U + 15U) & ~15U;
    context->heap_break = context->heap_base;
    context->heap_end   = context->heap_base + 1024U * 1024U;
    if (context->heap_end > HOST_RV32_RAM_SIZE - 16U * 1024U) {
        platform_riscv32_destroy(context);
        return NULL;
    }

    context->api            = *api;
    context->user_data      = user_data;
    context->state.pc       = minimum_address + (uint32_t) entry_offset;
    context->state.regs[2]  = HOST_RV32_RAM_SIZE - 16U;
    context->state.regs[10] = api_address;
    context->state.regs[11] = (uint32_t) argc;
    context->state.regs[12] = argv_address;
    context->state.regs[1]  = HOST_RV32_RETURN;
    return context;
}

platform_riscv32_result_t platform_riscv32_step(platform_riscv32_context_t* context, unsigned int instruction_budget,
                                                int* returned_status)
{
    if (context == NULL || instruction_budget == 0U || returned_status == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    for (unsigned int count = 0U; count < instruction_budget; ++count) {
        if (context->state.pc == HOST_RV32_RETURN) {
            *returned_status = (int) context->state.regs[10];
            return PLATFORM_RISCV32_RETURNED;
        }
        if (context->state.pc == HOST_RV32_CONSOLE_WRITE) {
            const char* text = guest_string(context->memory, context->state.regs[10]);
            if (text == NULL || context->api.console_write == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->api.console_write(text);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_CONSOLE_WRITE_RAW) {
            const char* text = guest_string(context->memory, context->state.regs[10]);
            if (text == NULL || context->api.console_write_raw == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->api.console_write_raw(text);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_REQUEST_EXIT) {
            if (context->api.request_exit == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->api.request_exit((int) context->state.regs[10]);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_CONSOLE_READ || context->state.pc == HOST_RV32_FS_GETCWD) {
            const uint32_t capacity = context->state.regs[11];
            char* buffer            = guest_buffer(context->memory, context->state.regs[10], capacity);
            int (*operation)(char*, uint32_t) =
                context->state.pc == HOST_RV32_CONSOLE_READ ? context->api.console_read : context->api.fs_getcwd;
            if (buffer == NULL || operation == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) operation(buffer, capacity);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_CONSOLE_CLEAR) {
            if (context->api.console_clear == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.console_clear();
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_CHDIR) {
            const char* path = guest_string(context->memory, context->state.regs[10]);
            if (path == NULL || context->api.fs_chdir == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_chdir(path);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_EXEC) {
            const char* path            = guest_string(context->memory, context->state.regs[10]);
            const uint32_t argc         = context->state.regs[11];
            const uint32_t argv_address = context->state.regs[12];
            if (path == NULL || argc > TABOS_ELF_ARG_MAX || context->api.exec == NULL ||
                argv_address > HOST_RV32_RAM_SIZE - (argc + 1U) * 4U) {
                return PLATFORM_RISCV32_FAULT;
            }
            const char* arguments[TABOS_ELF_ARG_MAX + 1U];
            for (uint32_t index = 0U; index < argc; ++index) {
                const uint32_t offset = argv_address + index * 4U;
                const uint32_t address =
                    (uint32_t) context->memory[offset] | (uint32_t) context->memory[offset + 1U] << 8U |
                    (uint32_t) context->memory[offset + 2U] << 16U | (uint32_t) context->memory[offset + 3U] << 24U;
                arguments[index] = guest_string(context->memory, address);
                if (arguments[index] == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
            }
            arguments[argc]         = NULL;
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.exec(path, argc, arguments);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_LIST) {
            const char* path        = guest_string(context->memory, context->state.regs[10]);
            const uint32_t capacity = context->state.regs[12];
            char* buffer            = guest_buffer(context->memory, context->state.regs[11], capacity);
            if (path == NULL || buffer == NULL || context->api.fs_list == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_list(path, buffer, capacity);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_OPEN) {
            const char* path = guest_string(context->memory, context->state.regs[10]);
            if (path == NULL || context->api.fd_open == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) context->api.fd_open(path, (int) context->state.regs[11], context->state.regs[12]);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_CLOSE || context->state.pc == HOST_RV32_FD_GET_FLAGS) {
            int (*operation)(int) =
                context->state.pc == HOST_RV32_FD_CLOSE ? context->api.fd_close : context->api.fd_get_flags;
            if (operation == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) operation((int) context->state.regs[10]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_READ || context->state.pc == HOST_RV32_FD_WRITE) {
            const uint32_t count_bytes = context->state.regs[12];
            void* buffer               = guest_buffer(context->memory, context->state.regs[11], count_bytes);
            if (buffer == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) (context->state.pc == HOST_RV32_FD_READ ?
                                context->api.fd_read((int) context->state.regs[10], buffer, count_bytes) :
                                context->api.fd_write((int) context->state.regs[10], buffer, count_bytes));
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_SEEK) {
            int32_t* position = guest_buffer(context->memory, context->state.regs[13], 4U);
            if (position == NULL || context->api.fd_seek == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) context->api.fd_seek((int) context->state.regs[10], (int32_t) context->state.regs[11],
                                                (int) context->state.regs[12], position);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_STAT) {
            const char* path         = guest_string(context->memory, context->state.regs[10]);
            tabos_elf_stat_t* status = guest_buffer(context->memory, context->state.regs[11], sizeof(*status));
            if (path == NULL || status == NULL || context->api.fs_stat == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_stat(path, status);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_STAT) {
            tabos_elf_stat_t* status = guest_buffer(context->memory, context->state.regs[11], sizeof(*status));
            if (status == NULL || context->api.fd_stat == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fd_stat((int) context->state.regs[10], status);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_MKDIR) {
            const char* path = guest_string(context->memory, context->state.regs[10]);
            if (path == NULL || context->api.fs_mkdir == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_mkdir(path, context->state.regs[11]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_UNLINK) {
            const char* path = guest_string(context->memory, context->state.regs[10]);
            if (path == NULL || context->api.fs_unlink == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_unlink(path);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_RMDIR) {
            const char* path = guest_string(context->memory, context->state.regs[10]);
            if (path == NULL || context->api.fs_rmdir == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_rmdir(path);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FS_RENAME) {
            const char* old_path = guest_string(context->memory, context->state.regs[10]);
            const char* new_path = guest_string(context->memory, context->state.regs[11]);
            if (old_path == NULL || new_path == NULL || context->api.fs_rename == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.fs_rename(old_path, new_path);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_FD_SET_FLAGS) {
            if (context->api.fd_set_flags == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) context->api.fd_set_flags((int) context->state.regs[10], (int) context->state.regs[11]);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_TTY_GET_MODE) {
            if (context->api.tty_get_mode == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.tty_get_mode((int) context->state.regs[10]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_TTY_SET_MODE) {
            if (context->api.tty_set_mode == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) context->api.tty_set_mode((int) context->state.regs[10], context->state.regs[11]);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_INPUT_POLL) {
            tabos_input_event_t* event = guest_buffer(context->memory, context->state.regs[10], sizeof(*event));
            if (event == NULL || context->api.input_poll == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.input_poll(event);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_HEAP_SBRK) {
            const int32_t increment = (int32_t) context->state.regs[10];
            const uint32_t previous = context->heap_break;
            if (increment < 0 || (uint32_t) increment > context->heap_end - context->heap_break) {
                context->state.regs[10] = UINT32_MAX;
            } else {
                context->heap_break     += (uint32_t) increment;
                context->state.regs[10]  = previous;
            }
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_MONOTONIC_MS) {
            if (context->api.monotonic_ms == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            const uint64_t value    = context->api.monotonic_ms();
            context->state.regs[10] = (uint32_t) value;
            context->state.regs[11] = (uint32_t) (value >> 32U);
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_SYSTEM_INFO) {
            tabos_elf_system_info_t* info = guest_buffer(context->memory, context->state.regs[10], sizeof(*info));
            if (info == NULL || context->api.system_info == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            context->state.regs[10] = (uint32_t) context->api.system_info(info);
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_OPEN) {
            uint32_t* width  = guest_buffer(context->memory, context->state.regs[10], sizeof(*width));
            uint32_t* height = guest_buffer(context->memory, context->state.regs[11], sizeof(*height));
            if (width == NULL || height == NULL || context->api.graphics_open == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.graphics_open(width, height);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_CLEAR || context->state.pc == HOST_RV32_GRAPHICS_PRESENT ||
            context->state.pc == HOST_RV32_GRAPHICS_CLOSE) {
            const bool presented = context->state.pc == HOST_RV32_GRAPHICS_PRESENT;
            int result;
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_GRAPHICS_CLEAR && context->api.graphics_clear != NULL) {
                result = context->api.graphics_clear(context->state.regs[10]);
            } else if (context->state.pc == HOST_RV32_GRAPHICS_PRESENT && context->api.graphics_present != NULL) {
                result = context->api.graphics_present();
            } else if (context->state.pc == HOST_RV32_GRAPHICS_CLOSE && context->api.graphics_close != NULL) {
                result = context->api.graphics_close();
            } else {
                current_user_data = NULL;
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = NULL;
            context->state.regs[10] = (uint32_t) result;
            context->state.pc       = context->state.regs[1];
            if (presented) {
                return PLATFORM_RISCV32_YIELDED;
            }
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_FILL_RECT) {
            if (context->api.graphics_fill_rect == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.graphics_fill_rect(
                (int32_t) context->state.regs[10], (int32_t) context->state.regs[11], context->state.regs[12],
                context->state.regs[13], context->state.regs[14]);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_BLIT) {
            const uint32_t width = context->state.regs[12], height = context->state.regs[13];
            if (width != 0U && height > UINT32_MAX / width) {
                return PLATFORM_RISCV32_FAULT;
            }
            const uint32_t pixel_count = width * height;
            if (pixel_count > UINT32_MAX / sizeof(uint16_t)) {
                return PLATFORM_RISCV32_FAULT;
            }
            const uint16_t* pixels =
                guest_buffer(context->memory, context->state.regs[14], pixel_count * sizeof(uint16_t));
            if (pixels == NULL || context->api.graphics_blit == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.graphics_blit(
                (int32_t) context->state.regs[10], (int32_t) context->state.regs[11], width, height, pixels);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_CAPABILITIES) {
            if (context->api.graphics_capabilities == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = context->api.graphics_capabilities();
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_GRAPHICS_BLIT_EX) {
            const uint32_t address = context->state.regs[10];
            const uint8_t* guest   = guest_buffer(context->memory, address, 56U);
            if (guest == NULL || context->api.graphics_blit_ex == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            tabos_graphics_blit_options_t options = {
                .bitmap_width  = read_u32(guest, 4U),
                .bitmap_height = read_u32(guest, 8U),
                .source =
                    {
                             .x      = (int32_t) read_u32(guest, 12U),
                             .y      = (int32_t) read_u32(guest, 16U),
                             .width  = read_u32(guest, 20U),
                             .height = read_u32(guest, 24U),
                             },
                .destination =
                    {
                             .x      = (int32_t) read_u32(guest, 28U),
                             .y      = (int32_t) read_u32(guest, 32U),
                             .width  = read_u32(guest, 36U),
                             .height = read_u32(guest, 40U),
                             },
                .rotation          = (tabos_graphics_rotation_t) read_u32(guest, 44U),
                .mirror_x          = guest[48U] != 0U,
                .mirror_y          = guest[49U] != 0U,
                .opacity           = guest[50U],
                .color_key_enabled = guest[51U] != 0U,
                .color_key_low     = read_u16(guest, 52U),
                .color_key_high    = read_u16(guest, 54U),
            };
            if (options.bitmap_width != 0U && options.bitmap_height > UINT32_MAX / options.bitmap_width) {
                return PLATFORM_RISCV32_FAULT;
            }
            const uint32_t pixel_count = options.bitmap_width * options.bitmap_height;
            if (pixel_count > UINT32_MAX / sizeof(uint16_t)) {
                return PLATFORM_RISCV32_FAULT;
            }
            options.pixels = guest_buffer(context->memory, read_u32(guest, 0U), pixel_count * sizeof(uint16_t));
            if (options.pixels == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.graphics_blit_ex(&options);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_YIELD) {
            if (context->api.yield == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->api.yield();
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            return PLATFORM_RISCV32_YIELDED;
        }
        const uint32_t instruction_pc = context->state.pc;
        (void) MiniRV32IMAStep(&context->state, context->memory, 0U, 0U, 1);
        if (context->state.mcause != 0U) {
            char message[96];
            (void) snprintf(message, sizeof(message), "RV32 fault at PC 0x%08x, cause 0x%08x", instruction_pc,
                            context->state.mcause);
            platform_log(message);
            return PLATFORM_RISCV32_FAULT;
        }
    }
    return PLATFORM_RISCV32_YIELDED;
}

void* platform_riscv32_current_user_data(void)
{
    return current_user_data;
}

void platform_riscv32_destroy(platform_riscv32_context_t* context)
{
    if (context != NULL) {
        free(context->memory);
        free(context);
    }
}
