#include <tabos/platform/platform.h>
#include <tabos/wait.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HOST_RV32_MAX_RAM_SIZE = 24 * 1024 * 1024,
    HOST_RV32_GUARD_BYTES  = 4 * 1024,
};

static _Thread_local uint32_t host_rv32_active_ram_size;

#define HOST_RV32_NO_API_OFFSET UINT32_MAX
#define HOST_RV32_API_GATE_BASE UINT32_C(0xffff0000)

/* Append new ABI gates. List order defines their contiguous trap addresses. */
#define HOST_RV32_API_GATES(X)               \
    X(CONSOLE_WRITE, 4U)                     \
    X(REQUEST_EXIT, 8U)                      \
    X(RETURN, HOST_RV32_NO_API_OFFSET)       \
    X(CONSOLE_READ, 12U)                     \
    X(CONSOLE_CLEAR, 16U)                    \
    X(FS_GETCWD, 20U)                        \
    X(FS_CHDIR, 24U)                         \
    X(FS_LIST, 28U)                          \
    X(EXEC, 32U)                             \
    X(YIELD, 36U)                            \
    X(CONSOLE_WRITE_RAW, 40U)                \
    X(FD_OPEN, 44U)                          \
    X(FD_CLOSE, 48U)                         \
    X(FD_READ, 52U)                          \
    X(FD_WRITE, 56U)                         \
    X(FD_SEEK, 60U)                          \
    X(FS_STAT, 64U)                          \
    X(FD_STAT, 68U)                          \
    X(FS_MKDIR, 72U)                         \
    X(FS_UNLINK, 76U)                        \
    X(FS_RENAME, 80U)                        \
    X(FD_GET_FLAGS, 84U)                     \
    X(FD_SET_FLAGS, 88U)                     \
    X(HEAP_SBRK, 92U)                        \
    X(FS_RMDIR, 96U)                         \
    X(MONOTONIC_MS, 100U)                    \
    X(SYSTEM_INFO, 104U)                     \
    X(GRAPHICS_OPEN, 108U)                   \
    X(GRAPHICS_CLEAR, 112U)                  \
    X(GRAPHICS_FILL_RECT, 116U)              \
    X(GRAPHICS_BLIT, 120U)                   \
    X(GRAPHICS_PRESENT, 124U)                \
    X(GRAPHICS_CLOSE, 128U)                  \
    X(GRAPHICS_CAPABILITIES, 132U)           \
    X(GRAPHICS_BLIT_EX, 136U)                \
    X(TTY_GET_MODE, 140U)                    \
    X(TTY_SET_MODE, 144U)                    \
    X(INPUT_POLL, 148U)                      \
    X(WALL_TIME_GET, 152U)                   \
    X(WALL_TIME_SET, 156U)                   \
    X(SYSTEM_ACTION, 160U)                   \
    X(NETWORK_STATUS, 164U)                  \
    X(NETWORK_CONNECT_SAVED, 168U)           \
    X(NETWORK_DISCONNECT, 172U)              \
    X(NETWORK_RESOLVE, 176U)                 \
    X(NETWORK_ECHO, 180U)                    \
    X(SOCKET_OPEN, 184U)                     \
    X(SOCKET_CLOSE, 188U)                    \
    X(SOCKET_BIND, 192U)                     \
    X(SOCKET_LISTEN, 196U)                   \
    X(SOCKET_ACCEPT, 200U)                   \
    X(SOCKET_CONNECT, 204U)                  \
    X(SOCKET_NONBLOCKING, 208U)              \
    X(SOCKET_SHUTDOWN, 212U)                 \
    X(SOCKET_SEND, 216U)                     \
    X(SOCKET_RECEIVE, 220U)                  \
    X(SOCKET_SEND_TO, 224U)                  \
    X(SOCKET_RECEIVE_FROM, 228U)             \
    X(SOCKET_LOCAL_ENDPOINT, 232U)           \
    X(GRAPHICS_SET_OVERLAYS, 248U)           \
    X(TLS_CONNECT, 252U)                     \
    X(TLS_CLOSE, 256U)                       \
    X(TLS_SEND, 260U)                        \
    X(TLS_RECEIVE, 264U)                     \
    X(DEVICE_COUNT, 268U)                    \
    X(DEVICE_AT, 272U)                       \
    X(DEVICE_GET, 276U)                      \
    X(DEVICE_FIND, 280U)                     \
    X(DEVICE_SUBSCRIBE, 284U)                \
    X(DEVICE_CLOSE, 288U)                    \
    X(DEVICE_EVENT_READ, 292U)               \
    X(SOCKET_WAIT_SOURCE, 296U)              \
    X(DEVICE_SUBSCRIPTION_WAIT_SOURCE, 300U) \
    X(WAIT, 304U)

