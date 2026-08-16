#ifndef TABOS_FILESYSTEM_H
#define TABOS_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TABOS_FS_PATH_MAX = 512,
    TABOS_FS_NAME_MAX = 255,
};

enum {
    TABOS_O_RDONLY = 0x0000,
    TABOS_O_WRONLY = 0x0001,
    TABOS_O_RDWR = 0x0002,
    TABOS_O_ACCMODE = 0x0003,
    TABOS_O_CREAT = 0x0010,
    TABOS_O_EXCL = 0x0020,
    TABOS_O_TRUNC = 0x0040,
    TABOS_O_APPEND = 0x0080,
    TABOS_O_NONBLOCK = 0x0100,
};

enum {
    TABOS_SEEK_SET = 0,
    TABOS_SEEK_CUR = 1,
    TABOS_SEEK_END = 2,
};

enum {
    TABOS_S_IFREG = 0x0001,
    TABOS_S_IFDIR = 0x0002,
};

enum {
    TABOS_EPERM = 1,
    TABOS_ENOENT = 2,
    TABOS_EIO = 5,
    TABOS_EBADF = 9,
    TABOS_EAGAIN = 11,
    TABOS_ENOMEM = 12,
    TABOS_EACCES = 13,
    TABOS_EEXIST = 17,
    TABOS_EXDEV = 18,
    TABOS_ENODEV = 19,
    TABOS_ENOTDIR = 20,
    TABOS_EISDIR = 21,
    TABOS_EINVAL = 22,
    TABOS_ENFILE = 23,
    TABOS_EMFILE = 24,
    TABOS_ENOSPC = 28,
    TABOS_EROFS = 30,
    TABOS_ENAMETOOLONG = 36,
    TABOS_ENOTEMPTY = 39,
    TABOS_ENOTSUP = 95,
    TABOS_EBUSY = 16,
    TABOS_ECHILD = 10,
};

typedef int32_t tabos_fd_t;
typedef int32_t tabos_dir_t;
typedef int64_t tabos_off_t;
typedef intptr_t tabos_ssize_t;

typedef struct {
    uint32_t mode;
    uint64_t size;
    int64_t modified_time;
} tabos_stat_t;

typedef struct {
    uint32_t mode;
    char name[TABOS_FS_NAME_MAX + 1];
} tabos_dirent_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
    const char *name;
    char letter;
    bool mounted;
    bool removable;
} tabos_drive_info_t;

int *tabos_errno_location(void);
size_t tabos_fs_drive_count(void);
bool tabos_fs_drive_info(size_t index, tabos_drive_info_t *info);

tabos_fd_t tabos_fs_open(const char *path, int flags, uint32_t mode);
int tabos_fs_close(tabos_fd_t descriptor);
tabos_ssize_t tabos_fs_read(tabos_fd_t descriptor, void *buffer, size_t count);
tabos_ssize_t tabos_fs_write(tabos_fd_t descriptor, const void *buffer, size_t count);
tabos_off_t tabos_fs_seek(tabos_fd_t descriptor, tabos_off_t offset, int whence);
int tabos_fs_stat(const char *path, tabos_stat_t *status);
int tabos_fs_fstat(tabos_fd_t descriptor, tabos_stat_t *status);
int tabos_fs_unlink(const char *path);
int tabos_fs_rename(const char *old_path, const char *new_path);
int tabos_fs_mkdir(const char *path, uint32_t mode);
int tabos_fs_rmdir(const char *path);
int tabos_fs_chdir(const char *path);
char *tabos_fs_getcwd(char *buffer, size_t size);

tabos_dir_t tabos_fs_opendir(const char *path);
int tabos_fs_readdir(tabos_dir_t directory, tabos_dirent_t *entry);
int tabos_fs_closedir(tabos_dir_t directory);

#endif
