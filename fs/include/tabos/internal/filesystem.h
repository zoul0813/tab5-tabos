#ifndef TABOS_INTERNAL_FILESYSTEM_H
#define TABOS_INTERNAL_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>

bool filesystem_init(void);
void filesystem_shutdown(void);
bool filesystem_is_mounted(void);
bool filesystem_normalize_path(const char *path, const char *working_directory,
                           char *output, size_t output_size);

#endif
