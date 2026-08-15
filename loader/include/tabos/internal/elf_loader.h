#ifndef TABOS_INTERNAL_ELF_LOADER_H
#define TABOS_INTERNAL_ELF_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LOADER_ELF_OK = 0,
    LOADER_ELF_INVALID_ARGUMENT,
    LOADER_ELF_TRUNCATED,
    LOADER_ELF_UNSUPPORTED_FORMAT,
    LOADER_ELF_UNSUPPORTED_RELOCATION,
    LOADER_ELF_INVALID_SEGMENT,
    LOADER_ELF_INVALID_ENTRY,
    LOADER_ELF_IMAGE_TOO_LARGE,
    LOADER_ELF_NO_EXECUTABLE_MEMORY,
    LOADER_ELF_PREPARE_FAILED,
    LOADER_ELF_FILE_OPEN_FAILED,
    LOADER_ELF_FILE_INVALID,
    LOADER_ELF_FILE_TOO_LARGE,
    LOADER_ELF_NO_FILE_MEMORY,
    LOADER_ELF_FILE_READ_FAILED,
} loader_elf_result_t;

typedef struct {
    uint32_t entry_address;
    uint32_t minimum_address;
    uint32_t maximum_address;
    size_t image_size;
    size_t load_segment_count;
} loader_elf_info_t;

typedef struct {
    void *memory;
    size_t memory_size;
    void *entry;
    loader_elf_info_t info;
} loader_elf_image_t;

loader_elf_result_t loader_elf_inspect(const uint8_t *data, size_t size, loader_elf_info_t *info);
loader_elf_result_t loader_elf_load(const uint8_t *data, size_t size, loader_elf_image_t *image);
loader_elf_result_t loader_elf_load_file(const char *path, loader_elf_image_t *image);
void loader_elf_unload(loader_elf_image_t *image);
const char *loader_elf_result_name(loader_elf_result_t result);

#endif
