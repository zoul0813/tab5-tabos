#ifndef TABOS_INTERNAL_ELF_API_H
#define TABOS_INTERNAL_ELF_API_H

#include <tabos/graphics.h>
#include <tabos/input.h>

#include <stdint.h>

#define TABOS_ELF_API_VERSION  7U
#define TABOS_ELF_EXEC_PENDING (-2147483647 - 1)

enum {
    TABOS_ELF_ARG_MAX          = 16,
    TABOS_ELF_ARG_BYTES_MAX    = 512,
    TABOS_ELF_SYSTEM_REBOOT    = 1,
    TABOS_ELF_SYSTEM_POWER_OFF = 2,
};

typedef struct {
        uint32_t mode;
        uint32_t size_low;
        uint32_t size_high;
        int32_t modified_time_low;
        int32_t modified_time_high;
} tabos_elf_stat_t;

typedef struct {
        char target[16];
        char device[32];
        char display[32];
        uint32_t display_width;
        uint32_t display_height;
        uint32_t cpu_cores;
        uint32_t cpu_frequency_mhz;
        uint32_t memory_total_low;
        uint32_t memory_total_high;
        uint32_t external_memory_total_low;
        uint32_t external_memory_total_high;
} tabos_elf_system_info_t;

typedef struct {
        uint32_t seconds_low;
        int32_t seconds_high;
} tabos_elf_wall_time_t;

typedef struct {
        uint32_t state;
        char hostname[33];
        char ssid[33];
        char ipv4[16];
        int32_t signal_dbm;
        uint32_t attempts;
        uint32_t auto_connect;
        uint32_t saved_config;
        char last_failure[96];
} tabos_elf_network_status_t;

typedef struct {
        uint32_t abi_version;
        void (*console_write)(const char* text);
        void (*request_exit)(int exit_status);
        int (*console_read)(char* buffer, uint32_t capacity);
        int (*console_clear)(void);
        int (*fs_getcwd)(char* buffer, uint32_t capacity);
        int (*fs_chdir)(const char* path);
        int (*fs_list)(const char* path, char* buffer, uint32_t capacity);
        int (*exec)(const char* path, uint32_t argc, const char* const* argv);
        void (*yield)(void);
        void (*console_write_raw)(const char* text);
        int (*fd_open)(const char* path, int flags, uint32_t mode);
        int (*fd_close)(int descriptor);
        int (*fd_read)(int descriptor, void* buffer, uint32_t count);
        int (*fd_write)(int descriptor, const void* buffer, uint32_t count);
        int (*fd_seek)(int descriptor, int32_t offset, int whence, int32_t* position);
        int (*fs_stat)(const char* path, tabos_elf_stat_t* status);
        int (*fd_stat)(int descriptor, tabos_elf_stat_t* status);
        int (*fs_mkdir)(const char* path, uint32_t mode);
        int (*fs_unlink)(const char* path);
        int (*fs_rename)(const char* old_path, const char* new_path);
        int (*fd_get_flags)(int descriptor);
        int (*fd_set_flags)(int descriptor, int flags);
        void* (*heap_sbrk)(int32_t increment);
        int (*fs_rmdir)(const char* path);
        uint64_t (*monotonic_ms)(void);
        int (*system_info)(tabos_elf_system_info_t* info);
        int (*graphics_open)(uint32_t* width, uint32_t* height);
        int (*graphics_clear)(uint32_t color);
        int (*graphics_fill_rect)(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
        int (*graphics_blit)(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint16_t* pixels);
        int (*graphics_present)(void);
        int (*graphics_close)(void);
        uint32_t (*graphics_capabilities)(void);
        int (*graphics_blit_ex)(const tabos_graphics_blit_options_t* options);
        int (*tty_get_mode)(int descriptor);
        int (*tty_set_mode)(int descriptor, uint32_t mode);
        int (*input_poll)(tabos_input_event_t* event);
        int (*wall_time_get)(tabos_elf_wall_time_t* time);
        int (*wall_time_set)(const tabos_elf_wall_time_t* time);
        int (*system_action)(uint32_t action);
        int (*network_status)(tabos_elf_network_status_t* status);
        int (*network_connect_saved)(void);
        int (*network_disconnect)(void);
} tabos_elf_api_t;

typedef int (*tabos_elf_entry_fn)(const tabos_elf_api_t* api, int argc, const char* const* argv);

#endif
