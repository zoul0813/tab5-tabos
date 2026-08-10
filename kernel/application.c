#include <tabos/internal/application.h>

#include <stddef.h>

static tabos_app_context_t active_context;
static bool running;
static bool last_exit_valid;
static int last_exit_status;

static void finish_active_application(void)
{
    if (!running) {
        return;
    }

    const tabos_app_descriptor_t *descriptor = active_context.descriptor;
    const int status = active_context.exit_status;
    if (descriptor->cleanup != NULL) {
        descriptor->cleanup(&active_context, status);
    }
    if (active_context.console_owned) {
        tabos_console_release(&active_context.console);
    }

    active_context = (tabos_app_context_t){0};
    running = false;
    last_exit_status = status;
    last_exit_valid = true;
}

void tab_app_system_init(void)
{
    active_context = (tabos_app_context_t){0};
    running = false;
    last_exit_valid = false;
    last_exit_status = 0;
    tab_app_registry_reset();
}

void tab_app_system_update(void)
{
    if (!running) {
        return;
    }
    if (!active_context.exit_requested && active_context.descriptor->update != NULL) {
        active_context.descriptor->update(&active_context);
    }
    if (active_context.exit_requested) {
        finish_active_application();
    }
}

void tab_app_system_shutdown(void)
{
    if (running) {
        active_context.exit_requested = true;
        finish_active_application();
    }
    tab_app_registry_reset();
}

tabos_app_result_t tabos_app_launch(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return TABOS_APP_RESULT_INVALID;
    }
    if (running) {
        return TABOS_APP_RESULT_BUSY;
    }
    const tabos_app_descriptor_t *descriptor = tabos_app_find(name);
    if (descriptor == NULL) {
        return TABOS_APP_RESULT_NOT_FOUND;
    }

    active_context = (tabos_app_context_t){
        .descriptor = descriptor,
    };
    if ((descriptor->capabilities & TABOS_APP_CAPABILITY_CONSOLE) != 0U) {
        if (!tabos_console_acquire(&active_context.console)) {
            active_context = (tabos_app_context_t){0};
            return TABOS_APP_RESULT_START_FAILED;
        }
        active_context.console_owned = true;
    }

    running = true;
    last_exit_valid = false;
    if (!descriptor->entry(&active_context)) {
        active_context.exit_status = -1;
        finish_active_application();
        return TABOS_APP_RESULT_START_FAILED;
    }
    if (active_context.exit_requested) {
        finish_active_application();
    }
    return TABOS_APP_RESULT_OK;
}

bool tabos_app_is_running(void)
{
    return running;
}

const tabos_app_descriptor_t *tabos_app_active(void)
{
    return running ? active_context.descriptor : NULL;
}

void tabos_app_request_exit(tabos_app_context_t *context, int exit_status)
{
    if (running && context == &active_context) {
        active_context.exit_requested = true;
        active_context.exit_status = exit_status;
    }
}

const tabos_console_session_t *tabos_app_console(const tabos_app_context_t *context)
{
    return running && context == &active_context && active_context.console_owned
        ? &active_context.console
        : NULL;
}

bool tabos_app_last_exit_status(int *exit_status)
{
    if (!last_exit_valid || exit_status == NULL) {
        return false;
    }
    *exit_status = last_exit_status;
    return true;
}
