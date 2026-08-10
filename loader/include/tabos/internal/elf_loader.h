#ifndef TABOS_INTERNAL_ELF_LOADER_H
#define TABOS_INTERNAL_ELF_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TAB_ELF_OK = 0,
    TAB_ELF_INVALID_ARGUMENT,
    TAB_ELF_TRUNCATED,
    TAB_ELF_UNSUPPORTED_FORMAT,
    TAB_ELF_UNSUPPORTED_RELOCATION,
    TAB_ELF_INVALID_SEGMENT,
    TAB_ELF_INVALID_ENTRY,
    TAB_ELF_IMAGE_TOO_LARGE,
    TAB_ELF_NO_EXECUTABLE_MEMORY,
    TAB_ELF_PREPARE_FAILED,
} tab_elf_result_t;

typedef struct {
    uint32_t entry_address;
    uint32_t minimum_address;
    uint32_t maximum_address;
    size_t image_size;
    size_t load_segment_count;
} tab_elf_info_t;

typedef struct {
    void *memory;
    size_t memory_size;
    void *entry;
    tab_elf_info_t info;
} tab_elf_image_t;

tab_elf_result_t tab_elf_inspect(const uint8_t *data, size_t size, tab_elf_info_t *info);
tab_elf_result_t tab_elf_load(const uint8_t *data, size_t size, tab_elf_image_t *image);
void tab_elf_unload(tab_elf_image_t *image);
const char *tab_elf_result_name(tab_elf_result_t result);

#endif