enum {
#define HOST_RV32_GATE_INDEX(name, api_offset) HOST_RV32_GATE_INDEX_##name,
    HOST_RV32_API_GATES(HOST_RV32_GATE_INDEX)
#undef HOST_RV32_GATE_INDEX
    HOST_RV32_API_GATE_COUNT,
};

#define HOST_RV32_GATE_VALUE(name, api_offset) \
    static const uint32_t HOST_RV32_##name =   \
        HOST_RV32_API_GATE_BASE + (uint32_t) HOST_RV32_GATE_INDEX_##name * sizeof(uint32_t);
HOST_RV32_API_GATES(HOST_RV32_GATE_VALUE)
#undef HOST_RV32_GATE_VALUE

#define HOST_RV32_API_GATE_FIRST HOST_RV32_API_GATE_BASE
#define HOST_RV32_API_GATE_LAST \
    (HOST_RV32_API_GATE_BASE + ((uint32_t) HOST_RV32_API_GATE_COUNT - 1U) * sizeof(uint32_t))
#define MINI_RV32_RAM_SIZE        host_rv32_active_ram_size
#define MINIRV32_RAM_IMAGE_OFFSET 0U
#define MINIRV32_POSTEXEC(pc, ir, trap)                                                                      \
    do {                                                                                                     \
        const uint32_t host_rv32_next_pc = (pc) + 4U;                                                        \
        if (host_rv32_next_pc >= HOST_RV32_API_GATE_FIRST && host_rv32_next_pc <= HOST_RV32_API_GATE_LAST) { \
            icount = count - 1;                                                                              \
        }                                                                                                    \
    } while (0)
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

typedef struct {
        uint32_t gate;
        uint32_t api_offset;
} host_rv32_api_gate_t;

