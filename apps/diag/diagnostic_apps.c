#include <tabos/internal/diagnostic_apps.h>

#include <tabos/internal/application.h>

#include <tabos/config/console.h>
#include <tabos/config/filesystem.h>
#include <tabos/config/loader.h>

#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
#include <tabos/internal/console_diagnostic.h>
#endif

#if TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
#include <tabos/internal/filesystem_diagnostic.h>
#endif

#if TABOS_ENABLE_ELF_LOADER_EXPERIMENT
#include <tabos/internal/elf_loader_diagnostic.h>
#endif

bool diagnostic_apps_register(void)
{
#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    return application_registry_register(&console_diagnostic_app);
#elif TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    return application_registry_register(&filesystem_diagnostic_app);
#elif TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    return application_registry_register(&elf_loader_diagnostic_app);
#else
    return true;
#endif
}

const char* diagnostic_startup_app(void)
{
#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    return console_diagnostic_app.name;
#elif TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    return filesystem_diagnostic_app.name;
#elif TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    return elf_loader_diagnostic_app.name;
#else
    return NULL;
#endif
}
