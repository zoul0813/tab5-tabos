#ifndef TABOS_ELF_API_H
#define TABOS_ELF_API_H

#include <stdint.h>

#define TABOS_ELF_API_VERSION 1U

typedef struct {
    uint32_t abi_version;
    void (*console_write)(const char *text);
    void (*request_exit)(int exit_status);
} tabos_elf_api_t;

typedef int (*tabos_elf_entry_fn)(const tabos_elf_api_t *api);

#endif
