#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/elf_application.h>
#include <tabos/internal/pointer.h>

#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdio.h>
#include <limits.h>

enum {
    KERNEL_PROCESS_CAPACITY = 16
};

typedef struct {
        bool occupied;
        tabos_process_id_t id;
        tabos_process_id_t parent_id;
        tabos_process_state_t state;
        tabos_app_context_t context;
        void (*application_data_destroy)(void* data);
        bool asynchronous;
        bool resources_released;
} kernel_process_t;

static kernel_process_t processes[KERNEL_PROCESS_CAPACITY];
static tabos_process_id_t foreground_stack[KERNEL_PROCESS_CAPACITY];
static size_t foreground_depth;
static tabos_process_id_t next_process_id;
static kernel_process_t* foreground_process;
static bool last_exit_valid;
static int last_exit_status;
static size_t scheduler_cursor;

static const char* termination_name(tabos_process_termination_t cause)
{
    switch (cause) {
        case TABOS_PROCESS_TERMINATION_EXIT_REQUEST: return "exit request";
        case TABOS_PROCESS_TERMINATION_RETURN: return "return";
        case TABOS_PROCESS_TERMINATION_FAULT: return "execution fault";
        case TABOS_PROCESS_TERMINATION_FORCED: return "forced termination";
        case TABOS_PROCESS_TERMINATION_NONE: return "unknown cause";
    }
    return "unknown cause";
}

static void panic_root_process(kernel_process_t* process)
{
    char message[96];
    process->state                  = TABOS_PROCESS_PANICKED;
    process->context.exit_requested = false;
    last_exit_status                = process->context.exit_status;
    last_exit_valid                 = true;
    (void) snprintf(message, sizeof(message), "KERNEL PANIC: process 0 (%s): %s; status %d",
                    process->context.descriptor->name, termination_name(process->context.termination_cause),
                    process->context.exit_status);
    platform_log(message);
    (void) console_write_panic(message);
}

static kernel_process_t* find_process(tabos_process_id_t id)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        if (processes[index].occupied && processes[index].id == id) {
            return &processes[index];
        }
    }
    return NULL;
}

static kernel_process_t* free_process_slot(void)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        if (!processes[index].occupied) {
            return &processes[index];
        }
    }
    return NULL;
}

static kernel_process_t* process_from_context(const tabos_app_context_t* context)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        if (processes[index].occupied && &processes[index].context == context) {
            return &processes[index];
        }
    }
    return NULL;
}

static void release_process_resources(kernel_process_t* process)
{
    if (process == NULL || !process->occupied || process->resources_released) {
        return;
    }

    tabos_app_context_t* context             = &process->context;
    const tabos_app_descriptor_t* descriptor = context->descriptor;
    const int status                         = context->exit_status;
    if (descriptor->cleanup != NULL) {
        descriptor->cleanup(context, status);
    }
    if (context->console_owned) {
        tabos_console_release(&context->console);
    }
    if (process->application_data_destroy != NULL) {
        process->application_data_destroy(context->application_data);
    }
    context->application_data   = NULL;
    context->descriptor         = NULL;
    context->console_owned      = false;
    process->resources_released = true;
    last_exit_status            = status;
    last_exit_valid             = true;
}

static void destroy_process(kernel_process_t* process)
{
    release_process_resources(process);
    *process = (kernel_process_t) {0};
}

static void destroy_descendants(tabos_process_id_t parent_id)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        kernel_process_t* child = &processes[index];
        if (child->occupied && child->parent_id == parent_id) {
            destroy_descendants(child->id);
            child->context.exit_status = -1;
            destroy_process(child);
        }
    }
}

static bool acquire_process_console(kernel_process_t* process)
{
    if ((process->context.descriptor->capabilities & TABOS_APP_CAPABILITY_CONSOLE) == 0U) {
        return true;
    }
    if (!tabos_console_acquire(&process->context.console)) {
        return false;
    }
    process->context.console_owned = true;
    return true;
}

static void release_process_console(kernel_process_t* process)
{
    if (process->context.console_owned) {
        tabos_console_release(&process->context.console);
        process->context.console_owned = false;
    }
}

