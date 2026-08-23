#include <tabos/internal/elf_loader.h>

#include "hello_elf.h"

#include <stdlib.h>
#include <string.h>

static void write_u32(uint8_t* value, uint32_t number)
{
    value[0] = (uint8_t) number;
    value[1] = (uint8_t) (number >> 8U);
    value[2] = (uint8_t) (number >> 16U);
    value[3] = (uint8_t) (number >> 24U);
}

static bool expect_mutation(size_t offset, uint32_t value, loader_elf_result_t expected)
{
    uint8_t* copy = malloc(loader_hello_elf_size);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, loader_hello_elf, loader_hello_elf_size);
    write_u32(copy + offset, value);
    loader_elf_info_t info;
    const loader_elf_result_t result = loader_elf_inspect(copy, loader_hello_elf_size, &info);
    free(copy);
    return result == expected;
}

int main(void)
{
    loader_elf_info_t info;
    if (loader_elf_inspect(loader_hello_elf, loader_hello_elf_size, &info) != LOADER_ELF_OK ||
        info.entry_address != 0U || info.minimum_address != 0U || info.maximum_address != 259U ||
        info.image_size != 259U || info.load_segment_count != 1U ||
        loader_elf_inspect(loader_hello_elf, 51U, &info) != LOADER_ELF_TRUNCATED ||
        !expect_mutation(16U, 3U, LOADER_ELF_UNSUPPORTED_FORMAT) ||
        !expect_mutation(68U, 260U, LOADER_ELF_INVALID_SEGMENT) ||
        !expect_mutation(24U, 260U, LOADER_ELF_INVALID_ENTRY) ||
        !expect_mutation(72U, (1024U * 1024U) + 1U, LOADER_ELF_IMAGE_TOO_LARGE) ||
        !expect_mutation(656U, 9U, LOADER_ELF_UNSUPPORTED_RELOCATION)) {
        return 1;
    }

    loader_elf_image_t image;
    if (loader_elf_load(loader_hello_elf, loader_hello_elf_size, &image) != LOADER_ELF_OK || image.memory == NULL ||
        image.entry != image.memory || image.memory_size != 259U ||
        memcmp(image.memory, loader_hello_elf + 84U, 259U) != 0) {
        return 1;
    }
    loader_elf_unload(&image);
    if (image.memory != NULL || strcmp(loader_elf_result_name(LOADER_ELF_OK), "OK") != 0) {
        return 1;
    }
    return 0;
}
