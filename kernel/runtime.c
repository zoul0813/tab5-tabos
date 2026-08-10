#include <tabos/internal/runtime.h>

#include <tabos/internal/display.h>

#include <tabos/config/identity.h>

static bool runtime_initialized;
static bool runtime_started;

bool tabos_runtime_init(void)
{
    if (runtime_initialized) {
        return true;
    }

    runtime_initialized = true;
    return true;
}

bool tabos_runtime_start(void)
{
    if (!runtime_initialized) {
        return false;
    }

    if (runtime_started) {
        return true;
    }

    if (!tab_display_init()) {
        return false;
    }

    tab_display_render_diagnostic();
    if (!tab_display_present()) {
        tab_display_shutdown();
        return false;
    }

    runtime_started = true;
    return true;
}

void tabos_runtime_shutdown(void)
{
    if (runtime_started) {
        tab_display_shutdown();
        runtime_started = false;
    }

    runtime_initialized = false;
}

const char *tabos_runtime_version(void)
{
    return TABOS_RUNTIME_VERSION;
}
