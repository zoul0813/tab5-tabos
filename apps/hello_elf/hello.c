#include <tabos/elf_api.h>

int tabos_elf_entry(const tabos_elf_api_t *api)
{
    if (api == 0 || api->abi_version != TABOS_ELF_API_VERSION) {
        return 2;
    }
    api->console_write("Hello from independent TabOS ELF");
    api->request_exit(0);
    return 0;
}
