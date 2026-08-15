#ifndef TABOS_INTERNAL_FILESYSTEM_H
#define TABOS_INTERNAL_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>

bool tab_fs_init(void);
void tab_fs_shutdown(void);
bool tab_fs_is_mounted(void);
bool tab_fs_normalize_path(const char *path, const char *working_directory,
                           char *output, size_t output_size);

#endif
