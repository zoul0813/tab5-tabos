#include <tabos/internal/elf_api.h>

#include <stdio.h>
#include <sys/reent.h>

const tabos_elf_api_t *tabos_runtime_api;
static struct _reent tabos_runtime_reent = _REENT_INIT(tabos_runtime_reent);

struct _reent *__getreent(void)
{
    return &tabos_runtime_reent;
}

extern int main(int argc, char **argv);

__attribute__((section(".text.tabos_elf_entry")))
int tabos_elf_entry(const tabos_elf_api_t *api, int argc, const char *const *argv)
{
    if (api == NULL || api->abi_version != TABOS_ELF_API_VERSION) return 127;
    tabos_runtime_api = api;
    (void)setvbuf(stdin, NULL, _IONBF, 0);
    (void)setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
    const int status = main(argc, (char **)argv);
    (void)fflush(NULL);
    api->request_exit(status);
    return status;
}
