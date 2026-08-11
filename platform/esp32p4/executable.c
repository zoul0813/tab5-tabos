#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mmu_map.h>
#include <sdkconfig.h>

#include <stdint.h>

typedef struct {
    void *writable;
    void *executable;
    size_t mapped_size;
} executable_mapping_t;

static executable_mapping_t mapping;
static const char *const TAG = TABOS_PLATFORM_LOG_TAG;

void *tab_platform_executable_alloc(size_t size)
{
    if (size == 0U || mapping.writable != NULL) return NULL;
    const size_t page_size = CONFIG_MMU_PAGE_SIZE;
    const size_t mapped_size = (size + page_size - 1U) & ~(page_size - 1U);
    void *writable = heap_caps_aligned_alloc(
        page_size, mapped_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (writable == NULL) return NULL;
    mapping = (executable_mapping_t){.writable = writable, .mapped_size = mapped_size};
    return writable;
}

void *tab_platform_executable_prepare(void *memory, size_t size)
{
    if (memory == NULL || memory != mapping.writable || size == 0U || size > mapping.mapped_size) {
        return NULL;
    }
    esp_paddr_t physical_address = 0U;
    mmu_target_t target = MMU_TARGET_FLASH0;
    if (esp_cache_msync(memory, size,
            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED) != ESP_OK ||
        esp_mmu_vaddr_to_paddr(memory, &physical_address, &target) != ESP_OK ||
        target != MMU_TARGET_PSRAM0) {
        return NULL;
    }
    void *executable = NULL;
    const esp_err_t result = esp_mmu_map(
        physical_address, mapping.mapped_size, MMU_TARGET_PSRAM0,
        MMU_MEM_CAP_EXEC | MMU_MEM_CAP_READ, ESP_MMU_MMAP_FLAG_PADDR_SHARED, &executable);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not map executable PSRAM: %s", esp_err_to_name(result));
        return NULL;
    }
    mapping.executable = executable;
    __builtin___clear_cache((char *)executable, (char *)executable + size);
    return executable;
}

const void *tab_platform_executable_data_pointer(const void *memory)
{
    const uintptr_t address = (uintptr_t)memory;
    const uintptr_t executable = (uintptr_t)mapping.executable;
    if (mapping.executable != NULL && address >= executable &&
        address - executable < mapping.mapped_size) {
        return (const uint8_t *)mapping.writable + (address - executable);
    }
    return memory;
}

void tab_platform_executable_free(void *memory)
{
    if (memory == mapping.executable && mapping.executable != NULL) {
        (void)esp_mmu_unmap(mapping.executable);
    }
    if ((memory == mapping.writable || memory == mapping.executable) && mapping.writable != NULL) {
        heap_caps_free(mapping.writable);
        mapping = (executable_mapping_t){0};
    }
}

bool tab_platform_can_execute_riscv32(void)
{
    return true;
}
