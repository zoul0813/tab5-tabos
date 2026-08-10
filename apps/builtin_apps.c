#include <tabos/internal/builtin_apps.h>

#include <tabos/internal/application.h>

#include <tabos/config/console.h>

#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
#include <tabos/internal/console_diagnostic.h>
#endif

bool tab_builtin_apps_register(void)
{
#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    return tab_app_registry_register(&tab_console_diagnostic_app);
#else
    return true;
#endif
}

const char *tab_builtin_startup_app(void)
{
#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    return tab_console_diagnostic_app.name;
#else
    return NULL;
#endif
}
