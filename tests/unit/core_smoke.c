#include <tabos/internal/runtime.h>

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

    tabos_runtime_shutdown();
    return 0;
}
