#include <tabos/internal/application.h>

#include <tabos/platform/platform.h>

#include <stddef.h>
#include <stdio.h>

enum { KERNEL_PROCESS_CAPACITY = 16 };

typedef struct {
    bool occupied;
    tabos_process_id_t id;
    tabos_process_id_t parent_id;
    tabos_process_state_t state;
    tabos_app_context_t context;
} kernel_process_t;

static kernel_process_t processes[KERNEL_PROCESS_CAPACITY];
static kernel_process_t *foreground_process;
static bool last_exit_valid;
static int last_exit_status;

static void panic_root_process(kernel_process_t *process)
{
    char message[96];
    process->state = TABOS_PROCESS_PANICKED;
    process->context.exit_requested = false;
    last_exit_status = process->context.exit_status;
    last_exit_valid = true;
    (void)snprintf(message, sizeof(message),
        "KERNEL PANIC: process 0 (%s) exited with status %d",
        process->context.descriptor->name, process->context.exit_status);
    platform_log(message);
    if (process->context.console_owned) {
        (void)tabos_console_write(&process->context.console, "\n[KERNEL PANIC] ");
        (void)tabos_console_write_line(&process->context.console, message + 14);
    }
}

static void release_process(kernel_process_t *process)
{
    if (process == NULL || !process->occupied) {
        return;
    }

    tabos_app_context_t *context = &process->context;
    const tabos_app_descriptor_t *descriptor = context->descriptor;
    const int status = context->exit_status;
    if (descriptor->cleanup != NULL) {
        descriptor->cleanup(context, status);
    }
    if (context->console_owned) {
        tabos_console_release(&context->console);
    }

    *process = (kernel_process_t){0};
    if (foreground_process == process) foreground_process = NULL;
    last_exit_status = status;
    last_exit_valid = true;
}

void kernel_application_system_init(void)
{
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        processes[index] = (kernel_process_t){0};
    }
    foreground_process = NULL;
    last_exit_valid = false;
    last_exit_status = 0;
    application_registry_reset();
}

void kernel_application_system_update(void)
{
    if (foreground_process == NULL) {
        return;
    }
    tabos_app_context_t *context = &foreground_process->context;
    if (foreground_process->state == TABOS_PROCESS_RUNNING &&
        !context->exit_requested && context->descriptor->update != NULL) {
        context->descriptor->update(context);
    }
    if (context->exit_requested) {
        panic_root_process(foreground_process);
    }
}

void kernel_application_system_shutdown(void)
{
    for (size_t index = KERNEL_PROCESS_CAPACITY; index > 0U; --index) {
        if (processes[index - 1U].occupied) release_process(&processes[index - 1U]);
    }
    application_registry_reset();
}

tabos_app_result_t tabos_app_launch(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    if (foreground_process != NULL) {
        return TABOS_APP_RESULT_BUSY;
    }
    const tabos_app_descriptor_t *descriptor = tabos_app_find(name);
    if (descriptor == NULL) {
        return TABOS_APP_RESULT_NOT_FOUND;
    }

    kernel_process_t *process = &processes[0];
    *process = (kernel_process_t){
        .occupied = true,
        .id = 0U,
        .parent_id = TABOS_PROCESS_ID_INVALID,
        .state = TABOS_PROCESS_RUNNING,
        .context = {
            .descriptor = descriptor,
            .process_id = 0U,
        },
    };
    tabos_app_context_t *context = &process->context;
    if ((descriptor->capabilities & TABOS_APP_CAPABILITY_CONSOLE) != 0U) {
        if (!tabos_console_acquire(&context->console)) {
            *process = (kernel_process_t){0};
            return TABOS_APP_RESULT_START_FAILED;
        }
        context->console_owned = true;
    }

    foreground_process = process;
    last_exit_valid = false;
    if (!descriptor->entry(context)) {
        context->exit_status = -1;
        release_process(process);
        return TABOS_APP_RESULT_START_FAILED;
    }
    if (context->exit_requested) panic_root_process(process);
    return TABOS_APP_RESULT_OK;
}

bool tabos_app_is_running(void)
{
    return foreground_process != NULL && foreground_process->state == TABOS_PROCESS_RUNNING;
}

const tabos_app_descriptor_t *tabos_app_active(void)
{
    return foreground_process != NULL ? foreground_process->context.descriptor : NULL;
}

void tabos_app_request_exit(tabos_app_context_t *context, int exit_status)
{
    if (foreground_process != NULL && context == &foreground_process->context) {
        context->exit_requested = true;
        context->exit_status = exit_status;
    }
}

const tabos_console_session_t *tabos_app_console(const tabos_app_context_t *context)
{
    return foreground_process != NULL && context == &foreground_process->context &&
            context->console_owned
        ? &context->console
        : NULL;
}

tabos_process_id_t tabos_app_process_id(const tabos_app_context_t *context)
{
    return foreground_process != NULL && context == &foreground_process->context
        ? context->process_id : TABOS_PROCESS_ID_INVALID;
}

size_t tabos_process_count(void)
{
    size_t count = 0U;
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        if (processes[index].occupied) ++count;
    }
    return count;
}

bool tabos_process_info(tabos_process_id_t id, tabos_process_info_t *info)
{
    if (info == NULL) return false;
    for (size_t index = 0U; index < KERNEL_PROCESS_CAPACITY; ++index) {
        const kernel_process_t *process = &processes[index];
        if (process->occupied && process->id == id) {
            *info = (tabos_process_info_t){
                .id = process->id,
                .parent_id = process->parent_id,
                .state = process->state,
                .name = process->context.descriptor->name,
            };
            return true;
        }
    }
    return false;
}

bool tabos_process_system_panicked(void)
{
    return foreground_process != NULL &&
        foreground_process->state == TABOS_PROCESS_PANICKED;
}

void application_report_diagnostic_result(tabos_app_context_t *context, int status)
{
    if (foreground_process != NULL && context == &foreground_process->context) {
        last_exit_status = status;
        last_exit_valid = true;
    }
}

bool tabos_app_last_exit_status(int *exit_status)
{
    if (!last_exit_valid || exit_status == NULL) {
        return false;
    }
    *exit_status = last_exit_status;
    return true;
}
