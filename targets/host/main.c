#include <tabos/internal/runtime.h>
#include <tabos/platform/platform.h>

#include <tabos/application.h>

#include <tabos/config/filesystem.h>
#include <tabos/config/identity.h>

#include <SDL3/SDL_main.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool is_smoke_test(int argc, char** argv)
{
    return argc == 2 && strcmp(argv[1], "--smoke") == 0;
}

int main(int argc, char** argv)
{
    const bool headless = is_smoke_test(argc, argv);
    int result          = 0;
    bool restart        = false;
    do {
        restart = false;
        if (!kernel_runtime_init()) {
            fprintf(stderr, "%s runtime initialization failed\n", TABOS_SYSTEM_NAME);
            return 1;
        }
        if (!platform_init(headless)) {
            kernel_runtime_shutdown();
            return 1;
        }
        if (!kernel_runtime_start(!headless)) {
            fprintf(stderr, "%s runtime startup failed\n", TABOS_SYSTEM_NAME);
            kernel_runtime_shutdown();
            platform_shutdown();
            return 1;
        }
        printf("%s %s on %s\n", TABOS_SYSTEM_NAME, kernel_runtime_version(), platform_name());
        result                                = platform_run(kernel_runtime_update);
        const platform_system_action_t action = kernel_runtime_take_system_action();
#if TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
        int diagnostic_status = -1;
        if (!tabos_app_last_exit_status(&diagnostic_status) || diagnostic_status != 0) {
            fprintf(stderr, "Filesystem diagnostic failed with status %d\n", diagnostic_status);
            result = 1;
        }
#endif
        kernel_runtime_shutdown();
        platform_shutdown();
        platform_perform_system_action(action);
        restart = action == PLATFORM_SYSTEM_ACTION_REBOOT;
    } while (restart);
    return result;
}
