#include <tabos/internal/elf_loader.h>

#include <tabos/application.h>
#include <tabos/filesystem.h>
#include <tabos/platform/platform.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    ELF_HEADER_SIZE             = 52,
    ELF_PROGRAM_HEADER_SIZE     = 32,
    ELF_SECTION_HEADER_SIZE     = 40,
    ELF_TYPE_EXECUTABLE         = 2,
    ELF_MACHINE_RISCV           = 243,
    ELF_PROGRAM_LOAD            = 1,
    ELF_PROGRAM_DYNAMIC         = 2,
    ELF_PROGRAM_EXECUTE         = 1,
    ELF_SECTION_SYMBOL_TABLE    = 2,
    ELF_SECTION_NOTE            = 7,
    ELF_SECTION_RELA            = 4,
    ELF_SECTION_REL             = 9,
    ELF_RELOCATION_NONE         = 0,
    ELF_RELOCATION_32           = 1,
    ELF_RELOCATION_BRANCH       = 16,
    ELF_RELOCATION_JAL          = 17,
    ELF_RELOCATION_CALL         = 18,
    ELF_RELOCATION_CALL_PLT     = 19,
    ELF_RELOCATION_PCREL_HI20   = 23,
    ELF_RELOCATION_PCREL_LO12_I = 24,
    ELF_RELOCATION_PCREL_LO12_S = 25,
    ELF_RELOCATION_HI20         = 26,
    ELF_RELOCATION_LO12_I       = 27,
    ELF_RELOCATION_LO12_S       = 28,
    ELF_RELOCATION_RELAX        = 51,
    ELF_RELA_ENTRY_SIZE         = 12,
    ELF_SYMBOL_ENTRY_SIZE       = 16,
    ELF_SECTION_ALLOCATED       = 2,
    ELF_SYMBOL_UNDEFINED        = 0,
    ELF_SYMBOL_ABSOLUTE         = 0xfff1,
    ELF_NOTE_HEADER_SIZE        = 12,
    ELF_NOTE_TYPE_TABOS         = 1,
    ELF_NOTE_NAME_SIZE          = 6,
    ELF_NOTE_DESC_PADDED_SIZE   = 32,
    ELF_MAX_IMAGE_SIZE          = 1024 * 1024,
    ELF_MAX_FILE_SIZE           = 2 * 1024 * 1024,
};

static uint16_t read_u16(const uint8_t* value)
{
    return (uint16_t) ((uint16_t) value[0] | ((uint16_t) value[1] << 8U));
}

static uint32_t read_u32(const uint8_t* value)
{
    return (uint32_t) value[0] | ((uint32_t) value[1] << 8U) | ((uint32_t) value[2] << 16U) |
           ((uint32_t) value[3] << 24U);
}

static int32_t read_i32(const uint8_t* value)
{
    return (int32_t) read_u32(value);
}

static void write_u32(uint8_t* value, uint32_t number)
{
    value[0] = (uint8_t) number;
    value[1] = (uint8_t) (number >> 8U);
    value[2] = (uint8_t) (number >> 16U);
    value[3] = (uint8_t) (number >> 24U);
}

