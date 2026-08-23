#include <tabos/internal/elf_api.h>

int tabos_elf_entry(const tabos_elf_api_t *api, int argc, const char *const *argv)
{
    if (api == 0 || api->abi_version != TABOS_ELF_API_VERSION) return 2;
    if (api->tty_set_mode(0, 1U) != 0 || api->tty_get_mode(0) != 1) return 3;
    api->console_write("Hello TabOS!");
    for (int index = 0; index < argc; ++index) {
        api->console_write_raw("argv: ");
        api->console_write(argv[index]);
    }
    api->request_exit(0);
    return 0;
}
