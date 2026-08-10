#include <tabos/internal/runtime.h>

#include <tabos/config/identity.h>

static bool runtime_initialized;

bool tabos_runtime_init(void)
{
    if (runtime_initialized) {
        return true;
    }

    runtime_initialized = true;
    return true;
}

void tabos_runtime_shutdown(void)
{
    runtime_initialized = false;
}

const char *tabos_runtime_version(void)
{
    return TABOS_RUNTIME_VERSION;
}
