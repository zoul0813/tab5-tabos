#ifndef TABOS_INTERNAL_RUNTIME_H
#define TABOS_INTERNAL_RUNTIME_H

#include <stdbool.h>

bool kernel_runtime_init(void);
bool kernel_runtime_start(void);
void kernel_runtime_update(void);
void kernel_runtime_shutdown(void);
const char *kernel_runtime_version(void);

#endif
