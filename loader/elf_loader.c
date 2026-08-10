#include <tabos/internal/elf_loader.h>

#include <tabos/platform/platform.h>

#include <limits.h>
#include <string.h>

enum {
    ELF_HEADER_SIZE = 52,
    ELF_PROGRAM_HEADER_SIZE = 32,
    ELF_SECTION_HEADER_SIZE = 40,
    ELF_TYPE_EXECUTABLE = 2,
    ELF_MACHINE_RISCV = 243,
    ELF_PROGRAM_LOAD = 1,
    ELF_PROGRAM_DYNAMIC = 2,
    ELF_PROGRAM_EXECUTE = 1,
    ELF_SECTION_RELA = 4,
    ELF_SECTION_REL = 9,
    ELF_MAX_IMAGE_SIZE = 1024 * 1024,
};

static uint16_t read_u16(const uint8_t *value)
{
    return (uint16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8U));
}

static uint32_t read_u32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8U) |
        ((uint32_t)value[2] << 16U) | ((uint32_t)value[3] << 24U);
}

static bool range_valid(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static bool table_valid(uint32_t offset, uint16_t count, uint16_t entry_size, size_t total)
{
    return count == 0U ||
        (entry_size != 0U && count <= SIZE_MAX / entry_size &&
         range_valid(offset, (size_t)count * entry_size, total));
}

static tab_elf_result_t inspect_sections(const uint8_t *data, size_t size)
{
    const uint32_t section_offset = read_u32(data + 32U);
    const uint16_t section_entry_size = read_u16(data + 46U);
    const uint16_t section_count = read_u16(data + 48U);
    if (!table_valid(section_offset, section_count, section_entry_size, size) ||
        (section_count > 0U && section_entry_size != ELF_SECTION_HEADER_SIZE)) {
        return TAB_ELF_TRUNCATED;
    }
    for (uint16_t index = 0U; index < section_count; ++index) {
        const uint8_t *section = data + section_offset + ((size_t)index * section_entry_size);
        const uint32_t type = read_u32(section + 4U);
        const uint32_t section_size = read_u32(section + 20U);
        if ((type == ELF_SECTION_REL || type == ELF_SECTION_RELA) && section_size > 0U) {
            return TAB_ELF_UNSUPPORTED_RELOCATION;
        }
    }
    return TAB_ELF_OK;
}

tab_elf_result_t tab_elf_inspect(const uint8_t *data, size_t size, tab_elf_info_t *info)
{
    if (data == NULL || info == NULL) {
        return TAB_ELF_INVALID_ARGUMENT;
    }
    *info = (tab_elf_info_t){0};
    if (size < ELF_HEADER_SIZE) {
        return TAB_ELF_TRUNCATED;
    }
    if (data[0] != 0x7fU || data[1] != 'E' || data[2] != 'L' || data[3] != 'F' ||
        data[4] != 1U || data[5] != 1U || data[6] != 1U ||
        read_u16(data + 16U) != ELF_TYPE_EXECUTABLE ||
        read_u16(data + 18U) != ELF_MACHINE_RISCV || read_u32(data + 20U) != 1U ||
        read_u16(data + 40U) != ELF_HEADER_SIZE) {
        return TAB_ELF_UNSUPPORTED_FORMAT;
    }

    const uint32_t program_offset = read_u32(data + 28U);
    const uint16_t program_entry_size = read_u16(data + 42U);
    const uint16_t program_count = read_u16(data + 44U);
    if (program_count == 0U || program_entry_size != ELF_PROGRAM_HEADER_SIZE ||
        !table_valid(program_offset, program_count, program_entry_size, size)) {
        return TAB_ELF_TRUNCATED;
    }
    const tab_elf_result_t section_result = inspect_sections(data, size);
    if (section_result != TAB_ELF_OK) {
        return section_result;
    }

    uint32_t minimum = UINT32_MAX;
    uint32_t maximum = 0U;
    bool executable_entry = false;
    const uint32_t entry = read_u32(data + 24U);
    for (uint16_t index = 0U; index < program_count; ++index) {
        const uint8_t *program = data + program_offset + ((size_t)index * program_entry_size);
        const uint32_t type = read_u32(program);
        if (type == ELF_PROGRAM_DYNAMIC) {
            return TAB_ELF_UNSUPPORTED_RELOCATION;
        }
        if (type != ELF_PROGRAM_LOAD) {
            continue;
        }
        const uint32_t file_offset = read_u32(program + 4U);
        const uint32_t virtual_address = read_u32(program + 8U);
        const uint32_t file_size = read_u32(program + 16U);
        const uint32_t memory_size = read_u32(program + 20U);
        const uint32_t flags = read_u32(program + 24U);
        const uint32_t alignment = read_u32(program + 28U);
        if (file_size > memory_size || !range_valid(file_offset, file_size, size) ||
            memory_size == 0U || virtual_address > UINT32_MAX - memory_size ||
            (alignment > 1U && ((alignment & (alignment - 1U)) != 0U ||
                               (file_offset % alignment) != (virtual_address % alignment)))) {
            return TAB_ELF_INVALID_SEGMENT;
        }
        const uint32_t end = virtual_address + memory_size;
        if (virtual_address < minimum) {
            minimum = virtual_address;
        }
        if (end > maximum) {
            maximum = end;
        }
        if ((flags & ELF_PROGRAM_EXECUTE) != 0U && entry >= virtual_address && entry < end) {
            executable_entry = true;
        }
        ++info->load_segment_count;
    }
    if (info->load_segment_count == 0U || minimum == UINT32_MAX || maximum <= minimum) {
        return TAB_ELF_INVALID_SEGMENT;
    }
    if (!executable_entry) {
        return TAB_ELF_INVALID_ENTRY;
    }
    const uint32_t image_size = maximum - minimum;
    if (image_size > ELF_MAX_IMAGE_SIZE) {
        return TAB_ELF_IMAGE_TOO_LARGE;
    }
    *info = (tab_elf_info_t){
        .entry_address = entry,
        .minimum_address = minimum,
        .maximum_address = maximum,
        .image_size = image_size,
        .load_segment_count = info->load_segment_count,
    };
    return TAB_ELF_OK;
}

tab_elf_result_t tab_elf_load(const uint8_t *data, size_t size, tab_elf_image_t *image)
{
    if (image == NULL) {
        return TAB_ELF_INVALID_ARGUMENT;
    }
    *image = (tab_elf_image_t){0};
    tab_elf_info_t info;
    const tab_elf_result_t result = tab_elf_inspect(data, size, &info);
    if (result != TAB_ELF_OK) {
        return result;
    }

    void *memory = tab_platform_executable_alloc(info.image_size);
    if (memory == NULL) {
        return TAB_ELF_NO_EXECUTABLE_MEMORY;
    }
    memset(memory, 0, info.image_size);

    const uint32_t program_offset = read_u32(data + 28U);
    const uint16_t program_entry_size = read_u16(data + 42U);
    const uint16_t program_count = read_u16(data + 44U);
    for (uint16_t index = 0U; index < program_count; ++index) {
        const uint8_t *program = data + program_offset + ((size_t)index * program_entry_size);
        if (read_u32(program) != ELF_PROGRAM_LOAD) {
            continue;
        }
        const uint32_t file_offset = read_u32(program + 4U);
        const uint32_t virtual_address = read_u32(program + 8U);
        const uint32_t file_size = read_u32(program + 16U);
        memcpy((uint8_t *)memory + (virtual_address - info.minimum_address),
               data + file_offset, file_size);
    }
    void *executable_memory = tab_platform_executable_prepare(memory, info.image_size);
    if (executable_memory == NULL) {
        tab_platform_executable_free(memory);
        return TAB_ELF_PREPARE_FAILED;
    }

    *image = (tab_elf_image_t){
        .memory = executable_memory,
        .memory_size = info.image_size,
        .entry = (uint8_t *)executable_memory +
            (info.entry_address - info.minimum_address),
        .info = info,
    };
    return TAB_ELF_OK;
}

void tab_elf_unload(tab_elf_image_t *image)
{
    if (image != NULL) {
        tab_platform_executable_free(image->memory);
        *image = (tab_elf_image_t){0};
    }
}

const char *tab_elf_result_name(tab_elf_result_t result)
{
    static const char *const names[] = {
        "OK", "INVALID ARGUMENT", "TRUNCATED", "UNSUPPORTED FORMAT",
        "UNSUPPORTED RELOCATION", "INVALID SEGMENT", "INVALID ENTRY",
        "IMAGE TOO LARGE", "NO EXECUTABLE MEMORY", "PREPARE FAILED",
    };
    return (unsigned int)result < (sizeof(names) / sizeof(names[0]))
        ? names[result]
        : "UNKNOWN";
}
