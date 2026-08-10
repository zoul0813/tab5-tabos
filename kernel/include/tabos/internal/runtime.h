#ifndef TABOS_INTERNAL_RUNTIME_H
#define TABOS_INTERNAL_RUNTIME_H

#include <stdbool.h>

bool tabos_runtime_init(void);
bool tabos_runtime_start(void);
void tabos_runtime_shutdown(void);
const char *tabos_runtime_version(void);

#endif
