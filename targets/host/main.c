#include <tabos/internal/runtime.h>
#include <tabos/platform/platform.h>

#include <tabos/application.h>

#include <tabos/config/filesystem.h>
#include <tabos/config/identity.h>

#include <SDL3/SDL_main.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool is_smoke_test(int argc, char **argv)
{
    return argc == 2 && strcmp(argv[1], "--smoke") == 0;
}

int main(int argc, char **argv)
{
    const bool headless = is_smoke_test(argc, argv);

    if (!tabos_runtime_init()) {
        fprintf(stderr, "%s runtime initialization failed\n", TABOS_SYSTEM_NAME);
        return 1;
    }

    if (!tab_platform_init(headless)) {
        tabos_runtime_shutdown();
        return 1;
    }

    if (!tabos_runtime_start()) {
        fprintf(stderr, "%s display initialization failed\n", TABOS_SYSTEM_NAME);
        tabos_runtime_shutdown();
        tab_platform_shutdown();
        return 1;
    }

    printf("%s %s on %s\n", TABOS_SYSTEM_NAME, tabos_runtime_version(), tab_platform_name());

    int result = tab_platform_run(tabos_runtime_update);
#if TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    int diagnostic_status = -1;
    if (!tabos_app_last_exit_status(&diagnostic_status) || diagnostic_status != 0) {
        fprintf(stderr, "Filesystem diagnostic failed with status %d\n", diagnostic_status);
        result = 1;
    }
#endif
    tabos_runtime_shutdown();
    tab_platform_shutdown();
    return result;
}
