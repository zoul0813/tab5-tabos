#ifndef TABOS_PLATFORM_STORAGE_H
#define TABOS_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/filesystem.h>

typedef uintptr_t tab_platform_file_t;
typedef uintptr_t tab_platform_dir_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
    bool mounted;
    bool removable;
    const char *name;
} tab_platform_storage_info_t;

bool tab_platform_storage_init(void);
void tab_platform_storage_shutdown(void);
bool tab_platform_storage_info(tab_platform_storage_info_t *info);

int tab_platform_storage_open(const char *path, int flags, uint32_t mode,
                              tab_platform_file_t *file);
int tab_platform_storage_close(tab_platform_file_t file);
int tab_platform_storage_read(tab_platform_file_t file, void *buffer, size_t count,
                              size_t *bytes_read);
int tab_platform_storage_write(tab_platform_file_t file, const void *buffer, size_t count,
                               size_t *bytes_written);
int tab_platform_storage_seek(tab_platform_file_t file, tabos_off_t offset, int whence,
                              tabos_off_t *position);
int tab_platform_storage_stat(const char *path, tabos_stat_t *status);
int tab_platform_storage_fstat(tab_platform_file_t file, tabos_stat_t *status);
int tab_platform_storage_unlink(const char *path);
int tab_platform_storage_rename(const char *old_path, const char *new_path);
int tab_platform_storage_mkdir(const char *path, uint32_t mode);
int tab_platform_storage_rmdir(const char *path);
int tab_platform_storage_opendir(const char *path, tab_platform_dir_t *directory);
int tab_platform_storage_readdir(tab_platform_dir_t directory, tabos_dirent_t *entry,
                                 bool *end);
int tab_platform_storage_closedir(tab_platform_dir_t directory);

#endif
