#ifndef TABOS_ELF_API_H
#define TABOS_ELF_API_H

#include <stdint.h>

#define TABOS_ELF_API_VERSION 3U
#define TABOS_ELF_EXEC_PENDING (-2147483647 - 1)

enum {
    TABOS_ELF_ARG_MAX = 16,
    TABOS_ELF_ARG_BYTES_MAX = 512,
};

typedef struct {
    uint32_t mode;
    uint32_t size_low;
    uint32_t size_high;
    int32_t modified_time_low;
    int32_t modified_time_high;
} tabos_elf_stat_t;

typedef struct {
    uint32_t abi_version;
    void (*console_write)(const char *text);
    void (*request_exit)(int exit_status);
    int (*console_read)(char *buffer, uint32_t capacity);
    int (*console_clear)(void);
    int (*fs_getcwd)(char *buffer, uint32_t capacity);
    int (*fs_chdir)(const char *path);
    int (*fs_list)(const char *path, char *buffer, uint32_t capacity);
    int (*exec)(const char *path, uint32_t argc, const char *const *argv);
    void (*yield)(void);
    void (*console_write_raw)(const char *text);
    int (*fd_open)(const char *path, int flags, uint32_t mode);
    int (*fd_close)(int descriptor);
    int (*fd_read)(int descriptor, void *buffer, uint32_t count);
    int (*fd_write)(int descriptor, const void *buffer, uint32_t count);
    int (*fd_seek)(int descriptor, int32_t offset, int whence, int32_t *position);
    int (*fs_stat)(const char *path, tabos_elf_stat_t *status);
    int (*fd_stat)(int descriptor, tabos_elf_stat_t *status);
    int (*fs_mkdir)(const char *path, uint32_t mode);
    int (*fs_unlink)(const char *path);
    int (*fs_rename)(const char *old_path, const char *new_path);
    int (*fd_get_flags)(int descriptor);
    int (*fd_set_flags)(int descriptor, int flags);
    void *(*heap_sbrk)(int32_t increment);
} tabos_elf_api_t;

typedef int (*tabos_elf_entry_fn)(const tabos_elf_api_t *api,
                                  int argc,
                                  const char *const *argv);

#endif
