#include <tabos/internal/elf_loader.h>

#include <tabos/application.h>

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

static void write_u16(uint8_t* value, uint16_t number)
{
    value[0] = (uint8_t) number;
    value[1] = (uint8_t) (number >> 8U);
}

static void write_metadata_section(uint8_t* section, uint32_t offset, uint32_t size)
{
    write_u32(section + 4U, 7U);
    write_u32(section + 16U, offset);
    write_u32(section + 20U, size);
    write_u32(section + 32U, 4U);
}

static void build_metadata_elf(uint8_t* data, uint32_t note_size, uint32_t note_type, uint32_t descriptor_version,
                               uint32_t descriptor_size, uint32_t heap_bytes, uint32_t stack_bytes,
                               uint32_t capabilities, uint32_t reserved, bool duplicate)
{
    memset(data, 0, 384U);
    data[0] = 0x7fU;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 1U;
    data[5] = 1U;
    data[6] = 1U;
    write_u16(data + 16U, 2U);
    write_u16(data + 18U, 243U);
    write_u32(data + 20U, 1U);
    write_u32(data + 28U, 52U);
    write_u32(data + 32U, 256U);
    write_u16(data + 40U, 52U);
    write_u16(data + 42U, 32U);
    write_u16(data + 44U, 1U);
    write_u16(data + 46U, 40U);
    write_u16(data + 48U, duplicate ? 3U : 2U);

    write_u32(data + 52U, 1U);
    write_u32(data + 56U, 200U);
    write_u32(data + 68U, 4U);
    write_u32(data + 72U, 4U);
    write_u32(data + 76U, 5U);
    write_u32(data + 80U, 4U);
    data[200] = 0x13U;

    write_u32(data + 84U, 6U);
    write_u32(data + 88U, 32U);
    write_u32(data + 92U, note_type);
    memcpy(data + 96U, "TABOS", 5U);
    write_u32(data + 104U, descriptor_version);
    write_u32(data + 108U, descriptor_size);
    write_u32(data + 112U, TABOS_APPLICATION_ABI_VERSION);
    write_u32(data + 116U, heap_bytes);
    write_u32(data + 120U, stack_bytes);
    write_u32(data + 124U, capabilities);
    write_u32(data + 128U, reserved);

    write_metadata_section(data + 296U, 84U, note_size);
    if (duplicate) {
        memcpy(data + 140U, data + 84U, 52U);
        write_metadata_section(data + 336U, 140U, note_size);
    }
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
    uint8_t metadata_elf[384];
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    loader_elf_info_t metadata_info;
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_OK ||
        !metadata_info.metadata_present || metadata_info.application_abi_version != TABOS_APPLICATION_ABI_VERSION ||
        metadata_info.requested_heap_bytes != 512U * 1024U || metadata_info.requested_stack_bytes != 64U * 1024U ||
        metadata_info.capabilities != TABOS_APP_CAPABILITY_CONSOLE ||
        loader_elf_inspect(loader_hello_elf, loader_hello_elf_size, &metadata_info) != LOADER_ELF_OK ||
        metadata_info.metadata_present || metadata_info.requested_heap_bytes != LOADER_ELF_DEFAULT_HEAP_BYTES ||
        metadata_info.requested_stack_bytes != LOADER_ELF_DEFAULT_STACK_BYTES ||
        metadata_info.capabilities != TABOS_APP_CAPABILITY_CONSOLE) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 11U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_TRUNCATED) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 2U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 2U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 513U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 31U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U + 1U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE | 2U, 0U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 1U,
                       false);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }
    build_metadata_elf(metadata_elf, 52U, 1U, 1U, 32U, 512U * 1024U, 64U * 1024U, TABOS_APP_CAPABILITY_CONSOLE, 0U,
                       true);
    if (loader_elf_inspect(metadata_elf, sizeof(metadata_elf), &metadata_info) != LOADER_ELF_UNSUPPORTED_FORMAT) {
        return 1;
    }

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
