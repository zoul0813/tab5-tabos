#include <tabos/posix_compat.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>

enum { TABOS_POSIX_MAX_DIRECTORIES = 8 };

static tabos_posix_dir_t directory_pool[TABOS_POSIX_MAX_DIRECTORIES];
const tabos_elf_api_t *tabos_runtime_api __attribute__((weak));

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
        directory_pool[index] = (tabos_posix_dir_t){
            .allocated = true,
#ifdef TABOS_APPLICATION
            .runtime_backed = true,
#endif
        };
        tabos_dir_t handle =
#ifdef TABOS_APPLICATION
            -1;
        if (tabos_runtime_api != NULL && tabos_runtime_api->fs_list != NULL) {
            const int result = tabos_runtime_api->fs_list(
                path, directory_pool[index].listing, sizeof(directory_pool[index].listing));
            if (result < 0) {
                directory_pool[index].allocated = false;
                errno = -result;
                return NULL;
            }
            handle = 0;
        }
#else
            handle = tabos_fs_opendir(path);
#endif
        if (handle < 0) return NULL;
        directory_pool[index].handle = handle;
        return &directory_pool[index];
    }
    errno = TABOS_EMFILE;
    return NULL;
}

void *tabos_posix_readdir(tabos_posix_dir_t *directory)
{
    if (directory == NULL || !directory->allocated) {
        errno = TABOS_EBADF;
        return NULL;
    }
 #ifdef TABOS_APPLICATION
    if (directory->runtime_backed) {
        const size_t start = directory->listing_offset;
        if (directory->listing[start] == '\0') return NULL;
        const char type = directory->listing[start];
        const size_t name_start = start + 2U;
        size_t end = name_start;
        while (directory->listing[end] != '\0' && directory->listing[end] != '\n') ++end;
        const size_t length = end - name_start;
        if (length > TABOS_FS_NAME_MAX) { errno = TABOS_ENAMETOOLONG; return NULL; }
        memcpy(directory->entry.d_name, directory->listing + name_start, length);
        directory->entry.d_name[length] = '\0';
        directory->entry.d_type = type == 'D' ? 2U : 1U;
        directory->listing_offset = directory->listing[end] == '\n' ? end + 1U : end;
        return &directory->entry;
    }
#else
    tabos_dirent_t entry;
    const int result = tabos_fs_readdir(directory->handle, &entry);
    if (result <= 0) return NULL;
    directory->entry.d_type = (entry.mode & TABOS_S_IFDIR) != 0U ? 2U : 1U;
    memcpy(directory->entry.d_name, entry.name, strlen(entry.name) + 1U);
    return &directory->entry;
#endif
}

int tabos_posix_closedir(tabos_posix_dir_t *directory)
{
    if (directory == NULL || !directory->allocated) {
        errno = TABOS_EBADF;
        return -1;
    }
#ifdef TABOS_APPLICATION
    directory->allocated = false;
    directory->runtime_backed = false;
    return 0;
#else
    const int result = tabos_fs_closedir(directory->handle);
    directory->allocated = false;
    return result;
#endif
}