static void finish_child_process(kernel_process_t* child)
{
    const int status = child->context.exit_status;
    destroy_descendants(child->id);
    if (child->asynchronous) {
        release_process_resources(child);
        child->state                  = TABOS_PROCESS_EXITED;
        child->context.exit_requested = false;
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
        return;
    }
    pointer_service_set_foreground_owner(NULL);
    release_process_console(child);
    destroy_process(child);
    if (foreground_depth > 0U) {
        --foreground_depth;
    }
    foreground_process = foreground_depth > 0U ? find_process(foreground_stack[foreground_depth - 1U]) : NULL;
    if (foreground_process == NULL) {
        return;
    }
    foreground_process->state                      = TABOS_PROCESS_RUNNING;
    foreground_process->context.child_status       = status;
    foreground_process->context.child_status_valid = true;
    pointer_service_set_foreground_owner(foreground_process->context.application_data);
    if (!acquire_process_console(foreground_process)) {
        foreground_process->context.exit_status = -1;
        if (foreground_process->id == 0U) {
            panic_root_process(foreground_process);
        } else {
            foreground_process->context.exit_requested = true;
        }
    }
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
}

void kernel_application_system_init(void)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        processes[index] = (kernel_process_t) {0};
    }
    foreground_process = NULL;
    foreground_depth   = 0U;
    next_process_id    = 0U;
    last_exit_valid    = false;
    last_exit_status   = 0;
    scheduler_cursor   = 0U;
    application_registry_reset();
}

void kernel_application_system_update(void)
{
    if (foreground_process == NULL || tabos_process_system_panicked()) {
        return;
    }
    tabos_process_id_t ready[KERNEL_PROCESS_CAPACITY];
    size_t count = 0U;
    for (size_t offset = 0U; offset < KERNEL_PROCESS_CAPACITY; ++offset) {
        const kernel_process_t* process = &processes[(scheduler_cursor + offset) % KERNEL_PROCESS_CAPACITY];
        if (process->occupied && process->state == TABOS_PROCESS_RUNNING) {
            ready[count++] = process->id;
        }
    }
    scheduler_cursor = (scheduler_cursor + 1U) % KERNEL_PROCESS_CAPACITY;
    for (size_t index = 0U; index < count; ++index) {
        kernel_process_t* process = find_process(ready[index]);
        if (process == NULL || process->state != TABOS_PROCESS_RUNNING) {
            continue;
        }
        tabos_app_context_t* context = &process->context;
        if (!context->exit_requested && context->descriptor->update != NULL) {
            context->descriptor->update(context);
        }
        if (context->exit_requested) {
            if (process->id == 0U) {
                panic_root_process(process);
                return;
            } else {
                finish_child_process(process);
            }
        }
    }
}

bool kernel_application_system_runnable(void)
{
    if (tabos_process_system_panicked()) {
        return false;
    }
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        const kernel_process_t* process = &processes[index];
        if (process->occupied && process->state == TABOS_PROCESS_RUNNING &&
            (process->context.exit_requested ||
             loader_elf_application_runtime_runnable(process->context.descriptor, process->context.application_data))) {
            return true;
        }
    }
    return false;
}

uint64_t kernel_application_system_next_deadline(void)
{
    if (tabos_process_system_panicked()) {
        return PLATFORM_RUNTIME_DEADLINE_NONE;
    }
    uint64_t deadline = PLATFORM_RUNTIME_DEADLINE_NONE;
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        const kernel_process_t* process = &processes[index];
        if (process->occupied && process->state == TABOS_PROCESS_RUNNING) {
            const uint64_t candidate =
                loader_elf_application_next_deadline(process->context.descriptor, process->context.application_data);
            if (candidate < deadline) {
                deadline = candidate;
            }
        }
    }
    return deadline;
}

void kernel_application_system_shutdown(void)
{
    pointer_service_set_foreground_owner(NULL);
    destroy_descendants(0U);
    for (size_t index = KERNEL_PROCESS_CAPACITY; index > 0U; --index) {
        if (processes[index - 1U].occupied) {
            destroy_process(&processes[index - 1U]);
        }
    }
    foreground_process = NULL;
    foreground_depth   = 0U;
    application_registry_reset();
}

