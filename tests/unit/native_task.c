#define _POSIX_C_SOURCE 200809L
#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>
#include <freertos/idf_additions.h>

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Cooperative scheduler model with real concurrent service ownership. External
 * suspension is acknowledged only when the target reaches a checkpoint. */
struct fake_native_task {
        pthread_t thread;
        pthread_mutex_t mutex;
        pthread_cond_t changed;
        bool suspend;
        bool parked;
        bool deleted;
        unsigned int delayed_state_reads;
        void* tls;
        void (*entry)(void*);
        void* argument;
};

static _Thread_local TaskHandle_t self;
static pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool in_service;
static atomic_bool cancelled;
static atomic_bool guest_entered;
static atomic_bool returned;
static atomic_bool guest_after_gate;
static bool fail_create;
static size_t expected_stack_bytes = 4096U;
static unsigned int cancellations;
static unsigned int deletions;
static unsigned int running_observations;
static int owner;
static atomic_uint tick_offset;
static atomic_uint native_delays;

static void checkpoint(void)
{
    pthread_mutex_lock(&self->mutex);
    while (self->suspend && !self->deleted) {
        self->parked = true;
        pthread_cond_broadcast(&self->changed);
        pthread_cond_wait(&self->changed, &self->mutex);
    }
    const bool deleted = self->deleted;
    self->parked       = false;
    pthread_mutex_unlock(&self->mutex);
    if (deleted) {
        pthread_exit(NULL);
    }
}

TickType_t xTaskGetTickCount(void)
{
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (TickType_t) ((uint64_t) now.tv_sec * 1000U + (uint64_t) now.tv_nsec / 1000000U) + atomic_load(&tick_offset);
}

void vTaskDelay(TickType_t ticks)
{
    (void) ticks;
    if (self != NULL) {
        atomic_fetch_add(&native_delays, 1U);
        checkpoint();
    }
    const struct timespec delay = {.tv_nsec = 1000000};
    nanosleep(&delay, NULL);
}

void vTaskSuspend(TaskHandle_t task)
{
    TaskHandle_t target = task != NULL ? task : self;
    pthread_mutex_lock(&target->mutex);
    target->suspend = true;
    pthread_mutex_unlock(&target->mutex);
    if (target == self) {
        checkpoint();
    }
}

void vTaskResume(TaskHandle_t task)
{
    pthread_mutex_lock(&task->mutex);
    task->suspend = false;
    pthread_cond_broadcast(&task->changed);
    pthread_mutex_unlock(&task->mutex);
}

eTaskState eTaskGetState(TaskHandle_t task)
{
    pthread_mutex_lock(&task->mutex);
    bool stopped = task->parked;
    if (task->delayed_state_reads > 0U) {
        --task->delayed_state_reads;
        stopped = false;
    }
    pthread_mutex_unlock(&task->mutex);
    if (!stopped) {
        ++running_observations;
    }
    return stopped ? eSuspended : eRunning;
}

void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index, void* value)
{
    assert(task == NULL && index == 0);
    self->tls = value;
}

void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index)
{
    assert(task == NULL && index == 0);
    return self != NULL ? self->tls : NULL;
}

static void* task_start(void* argument)
{
    self = argument;
    self->entry(self->argument);
    assert(false && "native task must park instead of returning");
    return NULL;
}

BaseType_t xTaskCreateWithCaps(void (*entry)(void*), const char* name, size_t stack_depth, void* argument,
                               UBaseType_t priority, TaskHandle_t* created, UBaseType_t capabilities)
{
    (void) name;
    (void) priority;
    (void) capabilities;
    assert(stack_depth == (strcmp(name, "tabos-compute") == 0 ? 16384U : expected_stack_bytes));
    if (fail_create) {
        return pdFAIL;
    }
    TaskHandle_t task = calloc(1U, sizeof(*task));
    assert(task != NULL);
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->changed, NULL);
    task->entry               = entry;
    task->argument            = argument;
    task->delayed_state_reads = 2U;
    *created                  = task;
    assert(pthread_create(&task->thread, NULL, task_start, task) == 0);
    return pdPASS;
}

void vTaskDeleteWithCaps(TaskHandle_t task)
{
    /* Model the pinned IDF helper's existing suspension handshake as well. */
    vTaskSuspend(task);
    while (eTaskGetState(task) == eRunning) {
        vTaskDelay(1U);
    }
    assert(!atomic_load(&in_service));
    assert(pthread_mutex_trylock(&service_mutex) == 0);
    pthread_mutex_unlock(&service_mutex);
    pthread_mutex_lock(&task->mutex);
    assert(task->parked && task->delayed_state_reads == 0U);
    task->deleted = true;
    pthread_cond_broadcast(&task->changed);
    pthread_mutex_unlock(&task->mutex);
    pthread_join(task->thread, NULL);
    pthread_cond_destroy(&task->changed);
    pthread_mutex_destroy(&task->mutex);
    free(task);
    ++deletions;
}

