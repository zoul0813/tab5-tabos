#include <tabos/posix_compat.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>

enum {
    TABOS_POSIX_DIRECTORY_CAPACITY = 8
};

static tabos_posix_dir_t directory_pool[TABOS_POSIX_DIRECTORY_CAPACITY];
const tabos_elf_api_t* tabos_runtime_api __attribute__((weak));

int tabos_posix_open(const char* path, int flags, ...)
{
    uint32_t mode = 0U;
    if ((flags & TABOS_O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (uint32_t) va_arg(arguments, unsigned int);
        va_end(arguments);
    }
    return tabos_fs_open(path, flags, mode);
}

int tabos_posix_close(int descriptor)
{
    return tabos_fs_close(descriptor);
}

tabos_ssize_t tabos_posix_read(int descriptor, void* buffer, size_t count)
{
    return tabos_fs_read(descriptor, buffer, count);
}

tabos_ssize_t tabos_posix_write(int descriptor, const void* buffer, size_t count)
{
    return tabos_fs_write(descriptor, buffer, count);
}

tabos_off_t tabos_posix_lseek(int descriptor, tabos_off_t offset, int whence)
{
    return tabos_fs_seek(descriptor, offset, whence);
}

static int convert_status(const tabos_stat_t* source, void* destination)
{
    if (source == NULL || destination == NULL) {
        *tabos_errno_location() = TABOS_EINVAL;
        return -1;
    }
    const tabos_posix_stat_t converted = {
        .st_mode  = source->mode,
        .st_size  = source->size,
        .st_mtime = source->modified_time,
        .st_dev   = source->device_id,
        .st_ino   = source->file_id,
    };
    memcpy(destination, &converted, sizeof(converted));
    return 0;
}

int tabos_posix_stat(const char* path, void* status)
{
    tabos_stat_t native_status;
    return tabos_fs_stat(path, &native_status) == 0 ? convert_status(&native_status, status) : -1;
}

int tabos_posix_fstat(int descriptor, void* status)
{
    tabos_stat_t native_status;
    return tabos_fs_fstat(descriptor, &native_status) == 0 ? convert_status(&native_status, status) : -1;
}

int tabos_posix_unlink(const char* path)
{
    return tabos_fs_unlink(path);
}
int tabos_posix_rename(const char* old_path, const char* new_path)
{
    return tabos_fs_rename(old_path, new_path);
}
int tabos_posix_mkdir(const char* path, uint32_t mode)
{
    return tabos_fs_mkdir(path, mode);
}
int tabos_posix_rmdir(const char* path)
{
    return tabos_fs_rmdir(path);
}
int tabos_posix_chdir(const char* path)
{
    return tabos_fs_chdir(path);
}
char* tabos_posix_getcwd(char* buffer, size_t size)
{
    return tabos_fs_getcwd(buffer, size);
}

tabos_posix_dir_t* tabos_posix_opendir(const char* path)
{
    for (size_t index = 0U; index < TABOS_POSIX_DIRECTORY_CAPACITY; ++index) {
        if (directory_pool[index].allocated) {
            continue;
        }
        directory_pool[index] = (tabos_posix_dir_t) {
            .allocated = true,
#ifdef TABOS_APPLICATION
            .runtime_backed = true,
#endif
        };
        tabos_dir_t handle =
#ifdef TABOS_APPLICATION
            -1;
        if (tabos_runtime_api != NULL && tabos_runtime_api->fs_list != NULL) {
            const int result =
                tabos_runtime_api->fs_list(path, directory_pool[index].listing, sizeof(directory_pool[index].listing));
            if (result < 0) {
                errno = -result;
            } else if ((size_t) result > sizeof(directory_pool[index].listing)) {
                errno = EIO;
            } else {
                directory_pool[index].listing_size = (size_t) result;
                handle                             = 0;
            }
        } else {
            errno = ENOSYS;
        }
#else
            handle = tabos_fs_opendir(path);
#endif
        if (handle < 0) {
            directory_pool[index] = (tabos_posix_dir_t) {0};
            return NULL;
        }
        directory_pool[index].handle = handle;
        return &directory_pool[index];
    }
    errno = TABOS_EMFILE;
    return NULL;
}

void* tabos_posix_readdir(tabos_posix_dir_t* directory)
{
    if (directory == NULL || !directory->allocated) {
        errno = TABOS_EBADF;
        return NULL;
    }
#ifdef TABOS_APPLICATION
    if (directory->runtime_backed) {
        const size_t start = directory->listing_offset;
        if (start == directory->listing_size) {
            return NULL;
        }
        if (start > directory->listing_size || directory->listing_size - start < 3U) {
            errno = EIO;
            return NULL;
        }
        const char type = directory->listing[start];
        const size_t length =
            (uint8_t) directory->listing[start + 1U] | (size_t) (uint8_t) directory->listing[start + 2U] << 8U;
        const size_t name_start = start + 3U;
        if ((type != 'F' && type != 'D') || length == 0U || length > TABOS_FS_NAME_MAX ||
            length > directory->listing_size - name_start ||
            memchr(directory->listing + name_start, '\0', length) != NULL ||
            memchr(directory->listing + name_start, '/', length) != NULL) {
            errno = EIO;
            return NULL;
        }
        memcpy(directory->entry.d_name, directory->listing + name_start, length);
        directory->entry.d_name[length] = '\0';
        directory->entry.d_type         = type == 'D' ? 2U : 1U;
        directory->listing_offset = name_start + length;
        return &directory->entry;
    }
#else
    tabos_dirent_t entry;
    const int result = tabos_fs_readdir(directory->handle, &entry);
    if (result <= 0) {
        return NULL;
    }
    directory->entry.d_type = (entry.mode & TABOS_S_IFDIR) != 0U ? 2U : 1U;
    memcpy(directory->entry.d_name, entry.name, strlen(entry.name) + 1U);
    return &directory->entry;
#endif
    return NULL;
}

int tabos_posix_closedir(tabos_posix_dir_t* directory)
{
    if (directory == NULL || !directory->allocated) {
        errno = TABOS_EBADF;
        return -1;
    }
#ifdef TABOS_APPLICATION
    directory->allocated      = false;
    directory->runtime_backed = false;
    return 0;
#else
    const int result     = tabos_fs_closedir(directory->handle);
    directory->allocated = false;
    return result;
#endif
}