static tabos_app_result_t launch_root_descriptor(const tabos_app_descriptor_t* descriptor, void* application_data,
                                                 void (*application_data_destroy)(void* data))
{
    if (descriptor == NULL) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_INVALID;
    }
    if (foreground_process != NULL) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_BUSY;
    }

    kernel_process_t* process = free_process_slot();
    if (process == NULL) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_START_FAILED;
    }
    *process = (kernel_process_t) {
        .occupied  = true,
        .id        = 0U,
        .parent_id = TABOS_PROCESS_ID_INVALID,
        .state     = TABOS_PROCESS_RUNNING,
        .context =
            {
                      .descriptor       = descriptor,
                      .process_id       = 0U,
                      .application_data = application_data,
                      },
        .application_data_destroy = application_data_destroy,
    };
    next_process_id              = 1U;
    tabos_app_context_t* context = &process->context;
    if (!acquire_process_console(process)) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        *process = (kernel_process_t) {0};
        return TABOS_APP_RESULT_START_FAILED;
    }

    foreground_process  = process;
    foreground_stack[0] = 0U;
    foreground_depth    = 1U;
    last_exit_valid     = false;
    pointer_service_set_foreground_owner(context->application_data);
    if (!descriptor->entry(context)) {
        context->exit_status = -1;
        destroy_process(process);
        foreground_process = NULL;
        foreground_depth   = 0U;
        pointer_service_set_foreground_owner(NULL);
        return TABOS_APP_RESULT_START_FAILED;
    }
    if (context->exit_requested) {
        panic_root_process(process);
    }
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    return TABOS_APP_RESULT_OK;
}

tabos_app_result_t tabos_app_launch(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    if (foreground_process != NULL) {
        return TABOS_APP_RESULT_BUSY;
    }
    const tabos_app_descriptor_t* descriptor = tabos_app_find(name);
    if (descriptor == NULL) {
        return TABOS_APP_RESULT_NOT_FOUND;
    }
    return launch_root_descriptor(descriptor, NULL, NULL);
}

tabos_app_result_t tabos_app_launch_path(const char* path)
{
    const char* const argv[] = {path};
    return tabos_app_launch_path_args(path, 1U, argv);
}

tabos_app_result_t tabos_app_launch_path_args(const char* path, size_t argc, const char* const* argv)
{
    if (path == NULL || path[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    loader_elf_application_t* application = loader_elf_application_create(path, argc, argv);
    if (application == NULL) {
        return TABOS_APP_RESULT_INVALID;
    }
    return launch_root_descriptor(loader_elf_application_descriptor(application), application,
                                  loader_elf_application_destroy);
}

static tabos_app_result_t launch_child_descriptor(tabos_app_context_t* parent, const tabos_app_descriptor_t* descriptor,
                                                  void* application_data, void (*application_data_destroy)(void* data))
{
    if (parent == NULL || descriptor == NULL) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_INVALID;
    }
    if (foreground_process == NULL || parent != &foreground_process->context ||
        foreground_process->state != TABOS_PROCESS_RUNNING) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_BUSY;
    }
    kernel_process_t* child = free_process_slot();
    if (child == NULL || foreground_depth >= KERNEL_PROCESS_CAPACITY || next_process_id >= INT_MAX) {
        if (application_data_destroy != NULL) {
            application_data_destroy(application_data);
        }
        return TABOS_APP_RESULT_START_FAILED;
    }

    kernel_process_t* parent_process  = foreground_process;
    const tabos_process_id_t child_id = next_process_id++;
    *child                            = (kernel_process_t) {
                                   .occupied  = true,
                                   .id        = child_id,
                                   .parent_id = parent_process->id,
                                   .state     = TABOS_PROCESS_RUNNING,
                                   .context =
            {
                      .descriptor       = descriptor,
                      .process_id       = child_id,
                      .application_data = application_data,
                      },
                                   .application_data_destroy = application_data_destroy,
    };
    release_process_console(parent_process);
    parent_process->state                = TABOS_PROCESS_BLOCKED;
    foreground_stack[foreground_depth++] = child_id;
    foreground_process                   = child;
    pointer_service_set_foreground_owner(child->context.application_data);
    if (!acquire_process_console(child) || !descriptor->entry(&child->context)) {
        child->context.exit_status = -1;
        finish_child_process(child);
        return TABOS_APP_RESULT_START_FAILED;
    }
    if (child->context.exit_requested) {
        finish_child_process(child);
    } else {
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    }
    return TABOS_APP_RESULT_OK;
}

