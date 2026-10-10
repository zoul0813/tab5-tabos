#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>
#include <tabos/config/identity.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
#include <esp_timer.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <stdatomic.h>
#include <setjmp.h>
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
        TickType_t last_idle_window;
        atomic_bool started;
        atomic_bool finished;
        atomic_bool stop_requested;
        atomic_uint gate_depth;
        int returned_status;
        const void* executable_memory;
        size_t executable_bytes;
        TaskHandle_t compute_task;
        platform_signal_t* compute_ready;
        platform_signal_t* compute_finished;
        void (*compute_callback)(void*);
        void* compute_data;
        jmp_buf compute_escape;
        atomic_bool compute_submitted;
        atomic_bool compute_complete;
        bool compute_pending;
        int compute_result;
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        uint64_t compute_enqueued_us;
        uint64_t compute_queue_us;
        uint64_t compute_execution_us;
        uint64_t compute_join_us;
        uint32_t compute_jobs;
#endif
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
    if (xTaskGetCurrentTaskHandle() == context->compute_task) {
        longjmp(context->compute_escape, 1);
    }
    park_if_stopping(context);
    atomic_fetch_add_explicit(&context->gate_depth, 1U, memory_order_acq_rel);
    return context;
}

static void gate_leave(platform_riscv32_context_t* context)
{
    /* A continuously runnable app must still allow idle reclamation and the
     * existing watchdog on both cores. Keep the gate owned while blocking. */
    const TickType_t now = xTaskGetTickCount();
    if (atomic_load_explicit(&context->gate_depth, memory_order_acquire) == 1U &&
        (TickType_t) (now - context->last_idle_window) >= pdMS_TO_TICKS(500U)) {
        vTaskDelay(1U);
        context->last_idle_window = xTaskGetTickCount();
    }
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
    ELF_TASK_PRIORITY        = 5,
    COMPUTE_TASK_STACK_BYTES = 16384
};

static void compute_task_main(void* argument)
{
    platform_riscv32_context_t* context = argument;
    vTaskSetThreadLocalStoragePointer(NULL, 0, context);
    for (;;) {
        platform_signal_wait(context->compute_ready, UINT32_MAX);
        if (!atomic_exchange_explicit(&context->compute_submitted, false, memory_order_acquire)) {
            continue;
        }
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        const uint64_t compute_start_us  = (uint64_t) esp_timer_get_time();
        context->compute_queue_us       += compute_start_us - context->compute_enqueued_us;
#endif
        if (setjmp(context->compute_escape) == 0) {
            context->compute_callback(context->compute_data);
            context->compute_result = 0;
        } else {
            context->compute_result = -TABOS_EPERM;
        }
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        context->compute_execution_us += (uint64_t) esp_timer_get_time() - compute_start_us;
        ++context->compute_jobs;
#endif
        atomic_store_explicit(&context->compute_complete, true, memory_order_release);
        platform_signal_notify(context->compute_finished);
    }
}

