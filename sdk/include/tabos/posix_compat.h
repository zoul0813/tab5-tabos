#ifndef TABOS_POSIX_COMPAT_H
#define TABOS_POSIX_COMPAT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <tabos/filesystem.h>

typedef struct tabos_posix_dir {
    tabos_dir_t handle;
    tabos_dirent_t native_entry;
    struct {
        uint8_t d_type;
        char d_name[TABOS_FS_NAME_MAX + 1];
    } entry;
    bool allocated;
    bool runtime_backed;
    size_t listing_offset;
    char listing[4096];
} tabos_posix_dir_t;

typedef struct {
    uint32_t st_mode;
    uint64_t st_size;
    int64_t st_mtime;
} tabos_posix_stat_t;

int tabos_posix_open(const char *path, int flags, ...);
int tabos_posix_close(int descriptor);
tabos_ssize_t tabos_posix_read(int descriptor, void *buffer, size_t count);
tabos_ssize_t tabos_posix_write(int descriptor, const void *buffer, size_t count);
tabos_off_t tabos_posix_lseek(int descriptor, tabos_off_t offset, int whence);
int tabos_posix_stat(const char *path, void *status);
int tabos_posix_fstat(int descriptor, void *status);
int tabos_posix_unlink(const char *path);
int tabos_posix_rename(const char *old_path, const char *new_path);
int tabos_posix_mkdir(const char *path, uint32_t mode);
int tabos_posix_rmdir(const char *path);
int tabos_posix_chdir(const char *path);
char *tabos_posix_getcwd(char *buffer, size_t size);
tabos_posix_dir_t *tabos_posix_opendir(const char *path);
void *tabos_posix_readdir(tabos_posix_dir_t *directory);
int tabos_posix_closedir(tabos_posix_dir_t *directory);

#endif
