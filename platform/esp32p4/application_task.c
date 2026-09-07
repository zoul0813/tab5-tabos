#include <tabos/platform/platform.h>
#include <tabos/config/identity.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;

struct platform_riscv32_context {
        tabos_elf_entry_fn entry;
        tabos_elf_api_t api;
        tabos_elf_api_t guarded_api;
        size_t argc;
        const char* const* argv;
        void* user_data;
        size_t stack_bytes;
        TaskHandle_t task;
        atomic_bool started;
        atomic_bool finished;
        atomic_bool stop_requested;
        atomic_uint gate_depth;
        int returned_status;
};

static platform_riscv32_context_t* current_context(void)
{
    return pvTaskGetThreadLocalStoragePointer(NULL, 0);
}

static void park_if_stopping(platform_riscv32_context_t* context)
{
    while (atomic_load_explicit(&context->stop_requested, memory_order_acquire)) {
        vTaskSuspend(NULL);
    }
}

static platform_riscv32_context_t* gate_enter(void)
{
    platform_riscv32_context_t* context = current_context();
    park_if_stopping(context);
    atomic_fetch_add_explicit(&context->gate_depth, 1U, memory_order_acq_rel);
    return context;
}

static void gate_leave(platform_riscv32_context_t* context)
{
    if (atomic_fetch_sub_explicit(&context->gate_depth, 1U, memory_order_acq_rel) == 1U) {
        park_if_stopping(context);
    }
}

#define NATIVE_GATE(type, name, parameters, arguments)                     \
    static type guarded_##name parameters                                  \
    {                                                                      \
        platform_riscv32_context_t* context = gate_enter();                \
        type gate_result                    = context->api.name arguments; \
        gate_leave(context);                                               \
        return gate_result;                                                \
    }
#define NATIVE_VOID_GATE(name, parameters, arguments)       \
    static void guarded_##name parameters                   \
    {                                                       \
        platform_riscv32_context_t* context = gate_enter(); \
        context->api.name arguments;                        \
        gate_leave(context);                                \
    }
#include "application_gates.inc"
#undef NATIVE_GATE
#undef NATIVE_VOID_GATE

enum {
    NATIVE_GATE_COUNT = 0
#define NATIVE_GATE(type, name, parameters, arguments) +1
#define NATIVE_VOID_GATE(name, parameters, arguments)  +1
#include "application_gates.inc"
#undef NATIVE_GATE
#undef NATIVE_VOID_GATE
};
_Static_assert(sizeof(tabos_elf_api_t) ==
                   offsetof(tabos_elf_api_t, console_write) + NATIVE_GATE_COUNT * sizeof(void (*)(void)),
               "Update native gate guards when the private ABI changes");

enum {
    ELF_TASK_PRIORITY = 5
};

static void elf_task_main(void* argument)
{
    platform_riscv32_context_t* context = argument;
    vTaskSetThreadLocalStoragePointer(NULL, 0, context);
    context->returned_status = context->entry(&context->guarded_api, (int) context->argc, context->argv);
    atomic_store_explicit(&context->finished, true, memory_order_release);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    vTaskSuspend(NULL);
}

platform_riscv32_context_t* platform_riscv32_create(const void* entry, const void* memory, size_t memory_size,
                                                    uint32_t minimum_address, size_t heap_bytes, size_t stack_bytes,
                                                    const tabos_elf_api_t* api, size_t argc, const char* const* argv,
                                                    void* user_data)
{
    (void) memory;
    (void) memory_size;
    (void) minimum_address;
    if (entry == NULL || api == NULL || heap_bytes == 0U || stack_bytes == 0U || argc > TABOS_ELF_ARG_MAX ||
        (argc > 0U && argv == NULL) || stack_bytes > UINT32_MAX) {
        return NULL;
    }
    platform_riscv32_context_t* context = calloc(1U, sizeof(*context));
    if (context == NULL) {
        return NULL;
    }
    tabos_elf_entry_fn entry_function = NULL;
    _Static_assert(sizeof(entry_function) == sizeof(entry), "ELF entry pointer must match data pointer size");
    memcpy(&entry_function, &entry, sizeof(entry_function));
    context->entry       = entry_function;
    context->api         = *api;
    context->guarded_api = *api;
#define NATIVE_GATE(type, name, parameters, arguments) \
    context->guarded_api.name = api->name != NULL ? guarded_##name : NULL;
#define NATIVE_VOID_GATE(name, parameters, arguments) NATIVE_GATE(void, name, parameters, arguments)
#include "application_gates.inc"
#undef NATIVE_GATE
#undef NATIVE_VOID_GATE
    context->argc        = argc;
    context->argv        = argv;
    context->user_data   = user_data;
    context->stack_bytes = stack_bytes;
    return context;
}

platform_riscv32_result_t platform_riscv32_step(platform_riscv32_context_t* context, unsigned int instruction_budget,
                                                int* returned_status)
{
    if (context == NULL || instruction_budget == 0U || returned_status == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    if (!atomic_load_explicit(&context->started, memory_order_acquire)) {
        if (xTaskCreateWithCaps(elf_task_main, "tabos-app", context->stack_bytes / sizeof(StackType_t), context,
                                ELF_TASK_PRIORITY, &context->task, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
            ESP_LOGE(TAG, "Could not create ELF task; free internal=%u, PSRAM=%u",
                     (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
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

bool platform_riscv32_requires_runtime_slices(void)
{
    return false;
}

uint64_t platform_riscv32_next_deadline(const platform_riscv32_context_t* context)
{
    (void) context;
    return PLATFORM_RUNTIME_DEADLINE_NONE;
}

void platform_riscv32_destroy(platform_riscv32_context_t* context)
{
    platform_riscv32_stop(context, NULL, NULL);
    if (context != NULL && atomic_load_explicit(&context->started, memory_order_acquire) && context->task != NULL) {
        vTaskDeleteWithCaps(context->task);
    }
    free(context);
}

void* platform_riscv32_current_user_data(void)
{
    platform_riscv32_context_t* context = current_context();
    return context != NULL ? context->user_data : NULL;
}

bool platform_riscv32_current_cancelled(void)
{
    platform_riscv32_context_t* context = current_context();
    return context != NULL && atomic_load_explicit(&context->stop_requested, memory_order_acquire);
}

void platform_riscv32_stop(platform_riscv32_context_t* context, void (*cancel)(void*), void* user_data)
{
    if (context == NULL || !atomic_load_explicit(&context->started, memory_order_acquire) || context->task == NULL) {
        return;
    }
    atomic_store_explicit(&context->stop_requested, true, memory_order_release);
    for (;;) {
        vTaskSuspend(context->task);
        /* eTaskGetState checks both cores' current TCBs before suspended lists.
         * Publishing a flag alone is not proof the other core has stopped. */
        while (eTaskGetState(context->task) == eRunning) {
            vTaskDelay(1U);
        }
        if (atomic_load_explicit(&context->gate_depth, memory_order_acquire) == 0U) {
            return;
        }
        /* The suspended task cannot mutate its handles while cancellation is
         * issued. Resume active gates so they release service/worker mutexes.
         * Gate exit parks permanently; no subsequent guest call can start. */
        if (cancel != NULL) {
            cancel(user_data);
        }
        vTaskResume(context->task);
        vTaskDelay(1U);
    }
}
