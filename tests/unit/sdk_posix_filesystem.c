#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>

#include <errno.h>
#include <stddef.h>

const tabos_elf_api_t* tabos_runtime_api;

static int list_empty_directory(const char* path, char* buffer, uint32_t capacity)
{
    if (path == NULL || buffer == NULL || capacity == 0U) {
        return -TABOS_EINVAL;
    }
    buffer[0] = '\0';
    return 0;
}

int main(void)
{
    for (size_t attempt = 0U; attempt < 16U; ++attempt) {
        errno = 0;
        if (tabos_posix_opendir("/missing") != NULL || errno != ENOSYS) {
            return 1;
        }
    }

    const tabos_elf_api_t unavailable_api = {0};
    tabos_runtime_api                     = &unavailable_api;
    for (size_t attempt = 0U; attempt < 16U; ++attempt) {
        errno = 0;
        if (tabos_posix_opendir("/missing") != NULL || errno != ENOSYS) {
            return 1;
        }
    }

    const tabos_elf_api_t available_api = {
        .fs_list = list_empty_directory,
    };
    tabos_runtime_api          = &available_api;
    tabos_posix_dir_t* listing = tabos_posix_opendir("/available");
    return listing != NULL && tabos_posix_readdir(listing) == NULL && tabos_posix_closedir(listing) == 0 ? 0 : 1;
}

int* tabos_errno_location(void)
{
    static int error;
    return &error;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define TABOS_UNUSED_FILESYSTEM_STUB(return_type, name, arguments) \
    return_type name arguments                                     \
    {                                                              \
        return (return_type) - 1;                                  \
    }

TABOS_UNUSED_FILESYSTEM_STUB(tabos_fd_t, tabos_fs_open, (const char* path, int flags, uint32_t mode))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_close, (tabos_fd_t descriptor))
TABOS_UNUSED_FILESYSTEM_STUB(tabos_ssize_t, tabos_fs_read, (tabos_fd_t descriptor, void* buffer, size_t count))
TABOS_UNUSED_FILESYSTEM_STUB(tabos_ssize_t, tabos_fs_write, (tabos_fd_t descriptor, const void* buffer, size_t count))
TABOS_UNUSED_FILESYSTEM_STUB(tabos_off_t, tabos_fs_seek, (tabos_fd_t descriptor, tabos_off_t offset, int whence))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_stat, (const char* path, tabos_stat_t* status))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_fstat, (tabos_fd_t descriptor, tabos_stat_t* status))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_unlink, (const char* path))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_rename, (const char* old_path, const char* new_path))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_mkdir, (const char* path, uint32_t mode))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_rmdir, (const char* path))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_chdir, (const char* path))
TABOS_UNUSED_FILESYSTEM_STUB(char*, tabos_fs_getcwd, (char* buffer, size_t size))
TABOS_UNUSED_FILESYSTEM_STUB(tabos_dir_t, tabos_fs_opendir, (const char* path))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_readdir, (tabos_dir_t directory, tabos_dirent_t* entry))
TABOS_UNUSED_FILESYSTEM_STUB(int, tabos_fs_closedir, (tabos_dir_t directory))

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
