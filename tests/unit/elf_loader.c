#include <tabos/internal/elf_loader.h>

#include "hello_elf.h"

#include <stdlib.h>
#include <string.h>

static void write_u32(uint8_t *value, uint32_t number)
{
    value[0] = (uint8_t)number;
    value[1] = (uint8_t)(number >> 8U);
    value[2] = (uint8_t)(number >> 16U);
    value[3] = (uint8_t)(number >> 24U);
}

static bool expect_mutation(size_t offset, uint32_t value, tab_elf_result_t expected)
{
    uint8_t *copy = malloc(tab_hello_elf_size);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, tab_hello_elf, tab_hello_elf_size);
    write_u32(copy + offset, value);
    tab_elf_info_t info;
    const tab_elf_result_t result = tab_elf_inspect(copy, tab_hello_elf_size, &info);
    free(copy);
    return result == expected;
}

int main(void)
{
    tab_elf_info_t info;
    if (tab_elf_inspect(tab_hello_elf, tab_hello_elf_size, &info) != TAB_ELF_OK ||
        info.entry_address != 0U || info.minimum_address != 0U ||
        info.maximum_address != 125U || info.image_size != 125U ||
        info.load_segment_count != 1U ||
        tab_elf_inspect(tab_hello_elf, 51U, &info) != TAB_ELF_TRUNCATED ||
        !expect_mutation(16U, 3U, TAB_ELF_UNSUPPORTED_FORMAT) ||
        !expect_mutation(68U, 126U, TAB_ELF_INVALID_SEGMENT) ||
        !expect_mutation(24U, 256U, TAB_ELF_INVALID_ENTRY) ||
        !expect_mutation(72U, (1024U * 1024U) + 1U, TAB_ELF_IMAGE_TOO_LARGE) ||
        !expect_mutation(272U, 9U, TAB_ELF_UNSUPPORTED_RELOCATION)) {
        return 1;
    }

    tab_elf_image_t image;
    if (tab_elf_load(tab_hello_elf, tab_hello_elf_size, &image) != TAB_ELF_OK ||
        image.memory == NULL || image.entry != image.memory || image.memory_size != 125U ||
        memcmp(image.memory, tab_hello_elf + 84U, 125U) != 0) {
        return 1;
    }
    tab_elf_unload(&image);
    if (image.memory != NULL || strcmp(tab_elf_result_name(TAB_ELF_OK), "OK") != 0) {
        return 1;
    }
    return 0;
}
