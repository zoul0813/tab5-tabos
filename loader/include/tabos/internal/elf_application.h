#ifndef TABOS_INTERNAL_ELF_APPLICATION_H
#define TABOS_INTERNAL_ELF_APPLICATION_H

#include <tabos/application.h>

typedef struct loader_elf_application loader_elf_application_t;

loader_elf_application_t *loader_elf_application_create(const char *path);
const tabos_app_descriptor_t *loader_elf_application_descriptor(
    const loader_elf_application_t *application);
void loader_elf_application_destroy(void *application);

#endif