size_t heap_caps_get_free_size(unsigned int capabilities)
{
    (void) capabilities;
    return 4096U;
}

void fake_native_log(const char* tag, const char* format, ...)
{
    (void) tag;
    (void) format;
}

void platform_runtime_notify(platform_runtime_events_t events)
{
    assert(events == PLATFORM_RUNTIME_EVENT_APPLICATION);
    atomic_store(&returned, true);
    /* Hold completion before self-suspension to reproduce the reported window. */
    for (;;) {
        checkpoint();
        vTaskDelay(1U);
    }
}

static void service(const char* text)
{
    assert(strcmp(text, "owned") == 0 && platform_riscv32_current_user_data() == &owner);
    pthread_mutex_lock(&service_mutex);
    atomic_store(&in_service, true);
    while (!atomic_load(&cancelled)) {
        vTaskDelay(1U);
    }
    assert(platform_riscv32_current_cancelled());
    atomic_store(&in_service, false);
    pthread_mutex_unlock(&service_mutex);
}

static void cancel_service(void* user_data)
{
    assert(user_data == &owner);
    ++cancellations;
    atomic_store(&cancelled, true);
}

static int normal_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argv;
    assert(argc == 0 && api->abi_version == 123U && api->console_read == NULL);
    assert(api->fd_get_flags(17) == 42);
    assert(api->monotonic_ms() == UINT64_C(0x123456789abcdef0));
    assert(api->heap_sbrk(16) == &owner);
    return 7;
}

static int service_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argc;
    (void) argv;
    api->console_write("owned");
    atomic_store(&guest_after_gate, true);
    return 0;
}

static int computing_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) api;
    (void) argc;
    (void) argv;
    atomic_store(&guest_entered, true);
    for (;;) {
        vTaskDelay(1U); /* Scheduler checkpoint; no application call gate. */
    }
}

static int fairness_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argc;
    (void) argv;
    assert(atomic_load(&native_delays) == 0U);
    atomic_fetch_add(&tick_offset, 500U);
    (void) api->fd_get_flags(17);
    assert(atomic_load(&native_delays) == 1U);
    (void) api->fd_get_flags(17);
    assert(atomic_load(&native_delays) == 1U);
    atomic_fetch_add(&tick_offset, 500U);
    (void) api->fd_get_flags(17);
    assert(atomic_load(&native_delays) == 2U);
    return 0;
}

static int flags_gate(int descriptor)
{
    assert(descriptor == 17);
    return 42;
}

static uint64_t clock_gate(void)
{
    return UINT64_C(0x123456789abcdef0);
}

static void* heap_gate(int32_t increment)
{
    assert(increment == 16);
    return &owner;
}

static platform_riscv32_context_t* create_with_stack(tabos_elf_entry_fn entry, size_t stack_bytes)
{
    const void* address = NULL;
    memcpy(&address, &entry, sizeof(address));
    const tabos_elf_api_t api = {.abi_version   = 123U,
                                 .console_write = service,
                                 .fd_get_flags  = flags_gate,
                                 .monotonic_ms  = clock_gate,
                                 .heap_sbrk     = heap_gate};
    platform_riscv32_context_t* context =
        platform_riscv32_create(address, NULL, 0U, 0U, 128U, stack_bytes, &api, 0U, NULL, &owner);
    assert(context != NULL);
    return context;
}

static platform_riscv32_context_t* create(tabos_elf_entry_fn entry)
{
    return create_with_stack(entry, 4096U);
}


static const tabos_elf_api_t* compute_api;
static atomic_bool compute_entered;
static atomic_bool compute_release;
static atomic_bool compute_waiting;
static unsigned compute_scenario;
static int compute_output;
static unsigned signal_allocations;
static unsigned fail_signal_at;
static unsigned live_signals;

static void __attribute__((aligned(4))) compute_callback(void* data)
{
    assert(data == &compute_output);
    atomic_store(&compute_entered, true);
    while (!atomic_load(&compute_release)) {
        vTaskDelay(1U);
    }
    *(int*) data = 42;
}

static void __attribute__((aligned(4))) forbidden_callback(void* data)
{
    (void) data;
    (void) compute_api->fd_get_flags(99); /* Must abort before service entry. */
    assert(false);
}

