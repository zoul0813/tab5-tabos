#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mmu_map.h>
#include <sdkconfig.h>

#include <stdint.h>

typedef struct {
        void* writable;
        void* executable;
        size_t mapped_size;
} executable_mapping_t;

enum {
    EXECUTABLE_MAPPING_CAPACITY = 16
};
static executable_mapping_t mappings[EXECUTABLE_MAPPING_CAPACITY];
static const char* const TAG = TABOS_PLATFORM_LOG_TAG;

void* platform_executable_alloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }
    executable_mapping_t* mapping = NULL;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        if (mappings[index].writable == NULL) {
            mapping = &mappings[index];
            break;
        }
    }
    if (mapping == NULL) {
        return NULL;
    }
    const size_t page_size   = CONFIG_MMU_PAGE_SIZE;
    const size_t mapped_size = (size + page_size - 1U) & ~(page_size - 1U);
    void* writable           = heap_caps_aligned_alloc(page_size, mapped_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (writable == NULL) {
        return NULL;
    }
    *mapping = (executable_mapping_t) {.writable = writable, .mapped_size = mapped_size};
    return writable;
}

void* platform_executable_prepare(void* memory, size_t size)
{
    executable_mapping_t* mapping = NULL;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        if (mappings[index].writable == memory) {
            mapping = &mappings[index];
            break;
        }
    }
    if (mapping == NULL || size == 0U || size > mapping->mapped_size) {
        return NULL;
    }
    esp_paddr_t physical_address = 0U;
    mmu_target_t target          = MMU_TARGET_FLASH0;
    if (esp_cache_msync(memory, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED) != ESP_OK ||
        esp_mmu_vaddr_to_paddr(memory, &physical_address, &target) != ESP_OK || target != MMU_TARGET_PSRAM0) {
        return NULL;
    }
    void* executable = NULL;
    /* ESP-IDF rejects EXEC|WRITE requests. On ESP32-P4 this selects the PSRAM
       linear region, whose hardware region is connected to both I-bus and D-bus. */
    const esp_err_t result =
        esp_mmu_map(physical_address, mapping->mapped_size, MMU_TARGET_PSRAM0, MMU_MEM_CAP_EXEC | MMU_MEM_CAP_READ,
                    ESP_MMU_MMAP_FLAG_PADDR_SHARED, &executable);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not map executable PSRAM: %s", esp_err_to_name(result));
        return NULL;
    }
    mapping->executable = executable;
    __builtin___clear_cache((char*) executable, (char*) executable + size);
    return executable;
}

bool platform_executable_finalize(void* memory, size_t size)
{
    executable_mapping_t* mapping = NULL;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        if (mappings[index].writable == memory || mappings[index].executable == memory) {
            mapping = &mappings[index];
            break;
        }
    }
    if (mapping == NULL || size == 0U || size > mapping->mapped_size ||
        esp_cache_msync(mapping->writable, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED) !=
            ESP_OK) {
        return false;
    }
    __builtin___clear_cache((char*) mapping->executable, (char*) mapping->executable + size);
    return true;
}

const void* platform_executable_data_pointer(const void* memory, size_t size)
{
    const uintptr_t address = (uintptr_t) memory;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        const executable_mapping_t* mapping = &mappings[index];
        const uintptr_t executable          = (uintptr_t) mapping->executable;
        if (mapping->executable != NULL && address >= executable && address - executable < mapping->mapped_size) {
            const size_t offset       = address - executable;
            const size_t synchronized = size < mapping->mapped_size - offset ? size : mapping->mapped_size - offset;
            void* writable            = (uint8_t*) mapping->writable + offset;
            if (synchronized > 0U) {
                const uintptr_t cache_line_size  = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
                const uintptr_t invalidate_start = (uintptr_t) writable & ~(cache_line_size - 1U);
                const uintptr_t invalidate_end =
                    ((uintptr_t) writable + synchronized + cache_line_size - 1U) & ~(cache_line_size - 1U);
                if (esp_cache_msync((void*) memory, synchronized,
                                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED) != ESP_OK ||
                    esp_cache_msync((void*) invalidate_start, invalidate_end - invalidate_start,
                                    ESP_CACHE_MSYNC_FLAG_DIR_M2C) != ESP_OK) {
                    return NULL;
                }
            }
            return writable;
        }
    }
    return memory;
}

void platform_executable_free(void* memory)
{
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        executable_mapping_t* mapping = &mappings[index];
        if (memory != mapping->writable && memory != mapping->executable) {
            continue;
        }
        if (mapping->executable != NULL) {
            (void) esp_mmu_unmap(mapping->executable);
        }
        heap_caps_free(mapping->writable);
        *mapping = (executable_mapping_t) {0};
        return;
    }
}

bool platform_can_execute_riscv32(void)
{
    return true;
}
