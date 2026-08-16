#ifndef TABOS_INTERNAL_ELF_APPLICATION_H
#define TABOS_INTERNAL_ELF_APPLICATION_H

#include <tabos/application.h>

typedef struct loader_elf_application loader_elf_application_t;

loader_elf_application_t *loader_elf_application_create(const char *path,
                                                        size_t argc,
                                                        const char *const *argv);
const tabos_app_descriptor_t *loader_elf_application_descriptor(
    const loader_elf_application_t *application);
void loader_elf_application_destroy(void *application);
const char *loader_elf_application_working_directory(
    const loader_elf_application_t *application);
bool loader_elf_application_set_working_directory(loader_elf_application_t *application,
                                                  const char *working_directory);

#endif