static uintptr_t callback_address(void (*callback)(void*))
{
    uintptr_t address;
    memcpy(&address, &callback, sizeof(address));
    return address;
}

static int compute_submit_gate(uintptr_t callback, void* data, uint32_t bytes)
{
    assert(bytes == sizeof(compute_output));
    return platform_riscv32_compute_submit(callback, data);
}

static int compute_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argc;
    (void) argv;
    compute_api = api;
    assert(api->compute_poll() == 1);
    assert(api->compute_wait() == 0);
    assert(api->compute_submit(0U, &compute_output, sizeof(compute_output)) == -TABOS_EINVAL);
    assert(api->compute_submit(callback_address(compute_callback) + 1U, &compute_output, sizeof(compute_output)) ==
           -TABOS_EINVAL);
    assert(api->compute_submit(UINTPTR_MAX & ~(uintptr_t) 3U, &compute_output, sizeof(compute_output)) ==
           -TABOS_EINVAL);
    assert(api->compute_submit(callback_address(compute_callback), NULL, sizeof(compute_output)) == -TABOS_EINVAL);
    for (unsigned allocation = 1U; allocation <= 2U; ++allocation) {
        signal_allocations = 0U;
        fail_signal_at     = allocation;
        assert(api->compute_submit(callback_address(compute_callback), &compute_output, sizeof(compute_output)) ==
               -TABOS_ENOMEM);
        assert(live_signals == 0U);
    }
    fail_signal_at = 0U;
    fail_create    = true;
    assert(api->compute_submit(callback_address(compute_callback), &compute_output, sizeof(compute_output)) ==
           -TABOS_ENOMEM);
    fail_create = false;
    assert(live_signals == 0U);
    assert(api->compute_submit(callback_address(compute_callback), &compute_output, sizeof(compute_output)) == 0);
    while (!atomic_load(&compute_entered)) {
        vTaskDelay(1U);
    }
    assert(api->compute_poll() == 0);
    assert(api->compute_submit(callback_address(compute_callback), &compute_output, sizeof(compute_output)) ==
           -TABOS_EBUSY);
    atomic_store(&guest_entered, true);
    if (compute_scenario == 1U) {
        return 0; /* Deliberately leave callback running across entry return. */
    }
    if (compute_scenario == 2U) {
        for (;;) {
            vTaskDelay(1U); /* Force teardown outside all ABI gates. */
        }
    }
    if (compute_scenario == 3U) {
        atomic_store(&compute_waiting, true);
        (void) api->compute_wait(); /* Forced stop must drain this gate. */
        assert(false);
    }
    atomic_store(&compute_release, true);
    assert(api->compute_wait() == 0 && compute_output == 42);
    assert(api->compute_wait() == 0);
    assert(api->compute_submit(callback_address(forbidden_callback), &compute_output, sizeof(compute_output)) == 0);
    assert(api->compute_wait() == -TABOS_EPERM);
    compute_output = 0;
    assert(api->compute_submit(callback_address(compute_callback), &compute_output, sizeof(compute_output)) == 0);
    assert(api->compute_wait() == 0 && compute_output == 42);
    return 0;
}

static platform_riscv32_context_t* create_compute(void)
{
    tabos_elf_entry_fn entry = compute_entry;
    const void* address;
    memcpy(&address, &entry, sizeof(address));
    const tabos_elf_api_t api = {.compute_submit = compute_submit_gate,
                                 .compute_wait   = platform_riscv32_compute_wait,
                                 .compute_poll   = platform_riscv32_compute_poll,
                                 .fd_get_flags   = flags_gate};
    const uintptr_t first     = callback_address(compute_callback);
    const uintptr_t second    = callback_address(forbidden_callback);
    const uintptr_t base      = first < second ? first : second;
    const uintptr_t end       = (first > second ? first : second) + 4U;
    return platform_riscv32_create(address, (void*) base, end - base, 0U, 128U, expected_stack_bytes, &api, 0U, NULL,
                                   &owner);
}

