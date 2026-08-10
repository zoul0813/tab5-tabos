#ifndef TABOS_PLATFORM_PLATFORM_H
#define TABOS_PLATFORM_PLATFORM_H

#include <stdbool.h>

bool tab_platform_init(bool headless);
int tab_platform_run(void);
void tab_platform_shutdown(void);
const char *tab_platform_name(void);

#endif