#define HOST_RV32_GATE_DESCRIPTOR(name, offset) \
    {.gate = HOST_RV32_API_GATE_BASE + (uint32_t) HOST_RV32_GATE_INDEX_##name * sizeof(uint32_t), .api_offset = offset},
static const host_rv32_api_gate_t host_rv32_api_gates[] = {HOST_RV32_API_GATES(HOST_RV32_GATE_DESCRIPTOR)};
#undef HOST_RV32_GATE_DESCRIPTOR

struct platform_riscv32_context {
        uint8_t* memory;
        uint32_t memory_size;
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

static size_t host_rv32_api_size(void)
{
    uint32_t last_offset = 0U;
    for (size_t index = 0U; index < sizeof(host_rv32_api_gates) / sizeof(host_rv32_api_gates[0]); ++index) {
        const uint32_t offset = host_rv32_api_gates[index].api_offset;
        if (offset != HOST_RV32_NO_API_OFFSET && offset > last_offset) {
            last_offset = offset;
        }
    }
    return (size_t) last_offset + sizeof(uint32_t);
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
    if (address >= host_rv32_active_ram_size ||
        memchr(memory + address, '\0', host_rv32_active_ram_size - address) == NULL) {
        return NULL;
    }
    return (const char*) memory + address;
}

static void* guest_buffer(uint8_t* memory, uint32_t address, uint32_t capacity)
{
    return address <= host_rv32_active_ram_size && capacity <= host_rv32_active_ram_size - address ? memory + address :
                                                                                                     NULL;
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
                                                    uint32_t minimum_address, size_t heap_bytes, size_t stack_bytes,
                                                    const tabos_elf_api_t* api, size_t argc, const char* const* argv,
                                                    void* user_data)
{
    if (entry == NULL || memory == NULL || memory_size == 0U || api == NULL || argc > TABOS_ELF_ARG_MAX ||
        (argc > 0U && argv == NULL) || heap_bytes == 0U || stack_bytes == 0U || memory_size > UINT32_MAX ||
        minimum_address > UINT32_MAX - memory_size) {
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
    const size_t image_end   = (size_t) minimum_address + memory_size;
    const size_t api_address = (image_end + 15U) & ~((size_t) 15U);
    const size_t api_size    = host_rv32_api_size();
    if (api_address > HOST_RV32_MAX_RAM_SIZE || api_size > HOST_RV32_MAX_RAM_SIZE - api_address) {
        free(context);
        return NULL;
    }
    const size_t argument_data_address = api_address + api_size;
    size_t argument_data_end           = argument_data_address;
    for (size_t index = 0U; index < argc; ++index) {
        if (argv[index] == NULL) {
            free(context);
            return NULL;
        }
        const size_t length = strlen(argv[index]) + 1U;
        const size_t used   = argument_data_end - argument_data_address;
        if (used > TABOS_ELF_ARG_BYTES_MAX || length > TABOS_ELF_ARG_BYTES_MAX - used ||
            argument_data_end > HOST_RV32_MAX_RAM_SIZE - length) {
            free(context);
            return NULL;
        }
        argument_data_end += length;
    }
    const size_t argv_address = (argument_data_end + 3U) & ~((size_t) 3U);
    const size_t argv_size    = (argc + 1U) * sizeof(uint32_t);
    if (argv_size > HOST_RV32_MAX_RAM_SIZE || argv_address > HOST_RV32_MAX_RAM_SIZE - argv_size) {
        free(context);
        return NULL;
    }
    const size_t heap_base  = (argv_address + argv_size + 15U) & ~((size_t) 15U);
    const size_t heap_end   = heap_base + heap_bytes;
    const size_t stack_base = heap_end + HOST_RV32_GUARD_BYTES;
    const size_t ram_size   = stack_base + stack_bytes;
    if (heap_end < heap_base || stack_base < heap_end || ram_size < stack_base || ram_size > HOST_RV32_MAX_RAM_SIZE ||
        ram_size > UINT32_MAX) {
        free(context);
        return NULL;
    }

    context->memory = calloc(1U, ram_size);
    if (context->memory == NULL) {
        free(context);
        return NULL;
    }
    context->memory_size = (uint32_t) ram_size;
    memcpy(context->memory + minimum_address, memory, memory_size);
    host_rv32_active_ram_size        = context->memory_size;
    const uint32_t guest_api_address = (uint32_t) api_address;
    write_u32(context->memory, guest_api_address, api->abi_version);
    for (size_t index = 0U; index < sizeof(host_rv32_api_gates) / sizeof(host_rv32_api_gates[0]); ++index) {
        const host_rv32_api_gate_t* gate = &host_rv32_api_gates[index];
        if (gate->api_offset != HOST_RV32_NO_API_OFFSET) {
            write_u32(context->memory, guest_api_address + gate->api_offset, gate->gate);
        }
    }

    size_t next_argument = argument_data_address;
    for (size_t index = 0U; index < argc; ++index) {
        const size_t length = strlen(argv[index]) + 1U;
        memcpy(context->memory + next_argument, argv[index], length);
        write_u32(context->memory, (uint32_t) argv_address + (uint32_t) index * 4U, (uint32_t) next_argument);
        next_argument += length;
    }
    write_u32(context->memory, (uint32_t) argv_address + (uint32_t) argc * 4U, 0U);
    context->heap_base  = (uint32_t) heap_base;
    context->heap_break = (uint32_t) heap_base;
    context->heap_end   = (uint32_t) heap_end;

    context->api            = *api;
    context->user_data      = user_data;
    context->state.pc       = minimum_address + (uint32_t) entry_offset;
    context->state.regs[2]  = context->memory_size - 16U;
    context->state.regs[10] = guest_api_address;
    context->state.regs[11] = (uint32_t) argc;
    context->state.regs[12] = (uint32_t) argv_address;
    context->state.regs[1]  = HOST_RV32_RETURN;
    return context;
}

platform_riscv32_result_t platform_riscv32_step(platform_riscv32_context_t* context, unsigned int instruction_budget,
                                                int* returned_status)
{
    if (context == NULL || instruction_budget == 0U || returned_status == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    host_rv32_active_ram_size = context->memory_size;
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
                argv_address > host_rv32_active_ram_size - (argc + 1U) * 4U) {
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
        if (context->state.pc == HOST_RV32_WALL_TIME_GET) {
            tabos_elf_wall_time_t* time = guest_buffer(context->memory, context->state.regs[10], sizeof(*time));
            if (time == NULL || context->api.wall_time_get == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.wall_time_get(time);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_WALL_TIME_SET) {
            const tabos_elf_wall_time_t* time = guest_buffer(context->memory, context->state.regs[10], sizeof(*time));
            if (time == NULL || context->api.wall_time_set == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.wall_time_set(time);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_SYSTEM_ACTION) {
            if (context->api.system_action == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.system_action(context->state.regs[10]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_NETWORK_STATUS) {
            tabos_elf_network_status_t* status =
                guest_buffer(context->memory, context->state.regs[10], sizeof(*status));
            if (status == NULL || context->api.network_status == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.network_status(status);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_NETWORK_CONNECT_SAVED) {
            if (context->api.network_connect_saved == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.network_connect_saved();
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_NETWORK_DISCONNECT) {
            if (context->api.network_disconnect == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.network_disconnect();
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_NETWORK_RESOLVE) {
            const char* hostname = guest_string(context->memory, context->state.regs[10]);
            tabos_elf_network_address_t* address =
                guest_buffer(context->memory, context->state.regs[12], sizeof(*address));
            if (hostname == NULL || address == NULL || context->api.network_resolve == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            context->state.regs[10] =
                (uint32_t) context->api.network_resolve(hostname, context->state.regs[11], address);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_NETWORK_ECHO) {
            const tabos_elf_network_address_t* address =
                guest_buffer(context->memory, context->state.regs[10], sizeof(*address));
            tabos_elf_network_echo_result_t* echo_result =
                guest_buffer(context->memory, context->state.regs[14], sizeof(*echo_result));
            if (address == NULL || echo_result == NULL || context->api.network_echo == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.network_echo(
                address, context->state.regs[11], context->state.regs[12], context->state.regs[13], echo_result);
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_SOCKET_OPEN || context->state.pc == HOST_RV32_SOCKET_CLOSE ||
            context->state.pc == HOST_RV32_SOCKET_LISTEN || context->state.pc == HOST_RV32_SOCKET_NONBLOCKING ||
            context->state.pc == HOST_RV32_SOCKET_SHUTDOWN) {
            int (*call)(int, uint32_t) = NULL;
            if (context->state.pc == HOST_RV32_SOCKET_LISTEN) {
                call = context->api.socket_listen;
            } else if (context->state.pc == HOST_RV32_SOCKET_NONBLOCKING) {
                call = context->api.socket_set_nonblocking;
            } else if (context->state.pc == HOST_RV32_SOCKET_SHUTDOWN) {
                call = context->api.socket_shutdown;
            }
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_SOCKET_OPEN) {
                if (context->api.socket_open == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] =
                    (uint32_t) context->api.socket_open(context->state.regs[10], context->state.regs[11]);
            } else if (context->state.pc == HOST_RV32_SOCKET_CLOSE) {
                if (context->api.socket_close == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] = (uint32_t) context->api.socket_close((int) context->state.regs[10]);
            } else {
                if (call == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] = (uint32_t) call((int) context->state.regs[10], context->state.regs[11]);
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_SOCKET_BIND || context->state.pc == HOST_RV32_SOCKET_CONNECT ||
            context->state.pc == HOST_RV32_SOCKET_ACCEPT || context->state.pc == HOST_RV32_SOCKET_LOCAL_ENDPOINT) {
            tabos_elf_socket_endpoint_t* endpoint = NULL;
            if (context->state.regs[11] != 0U) {
                endpoint = guest_buffer(context->memory, context->state.regs[11], sizeof(*endpoint));
                if (endpoint == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
            }
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_SOCKET_BIND && context->api.socket_bind != NULL) {
                context->state.regs[10] = (uint32_t) context->api.socket_bind((int) context->state.regs[10], endpoint);
            } else if (context->state.pc == HOST_RV32_SOCKET_CONNECT && context->api.socket_connect != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.socket_connect((int) context->state.regs[10], endpoint);
            } else if (context->state.pc == HOST_RV32_SOCKET_ACCEPT && context->api.socket_accept != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.socket_accept((int) context->state.regs[10], endpoint);
            } else if (context->state.pc == HOST_RV32_SOCKET_LOCAL_ENDPOINT &&
                       context->api.socket_get_local_endpoint != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.socket_get_local_endpoint((int) context->state.regs[10], endpoint);
            } else {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_SOCKET_SEND || context->state.pc == HOST_RV32_SOCKET_RECEIVE ||
            context->state.pc == HOST_RV32_SOCKET_SEND_TO || context->state.pc == HOST_RV32_SOCKET_RECEIVE_FROM) {
            void* data = guest_buffer(context->memory, context->state.regs[11], context->state.regs[12]);
            tabos_elf_socket_endpoint_t* endpoint = NULL;
            if ((context->state.pc == HOST_RV32_SOCKET_SEND_TO || context->state.pc == HOST_RV32_SOCKET_RECEIVE_FROM) &&
                context->state.regs[13] != 0U) {
                endpoint = guest_buffer(context->memory, context->state.regs[13], sizeof(*endpoint));
            }
            if (data == NULL || ((context->state.pc == HOST_RV32_SOCKET_SEND_TO) && endpoint == NULL)) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_SOCKET_SEND && context->api.socket_send != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.socket_send((int) context->state.regs[10], data, context->state.regs[12]);
            } else if (context->state.pc == HOST_RV32_SOCKET_RECEIVE && context->api.socket_receive != NULL) {
                context->state.regs[10] = (uint32_t) context->api.socket_receive((int) context->state.regs[10], data,
                                                                                 context->state.regs[12]);
            } else if (context->state.pc == HOST_RV32_SOCKET_SEND_TO && context->api.socket_send_to != NULL) {
                context->state.regs[10] = (uint32_t) context->api.socket_send_to((int) context->state.regs[10], data,
                                                                                 context->state.regs[12], endpoint);
            } else if (context->state.pc == HOST_RV32_SOCKET_RECEIVE_FROM && context->api.socket_receive_from != NULL) {
                context->state.regs[10] = (uint32_t) context->api.socket_receive_from(
                    (int) context->state.regs[10], data, context->state.regs[12], endpoint);
            } else {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_WAIT) {
            const uint32_t item_count = context->state.regs[11];
            if (item_count == 0U || item_count > TABOS_WAIT_MAX) {
                return PLATFORM_RISCV32_FAULT;
            }
            tabos_elf_wait_item_t* items =
                guest_buffer(context->memory, context->state.regs[10], item_count * sizeof(*items));
            if (items == NULL || context->api.wait == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.wait(items, item_count, context->state.regs[12]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_TLS_CONNECT || context->state.pc == HOST_RV32_TLS_CLOSE ||
            context->state.pc == HOST_RV32_TLS_SEND || context->state.pc == HOST_RV32_TLS_RECEIVE) {
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_TLS_CONNECT) {
                const char* hostname = guest_string(context->memory, context->state.regs[10]);
                if (hostname == NULL || context->api.tls_connect == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] = (uint32_t) context->api.tls_connect(hostname, context->state.regs[11]);
            } else if (context->state.pc == HOST_RV32_TLS_CLOSE) {
                if (context->api.tls_close == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] = (uint32_t) context->api.tls_close((int) context->state.regs[10]);
            } else {
                void* data = guest_buffer(context->memory, context->state.regs[11], context->state.regs[12]);
                int (*call)(int, void*, uint32_t) = context->state.pc == HOST_RV32_TLS_SEND ?
                                                        (int (*)(int, void*, uint32_t)) context->api.tls_send :
                                                        context->api.tls_receive;
                if (data == NULL || call == NULL) {
                    return PLATFORM_RISCV32_FAULT;
                }
                context->state.regs[10] = (uint32_t) call((int) context->state.regs[10], data, context->state.regs[12]);
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_DEVICE_COUNT || context->state.pc == HOST_RV32_DEVICE_SUBSCRIBE ||
            context->state.pc == HOST_RV32_DEVICE_CLOSE || context->state.pc == HOST_RV32_SOCKET_WAIT_SOURCE ||
            context->state.pc == HOST_RV32_DEVICE_SUBSCRIPTION_WAIT_SOURCE) {
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_DEVICE_COUNT && context->api.device_count != NULL) {
                context->state.regs[10] = context->api.device_count();
            } else if (context->state.pc == HOST_RV32_DEVICE_SUBSCRIBE && context->api.device_subscribe != NULL) {
                context->state.regs[10] = (uint32_t) context->api.device_subscribe();
            } else if (context->state.pc == HOST_RV32_DEVICE_CLOSE && context->api.device_subscription_close != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.device_subscription_close((int) context->state.regs[10]);
            } else if (context->state.pc == HOST_RV32_SOCKET_WAIT_SOURCE && context->api.socket_wait_source != NULL) {
                context->state.regs[10] = (uint32_t) context->api.socket_wait_source((int) context->state.regs[10]);
            } else if (context->state.pc == HOST_RV32_DEVICE_SUBSCRIPTION_WAIT_SOURCE &&
                       context->api.device_subscription_wait_source != NULL) {
                context->state.regs[10] =
                    (uint32_t) context->api.device_subscription_wait_source((int) context->state.regs[10]);
            } else {
                current_user_data = NULL;
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_DEVICE_AT || context->state.pc == HOST_RV32_DEVICE_GET) {
            tabos_device_info_t* info = guest_buffer(context->memory, context->state.regs[11], sizeof(*info));
            if (info == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = context->user_data;
            if (context->state.pc == HOST_RV32_DEVICE_AT && context->api.device_at != NULL) {
                context->state.regs[10] = (uint32_t) context->api.device_at(context->state.regs[10], info);
            } else if (context->state.pc == HOST_RV32_DEVICE_GET && context->api.device_get != NULL) {
                context->state.regs[10] = (uint32_t) context->api.device_get(context->state.regs[10], info);
            } else {
                current_user_data = NULL;
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data = NULL;
            context->state.pc = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_DEVICE_FIND) {
            const char* name          = guest_string(context->memory, context->state.regs[10]);
            tabos_device_info_t* info = guest_buffer(context->memory, context->state.regs[11], sizeof(*info));
            if (name == NULL || info == NULL || context->api.device_find == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.device_find(name, info);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
            continue;
        }
        if (context->state.pc == HOST_RV32_DEVICE_EVENT_READ) {
            tabos_device_event_t* event = guest_buffer(context->memory, context->state.regs[11], sizeof(*event));
            if (event == NULL || context->api.device_event_read == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.device_event_read((int) context->state.regs[10], event);
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
        if (context->state.pc == HOST_RV32_GRAPHICS_SET_OVERLAYS) {
            if (context->api.graphics_set_overlays == NULL) {
                return PLATFORM_RISCV32_FAULT;
            }
            current_user_data       = context->user_data;
            context->state.regs[10] = (uint32_t) context->api.graphics_set_overlays(context->state.regs[10]);
            current_user_data       = NULL;
            context->state.pc       = context->state.regs[1];
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
        const uint64_t cycle_before         = ((uint64_t) context->state.cycleh << 32U) | context->state.cyclel;
        const unsigned int remaining_budget = instruction_budget - count;
        int batch_budget                    = INT_MAX;
        if (remaining_budget <= (unsigned int) INT_MAX) {
            batch_budget = (int) remaining_budget;
        }
        (void) MiniRV32IMAStep(&context->state, context->memory, 0U, 0U, batch_budget);
        const uint64_t cycle_after = ((uint64_t) context->state.cycleh << 32U) | context->state.cyclel;
        const uint64_t executed    = cycle_after - cycle_before;
        if (context->state.mcause != 0U) {
            char message[96];
            (void) snprintf(message, sizeof(message), "RV32 fault at PC 0x%08x, cause 0x%08x", context->state.mepc,
                            context->state.mcause);
            platform_log(message);
            return PLATFORM_RISCV32_FAULT;
        }
        if (executed > 0U) {
            count += (unsigned int) executed - 1U;
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
