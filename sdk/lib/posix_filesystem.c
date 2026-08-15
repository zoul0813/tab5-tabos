#include <tabos/posix_compat.h>

#include <stdarg.h>
#include <string.h>

enum { TABOS_POSIX_MAX_DIRECTORIES = 8 };

static tabos_posix_dir_t directory_pool[TABOS_POSIX_MAX_DIRECTORIES];

int tabos_posix_open(const char *path, int flags, ...)
{
    uint32_t mode = 0U;
    if ((flags & TABOS_O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (uint32_t)va_arg(arguments, unsigned int);
        va_end(arguments);
    }
    return tabos_fs_open(path, flags, mode);
}

int tabos_posix_close(int descriptor)
{
    return tabos_fs_close(descriptor);
}

tabos_ssize_t tabos_posix_read(int descriptor, void *buffer, size_t count)
{
    return tabos_fs_read(descriptor, buffer, count);
}

tabos_ssize_t tabos_posix_write(int descriptor, const void *buffer, size_t count)
{
    return tabos_fs_write(descriptor, buffer, count);
}

tabos_off_t tabos_posix_lseek(int descriptor, tabos_off_t offset, int whence)
{
    return tabos_fs_seek(descriptor, offset, whence);
}

static int convert_status(const tabos_stat_t *source, void *destination)
{
    if (source == NULL || destination == NULL) {
        *tabos_errno_location() = TABOS_EINVAL;
        return -1;
    }
    const tabos_posix_stat_t converted = {
        .st_mode = source->mode,
        .st_size = source->size,
        .st_mtime = source->modified_time,
    };
    memcpy(destination, &converted, sizeof(converted));
    return 0;
}

int tabos_posix_stat(const char *path, void *status)
{
    tabos_stat_t native_status;
    return tabos_fs_stat(path, &native_status) == 0
        ? convert_status(&native_status, status) : -1;
}

int tabos_posix_fstat(int descriptor, void *status)
{
    tabos_stat_t native_status;
    return tabos_fs_fstat(descriptor, &native_status) == 0
        ? convert_status(&native_status, status) : -1;
}

int tabos_posix_unlink(const char *path) { return tabos_fs_unlink(path); }
int tabos_posix_rename(const char *old_path, const char *new_path)
{
    return tabos_fs_rename(old_path, new_path);
}
int tabos_posix_mkdir(const char *path, uint32_t mode) { return tabos_fs_mkdir(path, mode); }
int tabos_posix_rmdir(const char *path) { return tabos_fs_rmdir(path); }
int tabos_posix_chdir(const char *path) { return tabos_fs_chdir(path); }
char *tabos_posix_getcwd(char *buffer, size_t size) { return tabos_fs_getcwd(buffer, size); }

tabos_posix_dir_t *tabos_posix_opendir(const char *path)
{
    for (size_t index = 0U; index < TABOS_POSIX_MAX_DIRECTORIES; ++index) {
        if (directory_pool[index].allocated) continue;
        const tabos_dir_t handle = tabos_fs_opendir(path);
        if (handle < 0) return NULL;
        directory_pool[index] = (tabos_posix_dir_t){
            .handle = handle,
            .allocated = true,
        };
        return &directory_pool[index];
    }
    *tabos_errno_location() = TABOS_EMFILE;
    return NULL;
}

void *tabos_posix_readdir(tabos_posix_dir_t *directory)
{
    if (directory == NULL || !directory->allocated) {
        *tabos_errno_location() = TABOS_EBADF;
        return NULL;
    }
    const int result = tabos_fs_readdir(directory->handle, &directory->native_entry);
    if (result <= 0) return NULL;
    directory->entry.d_type = (directory->native_entry.mode & TABOS_S_IFDIR) != 0U ? 2U : 1U;
    memcpy(directory->entry.d_name, directory->native_entry.name,
           strlen(directory->native_entry.name) + 1U);
    return &directory->entry;
}

int tabos_posix_closedir(tabos_posix_dir_t *directory)
{
    if (directory == NULL || !directory->allocated) {
        *tabos_errno_location() = TABOS_EBADF;
        return -1;
    }
    const int result = tabos_fs_closedir(directory->handle);
    directory->allocated = false;
    return result;
}
