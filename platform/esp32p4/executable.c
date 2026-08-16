#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mmu_map.h>
#include <sdkconfig.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *writable;
    void *executable;
    size_t mapped_size;
} executable_mapping_t;

enum { EXECUTABLE_MAPPING_CAPACITY = 16 };
static executable_mapping_t mappings[EXECUTABLE_MAPPING_CAPACITY];
static const char *const TAG = TABOS_PLATFORM_LOG_TAG;

struct platform_riscv32_context {
    tabos_elf_entry_fn entry;
    tabos_elf_api_t api;
    void *user_data;
    TaskHandle_t task;
    atomic_bool started;
    atomic_bool finished;
    int returned_status;
};

enum {
    ELF_TASK_STACK_BYTES = 16 * 1024,
    ELF_TASK_PRIORITY = 5,
};

static void elf_task_main(void *argument)
{
    platform_riscv32_context_t *context = argument;
    vTaskSetThreadLocalStoragePointer(NULL, 0, context->user_data);
    context->returned_status = context->entry(&context->api);
    atomic_store_explicit(&context->finished, true, memory_order_release);
    vTaskDelete(NULL);
}

void *platform_executable_alloc(size_t size)
{
    if (size == 0U) return NULL;
    executable_mapping_t *mapping = NULL;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        if (mappings[index].writable == NULL) {
            mapping = &mappings[index];
            break;
        }
    }
    if (mapping == NULL) return NULL;
    const size_t page_size = CONFIG_MMU_PAGE_SIZE;
    const size_t mapped_size = (size + page_size - 1U) & ~(page_size - 1U);
    void *writable = heap_caps_aligned_alloc(
        page_size, mapped_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (writable == NULL) return NULL;
    *mapping = (executable_mapping_t){.writable = writable, .mapped_size = mapped_size};
    return writable;
}

void *platform_executable_prepare(void *memory, size_t size)
{
    executable_mapping_t *mapping = NULL;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        if (mappings[index].writable == memory) {
            mapping = &mappings[index];
            break;
        }
    }
    if (mapping == NULL || size == 0U || size > mapping->mapped_size) return NULL;
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
        physical_address, mapping->mapped_size, MMU_TARGET_PSRAM0,
        MMU_MEM_CAP_EXEC | MMU_MEM_CAP_READ, ESP_MMU_MMAP_FLAG_PADDR_SHARED, &executable);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not map executable PSRAM: %s", esp_err_to_name(result));
        return NULL;
    }
    mapping->executable = executable;
    __builtin___clear_cache((char *)executable, (char *)executable + size);
    return executable;
}

const void *platform_executable_data_pointer(const void *memory)
{
    const uintptr_t address = (uintptr_t)memory;
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        const executable_mapping_t *mapping = &mappings[index];
        const uintptr_t executable = (uintptr_t)mapping->executable;
        if (mapping->executable != NULL && address >= executable &&
            address - executable < mapping->mapped_size) {
            return (const uint8_t *)mapping->writable + (address - executable);
        }
    }
    return memory;
}

void platform_executable_free(void *memory)
{
    for (size_t index = 0U; index < EXECUTABLE_MAPPING_CAPACITY; ++index) {
        executable_mapping_t *mapping = &mappings[index];
        if (memory != mapping->writable && memory != mapping->executable) continue;
        if (mapping->executable != NULL) (void)esp_mmu_unmap(mapping->executable);
        heap_caps_free(mapping->writable);
        *mapping = (executable_mapping_t){0};
        return;
    }
}

bool platform_can_execute_riscv32(void)
{
    return true;
}

platform_riscv32_context_t *platform_riscv32_create(
    const void *entry,
    const void *memory,
    size_t memory_size,
    uint32_t minimum_address,
    const tabos_elf_api_t *api,
    void *user_data)
{
    (void)memory;
    (void)memory_size;
    (void)minimum_address;
    if (entry == NULL || api == NULL) return NULL;
    platform_riscv32_context_t *context = calloc(1U, sizeof(*context));
    if (context == NULL) return NULL;
    tabos_elf_entry_fn entry_function = NULL;
    _Static_assert(sizeof(entry_function) == sizeof(entry),
                   "ELF entry pointer must match data pointer size");
    memcpy(&entry_function, &entry, sizeof(entry_function));
    context->entry = entry_function;
    context->api = *api;
    context->user_data = user_data;
    return context;
}

platform_riscv32_result_t platform_riscv32_step(
    platform_riscv32_context_t *context,
    unsigned int instruction_budget,
    int *returned_status)
{
    if (context == NULL || instruction_budget == 0U || returned_status == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    if (!atomic_load_explicit(&context->started, memory_order_acquire)) {
        if (xTaskCreate(elf_task_main, "tabos-app", ELF_TASK_STACK_BYTES / sizeof(StackType_t),
                        context, ELF_TASK_PRIORITY, &context->task) != pdPASS) {
            return PLATFORM_RISCV32_FAULT;
        }
        atomic_store_explicit(&context->started, true, memory_order_release);
        return PLATFORM_RISCV32_YIELDED;
    }
    if (!atomic_load_explicit(&context->finished, memory_order_acquire)) {
        return PLATFORM_RISCV32_YIELDED;
    }
    *returned_status = context->returned_status;
    return PLATFORM_RISCV32_RETURNED;
}

void platform_riscv32_destroy(platform_riscv32_context_t *context)
{
    if (context != NULL && atomic_load_explicit(&context->started, memory_order_acquire) &&
        !atomic_load_explicit(&context->finished, memory_order_acquire) &&
        context->task != NULL) {
        vTaskDelete(context->task);
    }
    free(context);
}

void *platform_riscv32_current_user_data(void)
{
    return pvTaskGetThreadLocalStoragePointer(NULL, 0);
}
