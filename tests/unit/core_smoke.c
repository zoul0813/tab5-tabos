#include <tabos/internal/runtime.h>
#include <tabos/terminal.h>

#include <string.h>

int main(void)
{
    if (!tabos_runtime_init()) {
        return 1;
    }

    if (!tabos_runtime_init()) {
        return 1;
    }

    if (strlen(tabos_runtime_version()) == 0U) {
        return 1;
    }

    if (tabos_terminal_set_scale(0U) || tabos_terminal_set_scale(9U) ||
        !tabos_terminal_set_scale(3U) || tabos_terminal_get_scale() != 3U) {
        return 1;
    }

    if (!tabos_runtime_start()) {
        return 1;
    }

    if (!tabos_terminal_set_scale(4U) || tabos_terminal_get_scale() != 4U) {
        return 1;
    }

    tabos_runtime_shutdown();
    return 0;
}
