#ifndef TABOS_ELF_API_H
#define TABOS_ELF_API_H

#include <stdint.h>

#define TABOS_ELF_API_VERSION 1U
#define TABOS_ELF_EXEC_PENDING (-2147483647 - 1)

typedef struct {
    uint32_t abi_version;
    void (*console_write)(const char *text);
    void (*request_exit)(int exit_status);
    int (*console_read)(char *buffer, uint32_t capacity);
    int (*console_clear)(void);
    int (*fs_getcwd)(char *buffer, uint32_t capacity);
    int (*fs_chdir)(const char *path);
    int (*fs_list)(const char *path, char *buffer, uint32_t capacity);
    int (*exec)(const char *path);
    void (*yield)(void);
    void (*console_write_raw)(const char *text);
} tabos_elf_api_t;

typedef int (*tabos_elf_entry_fn)(const tabos_elf_api_t *api);

#endif
