#ifndef TABOS_PLATFORM_STORAGE_H
#define TABOS_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/filesystem.h>

typedef uintptr_t platform_file_t;
typedef uintptr_t platform_dir_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
    bool mounted;
    bool removable;
    const char *name;
} platform_storage_info_t;

bool platform_storage_init(void);
void platform_storage_shutdown(void);
bool platform_storage_info(platform_storage_info_t *info);

int platform_storage_open(const char *path, int flags, uint32_t mode,
                              platform_file_t *file);
int platform_storage_close(platform_file_t file);
int platform_storage_read(platform_file_t file, void *buffer, size_t count,
                              size_t *bytes_read);
int platform_storage_write(platform_file_t file, const void *buffer, size_t count,
                               size_t *bytes_written);
int platform_storage_seek(platform_file_t file, tabos_off_t offset, int whence,
                              tabos_off_t *position);
int platform_storage_stat(const char *path, tabos_stat_t *status);
int platform_storage_fstat(platform_file_t file, tabos_stat_t *status);
int platform_storage_unlink(const char *path);
int platform_storage_rename(const char *old_path, const char *new_path);
int platform_storage_mkdir(const char *path, uint32_t mode);
int platform_storage_rmdir(const char *path);
int platform_storage_opendir(const char *path, platform_dir_t *directory);
int platform_storage_readdir(platform_dir_t directory, tabos_dirent_t *entry,
                                 bool *end);
int platform_storage_closedir(platform_dir_t directory);

#endif