int main(void)
{
    assert(platform_riscv32_current_user_data() == NULL && !platform_riscv32_current_cancelled());
    platform_riscv32_destroy(create(normal_entry)); /* Never started. */
    fail_create                        = true;
    platform_riscv32_context_t* failed = create(normal_entry);
    int status                         = 0;
    assert(platform_riscv32_step(failed, 1U, &status) == PLATFORM_RISCV32_FAULT);
    platform_riscv32_destroy(failed);
    fail_create = false;
    for (unsigned int round = 0U; round < 20U; ++round) {
        atomic_store(&returned, false);
        platform_riscv32_context_t* normal = create(normal_entry);
        assert(platform_riscv32_step(normal, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        while (!atomic_load(&returned)) {
            vTaskDelay(1U);
        }
        assert(platform_riscv32_step(normal, 1U, &status) == PLATFORM_RISCV32_RETURNED && status == 7);
        platform_riscv32_destroy(normal);

        atomic_store(&guest_entered, false);
        platform_riscv32_context_t* computing = create(computing_entry);
        assert(platform_riscv32_step(computing, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        while (!atomic_load(&guest_entered)) {
            vTaskDelay(1U);
        }
        platform_riscv32_stop(computing, cancel_service, &owner);
        platform_riscv32_destroy(computing);

        atomic_store(&cancelled, false);
        platform_riscv32_context_t* blocked = create(service_entry);
        assert(platform_riscv32_step(blocked, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        while (!atomic_load(&in_service)) {
            vTaskDelay(1U);
        }
        platform_riscv32_stop(blocked, cancel_service, &owner);
        platform_riscv32_stop(blocked, cancel_service, &owner);
        platform_riscv32_destroy(blocked);
        assert(!atomic_load(&guest_after_gate));
    }
    assert(deletions == 60U && cancellations >= 20U && running_observations >= 120U);
    atomic_store(&native_delays, 0U);
    atomic_store(&returned, false);
    platform_riscv32_context_t* fair = create(fairness_entry);
    assert(platform_riscv32_step(fair, 1U, &status) == PLATFORM_RISCV32_YIELDED);
    while (!atomic_load(&returned)) {
        vTaskDelay(1U);
    }
    assert(platform_riscv32_step(fair, 1U, &status) == PLATFORM_RISCV32_RETURNED && status == 0);
    platform_riscv32_destroy(fair);
    for (size_t bytes = 16384U; bytes <= 65536U; bytes *= 4U) {
        expected_stack_bytes = bytes;
        atomic_store(&returned, false);
        platform_riscv32_context_t* sized = create_with_stack(normal_entry, bytes);
        assert(platform_riscv32_step(sized, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        while (!atomic_load(&returned)) {
            vTaskDelay(1U);
        }
        assert(platform_riscv32_step(sized, 1U, &status) == PLATFORM_RISCV32_RETURNED && status == 7);
        platform_riscv32_destroy(sized);
    }
    expected_stack_bytes = 4096U;
    for (unsigned scenario = 0U; scenario < 4U; ++scenario) {
        compute_scenario = scenario;
        compute_output   = 0;
        atomic_store(&compute_entered, false);
        atomic_store(&compute_release, false);
        atomic_store(&compute_waiting, false);
        atomic_store(&guest_entered, false);
        atomic_store(&returned, false);
        platform_riscv32_context_t* context = create_compute();
        assert(context != NULL);
        assert(platform_riscv32_step(context, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        if (scenario <= 1U) {
            while (!atomic_load(&returned)) {
                vTaskDelay(1U);
            }
            assert(platform_riscv32_step(context, 1U, &status) == PLATFORM_RISCV32_RETURNED && status == 0);
        } else {
            while (!atomic_load(scenario == 2U ? &guest_entered : &compute_waiting)) {
                vTaskDelay(1U);
            }
            platform_riscv32_stop(context, NULL, NULL);
        }
        platform_riscv32_destroy(context);
        assert(live_signals == 0U);
        if (scenario != 0U) {
            assert(compute_output == 0);
        }
    }
    return 0;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return self;
}
struct platform_signal {
        atomic_bool notified;
};
platform_signal_t* platform_signal_create(void)
{
    ++signal_allocations;
    if (signal_allocations == fail_signal_at) {
        return NULL;
    }
    platform_signal_t* signal = calloc(1U, sizeof(platform_signal_t));
    if (signal != NULL) {
        ++live_signals;
    }
    return signal;
}
void platform_signal_destroy(platform_signal_t* signal)
{
    if (signal != NULL) {
        assert(live_signals != 0U);
        --live_signals;
    }
    free(signal);
}
void platform_signal_notify(platform_signal_t* signal)
{
    atomic_store(&signal->notified, true);
}
void platform_signal_wait(platform_signal_t* signal, uint32_t timeout_ms)
{
    const TickType_t start = xTaskGetTickCount();
    while (!atomic_exchange(&signal->notified, false)) {
        if (timeout_ms != UINT32_MAX && xTaskGetTickCount() - start >= timeout_ms) {
            break;
        }
        vTaskDelay(1U);
    }
}