tabos_app_result_t kernel_process_spawn_descriptor(tabos_app_context_t* parent,
                                                   const tabos_app_descriptor_t* descriptor, void* application_data,
                                                   void (*application_data_destroy)(void*),
                                                   tabos_process_id_t* child_id)
{
    kernel_process_t* owner   = process_from_context(parent);
    tabos_app_result_t result = TABOS_APP_RESULT_INVALID;
    if (owner != NULL && descriptor != NULL && descriptor->entry != NULL && child_id != NULL) {
        result = TABOS_APP_RESULT_BUSY;
        if (owner->state == TABOS_PROCESS_RUNNING && !parent->exit_requested && !tabos_process_system_panicked()) {
            kernel_process_t* child = free_process_slot();
            result                  = TABOS_APP_RESULT_START_FAILED;
            if (child != NULL && next_process_id < INT_MAX) {
                const tabos_process_id_t id = next_process_id++;
                *child                      = (kernel_process_t) {
                                         .occupied  = true,
                                         .id        = id,
                                         .parent_id = owner->id,
                                         .state     = TABOS_PROCESS_RUNNING,
                                         .context =
                        {
                                  .descriptor       = descriptor,
                                  .process_id       = id,
                                  .application_data = application_data,
                                  },
                                         .application_data_destroy = application_data_destroy,
                                         .asynchronous             = true,
                };
                if (!descriptor->entry(&child->context)) {
                    destroy_descendants(id);
                    child->context.exit_status = -1;
                    destroy_process(child);
                    return TABOS_APP_RESULT_START_FAILED;
                }
                *child_id = id;
                if (child->context.exit_requested) {
                    finish_child_process(child);
                }
                platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
                return TABOS_APP_RESULT_OK;
            }
        }
    }
    if (application_data_destroy != NULL) {
        application_data_destroy(application_data);
    }
    return result;
}

int kernel_process_reap(tabos_app_context_t* parent, tabos_process_id_t child_id, int* status)
{
    const kernel_process_t* owner = process_from_context(parent);
    kernel_process_t* child       = find_process(child_id);
    if (owner == NULL || child == NULL || !child->asynchronous || child->parent_id != owner->id || status == NULL) {
        return -1;
    }
    if (child->state != TABOS_PROCESS_EXITED) {
        return 0;
    }
    *status = child->context.exit_status;
    destroy_process(child);
    return 1;
}