static bool range_valid(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static bool table_valid(uint32_t offset, uint16_t count, uint16_t entry_size, size_t total)
{
    return count == 0U || (entry_size != 0U && count <= SIZE_MAX / entry_size &&
                           range_valid(offset, (size_t) count * entry_size, total));
}

static bool aligned_four(uint32_t value, size_t* result)
{
    if (result == NULL || (size_t) value > SIZE_MAX - 3U) {
        return false;
    }
    *result = ((size_t) value + 3U) & ~((size_t) 3U);
    return true;
}

static bool metadata_name_matches(const uint8_t* name, uint32_t size)
{
    static const uint8_t expected[] = {'T', 'A', 'B', 'O', 'S', '\0'};
    return size == ELF_NOTE_NAME_SIZE && memcmp(name, expected, sizeof(expected)) == 0;
}

static loader_elf_result_t inspect_metadata(const uint8_t* data, size_t size, loader_elf_info_t* info)
{
    const uint32_t section_offset     = read_u32(data + 32U);
    const uint16_t section_entry_size = read_u16(data + 46U);
    const uint16_t section_count      = read_u16(data + 48U);
    bool found                        = false;

    info->application_abi_version = TABOS_APPLICATION_ABI_VERSION;
    info->requested_heap_bytes    = LOADER_ELF_DEFAULT_HEAP_BYTES;
    info->requested_stack_bytes   = LOADER_ELF_DEFAULT_STACK_BYTES;
    info->capabilities            = TABOS_APP_CAPABILITY_CONSOLE;
    info->metadata_present        = false;

    for (uint16_t section_index = 0U; section_index < section_count; ++section_index) {
        const uint8_t* section = data + section_offset + ((size_t) section_index * section_entry_size);
        if (read_u32(section + 4U) != ELF_SECTION_NOTE) {
            continue;
        }
        const uint32_t note_offset = read_u32(section + 16U);
        const uint32_t note_size   = read_u32(section + 20U);
        if (!range_valid(note_offset, note_size, size)) {
            return LOADER_ELF_TRUNCATED;
        }
        size_t cursor = 0U;
        while (cursor < note_size) {
            if (note_size - cursor < ELF_NOTE_HEADER_SIZE) {
                return LOADER_ELF_TRUNCATED;
            }
            const uint8_t* note      = data + note_offset + cursor;
            const uint32_t name_size = read_u32(note);
            const uint32_t desc_size = read_u32(note + 4U);
            const uint32_t note_type = read_u32(note + 8U);
            size_t name_padded;
            size_t desc_padded;
            if (!aligned_four(name_size, &name_padded) || !aligned_four(desc_size, &desc_padded)) {
                return LOADER_ELF_TRUNCATED;
            }
            const size_t payload_size = name_padded + desc_padded;
            if (payload_size < name_padded || payload_size > (size_t) note_size - cursor - ELF_NOTE_HEADER_SIZE) {
                return LOADER_ELF_TRUNCATED;
            }
            const uint8_t* name = note + ELF_NOTE_HEADER_SIZE;
            if (metadata_name_matches(name, name_size)) {
                if (found || note_type != ELF_NOTE_TYPE_TABOS || desc_size != ELF_NOTE_DESC_PADDED_SIZE) {
                    return LOADER_ELF_UNSUPPORTED_FORMAT;
                }
                const uint8_t* descriptor = name + name_padded;
                if (read_u32(descriptor) != LOADER_ELF_METADATA_VERSION ||
                    read_u32(descriptor + 4U) != LOADER_ELF_METADATA_DESCRIPTOR_SIZE ||
                    read_u32(descriptor + 8U) != TABOS_APPLICATION_ABI_VERSION || read_u32(descriptor + 24U) != 0U ||
                    read_u32(descriptor + 28U) != 0U) {
                    return LOADER_ELF_UNSUPPORTED_FORMAT;
                }
                const uint32_t heap_bytes     = read_u32(descriptor + 12U);
                const uint32_t stack_bytes    = read_u32(descriptor + 16U);
                const uint32_t capabilities   = read_u32(descriptor + 20U);
                const uint32_t supported_caps = TABOS_APP_CAPABILITY_CONSOLE;
                if (heap_bytes < LOADER_ELF_MIN_HEAP_BYTES || heap_bytes > LOADER_ELF_MAX_HEAP_BYTES ||
                    (heap_bytes % 4096U) != 0U || stack_bytes < LOADER_ELF_MIN_STACK_BYTES ||
                    stack_bytes > LOADER_ELF_MAX_STACK_BYTES || (stack_bytes % 16U) != 0U ||
                    (capabilities & ~supported_caps) != 0U) {
                    return LOADER_ELF_UNSUPPORTED_FORMAT;
                }
                info->application_abi_version = read_u32(descriptor + 8U);
                info->requested_heap_bytes    = heap_bytes;
                info->requested_stack_bytes   = stack_bytes;
                info->capabilities            = capabilities;
                info->metadata_present        = true;
                found                         = true;
            }
            cursor += ELF_NOTE_HEADER_SIZE + payload_size;
        }
    }
    return LOADER_ELF_OK;
}

static loader_elf_result_t inspect_sections(const uint8_t* data, size_t size)
{
    const uint32_t section_offset     = read_u32(data + 32U);
    const uint16_t section_entry_size = read_u16(data + 46U);
    const uint16_t section_count      = read_u16(data + 48U);
    if (!table_valid(section_offset, section_count, section_entry_size, size) ||
        (section_count > 0U && section_entry_size != ELF_SECTION_HEADER_SIZE)) {
        return LOADER_ELF_TRUNCATED;
    }
    for (uint16_t index = 0U; index < section_count; ++index) {
        const uint8_t* section      = data + section_offset + ((size_t) index * section_entry_size);
        const uint32_t type         = read_u32(section + 4U);
        const uint32_t file_offset  = read_u32(section + 16U);
        const uint32_t section_size = read_u32(section + 20U);
        const uint32_t entry_size   = read_u32(section + 36U);
        if (type == ELF_SECTION_REL && section_size > 0U) {
            return LOADER_ELF_UNSUPPORTED_RELOCATION;
        }
        if (type == ELF_SECTION_RELA && (entry_size != ELF_RELA_ENTRY_SIZE || section_size % entry_size != 0U ||
                                         !range_valid(file_offset, section_size, size))) {
            return LOADER_ELF_TRUNCATED;
        }
    }
    return LOADER_ELF_OK;
}

static loader_elf_result_t apply_relocations(const uint8_t* data, size_t size, const loader_elf_info_t* info,
                                             uint8_t* writable_memory, const void* executable_memory)
{
    const uint32_t section_offset     = read_u32(data + 32U);
    const uint16_t section_entry_size = read_u16(data + 46U);
    const uint16_t section_count      = read_u16(data + 48U);
    const void* translated            = platform_executable_data_pointer(executable_memory, 1U);
    const uint32_t load_bias =
        translated == executable_memory ? 0U : (uint32_t) (uintptr_t) executable_memory - info->minimum_address;
    if (load_bias == 0U) {
        return LOADER_ELF_OK;
    }
    bool found_relocations = false;

    for (uint16_t section_index = 0U; section_index < section_count; ++section_index) {
        const uint8_t* section = data + section_offset + (size_t) section_index * section_entry_size;
        if (read_u32(section + 4U) != ELF_SECTION_RELA) {
            continue;
        }
        const uint32_t relocation_offset    = read_u32(section + 16U);
        const uint32_t relocation_size      = read_u32(section + 20U);
        const uint32_t symbol_section_index = read_u32(section + 24U);
        const uint32_t target_section_index = read_u32(section + 28U);
        if (target_section_index >= section_count) {
            return LOADER_ELF_UNSUPPORTED_RELOCATION;
        }
        const uint8_t* target_section = data + section_offset + (size_t) target_section_index * section_entry_size;
        if ((read_u32(target_section + 8U) & ELF_SECTION_ALLOCATED) == 0U) {
            continue;
        }
        found_relocations = true;
        if (symbol_section_index >= section_count) {
            return LOADER_ELF_UNSUPPORTED_RELOCATION;
        }
        const uint8_t* symbol_section = data + section_offset + (size_t) symbol_section_index * section_entry_size;
        if (read_u32(symbol_section + 4U) != ELF_SECTION_SYMBOL_TABLE ||
            read_u32(symbol_section + 36U) != ELF_SYMBOL_ENTRY_SIZE) {
            return LOADER_ELF_UNSUPPORTED_RELOCATION;
        }
        const uint32_t symbol_offset = read_u32(symbol_section + 16U);
        const uint32_t symbol_size   = read_u32(symbol_section + 20U);
        if (symbol_size == 0U || symbol_size % ELF_SYMBOL_ENTRY_SIZE != 0U ||
            !range_valid(symbol_offset, symbol_size, size)) {
            return LOADER_ELF_TRUNCATED;
        }

        for (uint32_t used = 0U; used < relocation_size; used += ELF_RELA_ENTRY_SIZE) {
            const uint8_t* relocation      = data + relocation_offset + used;
            const uint32_t target_address  = read_u32(relocation);
            const uint32_t relocation_info = read_u32(relocation + 4U);
            const uint32_t type            = relocation_info & 0xffU;
            const uint32_t symbol_index    = relocation_info >> 8U;
            const int32_t addend           = read_i32(relocation + 8U);
            if (type == ELF_RELOCATION_NONE || type == ELF_RELOCATION_BRANCH || type == ELF_RELOCATION_JAL ||
                type == ELF_RELOCATION_CALL || type == ELF_RELOCATION_CALL_PLT || type == ELF_RELOCATION_PCREL_HI20 ||
                type == ELF_RELOCATION_PCREL_LO12_I || type == ELF_RELOCATION_PCREL_LO12_S ||
                type == ELF_RELOCATION_RELAX) {
                continue;
            }
            if (type != ELF_RELOCATION_32 && type != ELF_RELOCATION_HI20 && type != ELF_RELOCATION_LO12_I &&
                type != ELF_RELOCATION_LO12_S) {
                return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
            if (target_address < info->minimum_address || target_address > info->maximum_address - sizeof(uint32_t)) {
                return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
            if (symbol_index >= symbol_size / ELF_SYMBOL_ENTRY_SIZE) {
                return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
            const uint8_t* symbol       = data + symbol_offset + (size_t) symbol_index * ELF_SYMBOL_ENTRY_SIZE;
            const uint32_t symbol_value = read_u32(symbol + 4U);
            const uint16_t symbol_section_index_value = read_u16(symbol + 14U);
            if (symbol_section_index_value == ELF_SYMBOL_UNDEFINED && type != ELF_RELOCATION_NONE) {
                return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
            if (symbol_section_index_value >= section_count && symbol_section_index_value != ELF_SYMBOL_ABSOLUTE) {
                return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
            uint8_t* target            = writable_memory + (target_address - info->minimum_address);
            const uint32_t symbol_bias = symbol_section_index_value == ELF_SYMBOL_ABSOLUTE ? 0U : load_bias;
            const uint32_t value       = symbol_bias + symbol_value + (uint32_t) addend;
            uint32_t instruction       = read_u32(target);

            switch (type) {
                case ELF_RELOCATION_32: write_u32(target, value); break;
                case ELF_RELOCATION_HI20:
                    instruction =
                        (instruction & UINT32_C(0x00000fff)) | ((value + UINT32_C(0x800)) & UINT32_C(0xfffff000));
                    write_u32(target, instruction);
                    break;
                case ELF_RELOCATION_LO12_I:
                    instruction = (instruction & UINT32_C(0x000fffff)) | ((value & UINT32_C(0x00000fff)) << 20U);
                    write_u32(target, instruction);
                    break;
                case ELF_RELOCATION_LO12_S:
                    instruction &= ~UINT32_C(0xfe000f80);
                    instruction |= (value & UINT32_C(0x00000fe0)) << 20U;
                    instruction |= (value & UINT32_C(0x0000001f)) << 7U;
                    write_u32(target, instruction);
                    break;
                case ELF_RELOCATION_NONE:
                case ELF_RELOCATION_BRANCH:
                case ELF_RELOCATION_JAL:
                case ELF_RELOCATION_CALL:
                case ELF_RELOCATION_CALL_PLT:
                case ELF_RELOCATION_PCREL_HI20:
                case ELF_RELOCATION_PCREL_LO12_I:
                case ELF_RELOCATION_PCREL_LO12_S:
                case ELF_RELOCATION_RELAX: break;
                default: return LOADER_ELF_UNSUPPORTED_RELOCATION;
            }
        }
    }
    return found_relocations ? LOADER_ELF_OK : LOADER_ELF_UNSUPPORTED_RELOCATION;
}

loader_elf_result_t loader_elf_inspect(const uint8_t* data, size_t size, loader_elf_info_t* info)
{
    if (data == NULL || info == NULL) {
        return LOADER_ELF_INVALID_ARGUMENT;
    }
    *info = (loader_elf_info_t) {0};
    if (size < ELF_HEADER_SIZE) {
        return LOADER_ELF_TRUNCATED;
    }
    if (data[0] != 0x7fU || data[1] != 'E' || data[2] != 'L' || data[3] != 'F' || data[4] != 1U || data[5] != 1U ||
        data[6] != 1U || read_u16(data + 16U) != ELF_TYPE_EXECUTABLE || read_u16(data + 18U) != ELF_MACHINE_RISCV ||
        read_u32(data + 20U) != 1U || read_u16(data + 40U) != ELF_HEADER_SIZE) {
        return LOADER_ELF_UNSUPPORTED_FORMAT;
    }

    const uint32_t program_offset     = read_u32(data + 28U);
    const uint16_t program_entry_size = read_u16(data + 42U);
    const uint16_t program_count      = read_u16(data + 44U);
    if (program_count == 0U || program_entry_size != ELF_PROGRAM_HEADER_SIZE ||
        !table_valid(program_offset, program_count, program_entry_size, size)) {
        return LOADER_ELF_TRUNCATED;
    }
    const loader_elf_result_t section_result = inspect_sections(data, size);
    if (section_result != LOADER_ELF_OK) {
        return section_result;
    }
    const loader_elf_result_t metadata_result = inspect_metadata(data, size, info);
    if (metadata_result != LOADER_ELF_OK) {
        return metadata_result;
    }

    uint32_t minimum      = UINT32_MAX;
    uint32_t maximum      = 0U;
    bool executable_entry = false;
    const uint32_t entry  = read_u32(data + 24U);
    for (uint16_t index = 0U; index < program_count; ++index) {
        const uint8_t* program = data + program_offset + ((size_t) index * program_entry_size);
        const uint32_t type    = read_u32(program);
        if (type == ELF_PROGRAM_DYNAMIC) {
            return LOADER_ELF_UNSUPPORTED_RELOCATION;
        }
        if (type != ELF_PROGRAM_LOAD) {
            continue;
        }
        const uint32_t file_offset     = read_u32(program + 4U);
        const uint32_t virtual_address = read_u32(program + 8U);
        const uint32_t file_size       = read_u32(program + 16U);
        const uint32_t memory_size     = read_u32(program + 20U);
        const uint32_t flags           = read_u32(program + 24U);
        const uint32_t alignment       = read_u32(program + 28U);
        if (file_size > memory_size || !range_valid(file_offset, file_size, size) || memory_size == 0U ||
            virtual_address > UINT32_MAX - memory_size ||
            (alignment > 1U &&
             ((alignment & (alignment - 1U)) != 0U || (file_offset % alignment) != (virtual_address % alignment)))) {
            return LOADER_ELF_INVALID_SEGMENT;
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
        return LOADER_ELF_INVALID_SEGMENT;
    }
    if (!executable_entry) {
        return LOADER_ELF_INVALID_ENTRY;
    }
    const uint32_t image_size = maximum - minimum;
    if (image_size > ELF_MAX_IMAGE_SIZE) {
        return LOADER_ELF_IMAGE_TOO_LARGE;
    }
    *info = (loader_elf_info_t) {
        .entry_address           = entry,
        .minimum_address         = minimum,
        .maximum_address         = maximum,
        .image_size              = image_size,
        .load_segment_count      = info->load_segment_count,
        .application_abi_version = info->application_abi_version,
        .requested_heap_bytes    = info->requested_heap_bytes,
        .requested_stack_bytes   = info->requested_stack_bytes,
        .capabilities            = info->capabilities,
        .metadata_present        = info->metadata_present,
    };
    return LOADER_ELF_OK;
}

loader_elf_result_t loader_elf_load(const uint8_t* data, size_t size, loader_elf_image_t* image)
{
    if (image == NULL) {
        return LOADER_ELF_INVALID_ARGUMENT;
    }
    *image = (loader_elf_image_t) {0};
    loader_elf_info_t info;
    const loader_elf_result_t result = loader_elf_inspect(data, size, &info);
    if (result != LOADER_ELF_OK) {
        return result;
    }

    void* memory = platform_executable_alloc(info.image_size);
    if (memory == NULL) {
        return LOADER_ELF_NO_EXECUTABLE_MEMORY;
    }
    memset(memory, 0, info.image_size);

    const uint32_t program_offset     = read_u32(data + 28U);
    const uint16_t program_entry_size = read_u16(data + 42U);
    const uint16_t program_count      = read_u16(data + 44U);
    for (uint16_t index = 0U; index < program_count; ++index) {
        const uint8_t* program = data + program_offset + ((size_t) index * program_entry_size);
        if (read_u32(program) != ELF_PROGRAM_LOAD) {
            continue;
        }
        const uint32_t file_offset     = read_u32(program + 4U);
        const uint32_t virtual_address = read_u32(program + 8U);
        const uint32_t file_size       = read_u32(program + 16U);
        memcpy((uint8_t*) memory + (virtual_address - info.minimum_address), data + file_offset, file_size);
    }
    void* executable_memory = platform_executable_prepare(memory, info.image_size);
    if (executable_memory == NULL) {
        platform_executable_free(memory);
        return LOADER_ELF_PREPARE_FAILED;
    }

    const loader_elf_result_t relocation_result = apply_relocations(data, size, &info, memory, executable_memory);
    if (relocation_result != LOADER_ELF_OK || !platform_executable_finalize(executable_memory, info.image_size)) {
        platform_executable_free(memory);
        return relocation_result != LOADER_ELF_OK ? relocation_result : LOADER_ELF_PREPARE_FAILED;
    }

    *image = (loader_elf_image_t) {
        .memory      = executable_memory,
        .memory_size = info.image_size,
        .entry       = (uint8_t*) executable_memory + (info.entry_address - info.minimum_address),
        .info        = info,
    };
    return LOADER_ELF_OK;
}

loader_elf_result_t loader_elf_load_file(const char* path, loader_elf_image_t* image)
{
    if (path == NULL || path[0] == '\0' || image == NULL) {
        return LOADER_ELF_INVALID_ARGUMENT;
    }
    *image = (loader_elf_image_t) {0};

    const tabos_fd_t file = tabos_fs_open(path, TABOS_O_RDONLY, 0U);
    if (file < 0) {
        return LOADER_ELF_FILE_OPEN_FAILED;
    }

    tabos_stat_t status;
    if (tabos_fs_fstat(file, &status) != 0 || (status.mode & TABOS_S_IFREG) == 0U || status.size == 0U ||
        status.size > SIZE_MAX) {
        (void) tabos_fs_close(file);
        return LOADER_ELF_FILE_INVALID;
    }
    if (status.size > ELF_MAX_FILE_SIZE) {
        (void) tabos_fs_close(file);
        return LOADER_ELF_FILE_TOO_LARGE;
    }

    const size_t size = (size_t) status.size;
    uint8_t* data     = malloc(size);
    if (data == NULL) {
        (void) tabos_fs_close(file);
        return LOADER_ELF_NO_FILE_MEMORY;
    }

    size_t used = 0U;
    while (used < size) {
        const tabos_ssize_t count = tabos_fs_read(file, data + used, size - used);
        if (count <= 0 || (size_t) count > size - used) {
            free(data);
            (void) tabos_fs_close(file);
            return LOADER_ELF_FILE_READ_FAILED;
        }
        used += (size_t) count;
    }
    if (tabos_fs_close(file) != 0) {
        free(data);
        return LOADER_ELF_FILE_READ_FAILED;
    }

    const loader_elf_result_t result = loader_elf_load(data, size, image);
    free(data);
    return result;
}

void loader_elf_unload(loader_elf_image_t* image)
{
    if (image != NULL) {
        platform_executable_free(image->memory);
        *image = (loader_elf_image_t) {0};
    }
}

const char* loader_elf_result_name(loader_elf_result_t result)
{
    static const char* const names[] = {
        "OK",
        "INVALID ARGUMENT",
        "TRUNCATED",
        "UNSUPPORTED FORMAT",
        "UNSUPPORTED RELOCATION",
        "INVALID SEGMENT",
        "INVALID ENTRY",
        "IMAGE TOO LARGE",
        "NO EXECUTABLE MEMORY",
        "PREPARE FAILED",
        "FILE OPEN FAILED",
        "INVALID FILE",
        "FILE TOO LARGE",
        "NO FILE MEMORY",
        "FILE READ FAILED",
    };
    return (unsigned int) result < (sizeof(names) / sizeof(names[0])) ? names[result] : "UNKNOWN";
}