int platform_riscv32_compute_submit(uintptr_t callback, void* data)
{
    platform_riscv32_context_t* context = current_context();
    if (context == NULL || platform_riscv32_current_cancelled()) {
        return -TABOS_ECANCELED;
    }
    if (context->compute_pending) {
        return -TABOS_EBUSY;
    }
    const uintptr_t base = (uintptr_t) context->executable_memory;
    if (callback < base || callback - base >= context->executable_bytes ||
        context->executable_bytes - (callback - base) < 4U || (callback & 3U) != 0U || data == NULL) {
        return -TABOS_EINVAL;
    }
    if (context->compute_task == NULL) {
        context->compute_ready    = platform_signal_create();
        context->compute_finished = platform_signal_create();
        if (context->compute_ready == NULL || context->compute_finished == NULL ||
            xTaskCreateWithCaps(compute_task_main, "tabos-compute", COMPUTE_TASK_STACK_BYTES, context,
                                ELF_TASK_PRIORITY, &context->compute_task,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
            platform_signal_destroy(context->compute_ready);
            platform_signal_destroy(context->compute_finished);
            context->compute_ready    = NULL;
            context->compute_finished = NULL;
            return -TABOS_ENOMEM;
        }
    }
    memcpy(&context->compute_callback, &callback, sizeof(context->compute_callback));
    context->compute_data    = data;
    context->compute_pending = true;
    atomic_store_explicit(&context->compute_complete, false, memory_order_relaxed);
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    context->compute_enqueued_us = (uint64_t) esp_timer_get_time();
#endif
    atomic_store_explicit(&context->compute_submitted, true, memory_order_release);
    platform_signal_notify(context->compute_ready);
    return 0;
}

int platform_riscv32_compute_poll(void)
{
    platform_riscv32_context_t* context = current_context();
    if (context == NULL || platform_riscv32_current_cancelled()) {
        return -TABOS_ECANCELED;
    }
    return !context->compute_pending || atomic_load_explicit(&context->compute_complete, memory_order_acquire);
}

int platform_riscv32_compute_wait(void)
{
    platform_riscv32_context_t* context = current_context();
    if (context == NULL) {
        return -TABOS_ECANCELED;
    }
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    const uint64_t join_start_us = (uint64_t) esp_timer_get_time();
#endif
    while (context->compute_pending && !atomic_load_explicit(&context->compute_complete, memory_order_acquire)) {
        if (platform_riscv32_current_cancelled()) {
            return -TABOS_ECANCELED;
        }
        platform_signal_wait(context->compute_finished, 10U);
    }
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    context->compute_join_us += (uint64_t) esp_timer_get_time() - join_start_us;
#endif
    if (!context->compute_pending) {
        return 0;
    }
    context->compute_pending = false;
    return context->compute_result;
}

static void elf_task_main(void* argument)
{
    platform_riscv32_context_t* context = argument;
    vTaskSetThreadLocalStoragePointer(NULL, 0, context);
    context->last_idle_window = xTaskGetTickCount();
    context->returned_status  = context->entry(&context->guarded_api, (int) context->argc, context->argv);
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    ESP_LOGI(TAG, "ELF stack: bytes=%u unused_min=%u", (unsigned) context->stack_bytes,
             (unsigned) uxTaskGetStackHighWaterMark(NULL));
#endif
    atomic_store_explicit(&context->finished, true, memory_order_release);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    vTaskSuspend(NULL);
}

platform_riscv32_context_t* platform_riscv32_create(const void* entry, const void* memory, size_t memory_size,
                                                    uint32_t minimum_address, size_t heap_bytes, size_t stack_bytes,
                                                    const tabos_elf_api_t* api, size_t argc, const char* const* argv,
                                                    void* user_data)
{
    /* Retain the actual executable alias, not its readable data alias. */
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
    context->executable_memory = memory;
    context->executable_bytes  = memory_size;
    context->entry             = entry_function;
    context->api               = *api;
    context->guarded_api       = *api;
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
        if (xTaskCreateWithCaps(elf_task_main, "tabos-app", context->stack_bytes, context, ELF_TASK_PRIORITY,
                                &context->task, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
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
    if (context != NULL) {
        if (context->compute_task != NULL) {
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
            ESP_LOGI(TAG, "ELF compute: jobs=%u queue_us=%llu execution_us=%llu join_us=%llu",
                     (unsigned) context->compute_jobs, (unsigned long long) context->compute_queue_us,
                     (unsigned long long) context->compute_execution_us, (unsigned long long) context->compute_join_us);
#endif
            vTaskDeleteWithCaps(context->compute_task);
        }
        platform_signal_destroy(context->compute_ready);
        platform_signal_destroy(context->compute_finished);
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
            /* Pure workers own no service locks. Confirm cross-core suspension
             * before any callback code or borrowed app memory can be freed. */
            if (context->compute_task != NULL) {
                vTaskSuspend(context->compute_task);
                while (eTaskGetState(context->compute_task) == eRunning) {
                    vTaskDelay(1U);
                }
            }
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