tabos_app_result_t kernel_process_launch_child(tabos_app_context_t* parent, const char* name)
{
    if (parent == NULL || name == NULL || name[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    const tabos_app_descriptor_t* descriptor = tabos_app_find(name);
    if (descriptor == NULL) {
        return TABOS_APP_RESULT_NOT_FOUND;
    }
    return launch_child_descriptor(parent, descriptor, NULL, NULL);
}

tabos_app_result_t tabos_app_exec(tabos_app_context_t* context, const char* path)
{
    const char* const argv[] = {path};
    return tabos_app_exec_args(context, path, 1U, argv);
}

tabos_app_result_t kernel_process_spawn_path(tabos_app_context_t* parent, const char* path, size_t argc,
                                             const char* const* argv, tabos_process_id_t* child_id)
{
    const kernel_process_t* owner = process_from_context(parent);
    if (owner == NULL || child_id == NULL) {
        return TABOS_APP_RESULT_INVALID;
    }
    loader_elf_application_t* application = loader_elf_application_create(path, argc, argv);
    if (application == NULL) {
        return TABOS_APP_RESULT_INVALID;
    }
    if (owner->application_data_destroy == loader_elf_application_destroy &&
        (!loader_elf_application_set_working_directory(
             application, loader_elf_application_working_directory(parent->application_data)) ||
         !loader_elf_application_set_tty_mode(application,
                                              loader_elf_application_tty_mode(parent->application_data)))) {
        loader_elf_application_destroy(application);
        return TABOS_APP_RESULT_INVALID;
    }
    return kernel_process_spawn_descriptor(parent, loader_elf_application_descriptor(application), application,
                                           loader_elf_application_destroy, child_id);
}

tabos_app_result_t tabos_app_exec_args(tabos_app_context_t* context, const char* path, size_t argc,
                                       const char* const* argv)
{
    if (context == NULL || path == NULL || path[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    loader_elf_application_t* application = loader_elf_application_create(path, argc, argv);
    if (application == NULL) {
        return TABOS_APP_RESULT_INVALID;
    }
    if (foreground_process != NULL && foreground_process->application_data_destroy == loader_elf_application_destroy) {
        const char* working_directory =
            loader_elf_application_working_directory(foreground_process->context.application_data);
        if (!loader_elf_application_set_working_directory(application, working_directory)) {
            loader_elf_application_destroy(application);
            return TABOS_APP_RESULT_INVALID;
        }
        const uint32_t tty_mode = loader_elf_application_tty_mode(foreground_process->context.application_data);
        if (!loader_elf_application_set_tty_mode(application, tty_mode)) {
            loader_elf_application_destroy(application);
            return TABOS_APP_RESULT_INVALID;
        }
    }
    return launch_child_descriptor(context, loader_elf_application_descriptor(application), application,
                                   loader_elf_application_destroy);
}

bool tabos_app_take_child_status(tabos_app_context_t* parent, int* status)
{
    if (parent == NULL || status == NULL || !parent->child_status_valid) {
        return false;
    }
    *status                    = parent->child_status;
    parent->child_status_valid = false;
    return true;
}

bool tabos_app_is_running(void)
{
    return foreground_process != NULL && foreground_process->state == TABOS_PROCESS_RUNNING;
}

const tabos_app_descriptor_t* tabos_app_active(void)
{
    return foreground_process != NULL ? foreground_process->context.descriptor : NULL;
}

void tabos_app_request_exit(tabos_app_context_t* context, int exit_status)
{
    kernel_process_t* process = process_from_context(context);
    if (process != NULL && process->state == TABOS_PROCESS_RUNNING) {
        if (context->termination_cause == TABOS_PROCESS_TERMINATION_NONE) {
            context->termination_cause = TABOS_PROCESS_TERMINATION_EXIT_REQUEST;
        }
        context->exit_requested = true;
        context->exit_status    = exit_status;
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    }
}

void kernel_process_fail(tabos_app_context_t* context, tabos_process_termination_t cause, int exit_status)
{
    kernel_process_t* process = process_from_context(context);
    if (process == NULL || process->state != TABOS_PROCESS_RUNNING || cause == TABOS_PROCESS_TERMINATION_NONE) {
        return;
    }
    context->termination_cause = cause;
    context->exit_requested    = true;
    context->exit_status       = exit_status;
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
}

bool kernel_process_force_terminate(tabos_process_id_t process_id, int exit_status)
{
    kernel_process_t* process = find_process(process_id);
    if (process == NULL || process->state != TABOS_PROCESS_RUNNING) {
        return false;
    }
    kernel_process_fail(&process->context, TABOS_PROCESS_TERMINATION_FORCED, exit_status);
    return true;
}

const tabos_console_session_t* tabos_app_console(const tabos_app_context_t* context)
{
    return foreground_process != NULL && context == &foreground_process->context && context->console_owned ?
               &context->console :
               NULL;
}

tabos_process_id_t tabos_app_process_id(const tabos_app_context_t* context)
{
    const kernel_process_t* process = process_from_context(context);
    return process != NULL ? process->id : TABOS_PROCESS_ID_INVALID;
}

size_t tabos_process_count(void)
{
    size_t count = 0U;
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        if (processes[index].occupied) {
            ++count;
        }
    }
    return count;
}

bool tabos_process_info(tabos_process_id_t id, tabos_process_info_t* info)
{
    if (info == NULL) {
        return false;
    }
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        const kernel_process_t* process = &processes[index];
        if (process->occupied && process->id == id) {
            *info = (tabos_process_info_t) {
                .id        = process->id,
                .parent_id = process->parent_id,
                .state     = process->state,
                .name      = process->context.descriptor != NULL ? process->context.descriptor->name : NULL,
            };
            return true;
        }
    }
    return false;
}

bool tabos_process_system_panicked(void)
{
    const kernel_process_t* root = find_process(0U);
    return root != NULL && root->state == TABOS_PROCESS_PANICKED;
}

bool tabos_process_panic_info(tabos_process_termination_t* cause, int* exit_status)
{
    if (!tabos_process_system_panicked() || cause == NULL || exit_status == NULL) {
        return false;
    }
    const kernel_process_t* root = find_process(0U);
    *cause                       = root->context.termination_cause;
    *exit_status                 = root->context.exit_status;
    return true;
}

void application_report_diagnostic_result(tabos_app_context_t* context, int status)
{
    if (foreground_process != NULL && context == &foreground_process->context) {
        last_exit_status = status;
        last_exit_valid  = true;
    }
}

bool tabos_app_last_exit_status(int* exit_status)
{
    if (!last_exit_valid || exit_status == NULL) {
        return false;
    }
    *exit_status = last_exit_status;
    return true;
}
