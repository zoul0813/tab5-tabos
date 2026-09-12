#define _POSIX_C_SOURCE 200809L
#include <tabos/platform/platform.h>
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
        unsigned int notifications;
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
static unsigned int cancellations;
static unsigned int deletions;
static unsigned int running_observations;
static int owner;
static atomic_bool hold_completion = true;
static atomic_uint power_calls;

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

void vTaskDelay(TickType_t ticks)
{
    (void) ticks;
    if (self != NULL) {
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
    pthread_cond_broadcast(&target->changed);
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

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks)
{
    assert(clear == pdTRUE && ticks == portMAX_DELAY);
    pthread_mutex_lock(&self->mutex);
    while (self->notifications == 0U && !self->suspend && !self->deleted) {
        pthread_cond_wait(&self->changed, &self->mutex);
    }
    const unsigned int notifications = self->notifications;
    self->notifications              = 0U;
    pthread_mutex_unlock(&self->mutex);
    checkpoint();
    return notifications;
}

void xTaskNotifyGive(TaskHandle_t task)
{
    pthread_mutex_lock(&task->mutex);
    ++task->notifications;
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
    assert(stack_depth > 0U);
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
    if (!atomic_load(&hold_completion)) {
        return;
    }
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

static int flags_gate(int descriptor)
{
    assert(descriptor == 17);
    atomic_fetch_add(&power_calls, 1U);
    return 42;
}

static int power_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argc;
    (void) argv;
    atomic_store(&guest_entered, true);
    for (;;) {
        (void) api->fd_get_flags(17);
        vTaskDelay(1U);
    }
}

static void wait_gate(void)
{
    atomic_store(&guest_entered, true);
    while (!platform_riscv32_current_cancelled()) {
        platform_riscv32_power_checkpoint();
        atomic_fetch_add(&power_calls, 1U);
        vTaskDelay(1U);
    }
}

static int wait_entry(const tabos_elf_api_t* api, int argc, const char* const* argv)
{
    (void) argc;
    (void) argv;
    api->yield();
    atomic_store(&guest_after_gate, true);
    return 0;
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

static platform_riscv32_context_t* create(tabos_elf_entry_fn entry)
{
    const void* address = NULL;
    memcpy(&address, &entry, sizeof(address));
    const tabos_elf_api_t api = {.abi_version   = 123U,
                                 .console_write = service,
                                 .fd_get_flags  = flags_gate,
                                 .monotonic_ms  = clock_gate,
                                 .yield         = wait_gate,
                                 .heap_sbrk     = heap_gate};
    platform_riscv32_context_t* context =
        platform_riscv32_create(address, NULL, 0U, 0U, 128U, 4096U, &api, 0U, NULL, &owner);
    assert(context != NULL);
    return context;
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
        platform_riscv32_power_freeze(computing, true);
        vTaskDelay(1U);
        assert(!platform_riscv32_power_parked(computing));
        platform_riscv32_power_freeze(computing, false);
        platform_riscv32_stop(computing, cancel_service, &owner);
        platform_riscv32_destroy(computing);

        atomic_store(&cancelled, false);
        platform_riscv32_context_t* blocked = create(service_entry);
        assert(platform_riscv32_step(blocked, 1U, &status) == PLATFORM_RISCV32_YIELDED);
        while (!atomic_load(&in_service)) {
            vTaskDelay(1U);
        }
        platform_riscv32_power_freeze(blocked, true);
        assert(!platform_riscv32_power_parked(blocked));
        platform_riscv32_stop(blocked, cancel_service, &owner);
        platform_riscv32_stop(blocked, cancel_service, &owner);
        platform_riscv32_destroy(blocked);
        assert(!atomic_load(&guest_after_gate));
    }
    assert(deletions == 60U && cancellations >= 20U && running_observations >= 120U);
    atomic_store(&hold_completion, false);
    atomic_store(&guest_entered, false);
    platform_riscv32_context_t* power = create(power_entry);
    assert(platform_riscv32_step(power, 1U, &status) == PLATFORM_RISCV32_YIELDED);
    while (!atomic_load(&guest_entered)) {
        vTaskDelay(1U);
    }
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        platform_riscv32_power_freeze(power, true);
        while (!platform_riscv32_power_parked(power)) {
            vTaskDelay(1U);
        }
        const unsigned int before = atomic_load(&power_calls);
        vTaskDelay(1U);
        assert(atomic_load(&power_calls) == before);
        platform_riscv32_power_freeze(power, false);
        assert(!platform_riscv32_power_parked(power));
    }
    platform_riscv32_stop(power, NULL, NULL);
    platform_riscv32_destroy(power);
    atomic_store(&guest_entered, false);
    platform_riscv32_context_t* waiting = create(wait_entry);
    assert(platform_riscv32_step(waiting, 1U, &status) == PLATFORM_RISCV32_YIELDED);
    while (!atomic_load(&guest_entered)) {
        vTaskDelay(1U);
    }
    platform_riscv32_power_freeze(waiting, true);
    while (!platform_riscv32_power_parked(waiting)) {
        vTaskDelay(1U);
    }
    const unsigned int before = atomic_load(&power_calls);
    vTaskDelay(1U);
    assert(atomic_load(&power_calls) == before);
    /* Stop supersedes a retained, parked gate without deleting its active stack. */
    platform_riscv32_stop(waiting, NULL, NULL);
    platform_riscv32_destroy(waiting);
    assert(!atomic_load(&guest_after_gate));
    return 0;
}
