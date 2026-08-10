#include <tabos/internal/runtime.h>
#include <tabos/platform/platform.h>

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

    printf("%s %s on %s\n", TABOS_SYSTEM_NAME, tabos_runtime_version(), tab_platform_name());

    const int result = tab_platform_run();
    tab_platform_shutdown();
    tabos_runtime_shutdown();
    return result;
}
