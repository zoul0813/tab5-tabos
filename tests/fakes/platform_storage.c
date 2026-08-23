#include <tabos/platform/storage.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

bool platform_storage_init(void)
{
    return false;
}

void platform_storage_shutdown(void)
{
}

size_t platform_storage_drive_count(void)
{
    return 0U;
}

bool platform_storage_info(size_t index, platform_storage_info_t* info)
{
    if (info == NULL) {
        return false;
    }
    *info = (platform_storage_info_t) {0};
    return true;
}

bool platform_storage_has_drive(char letter)
{
    return false;
}
char platform_storage_default_drive(void)
{
    return '\0';
}

#define TABOS_FAKE_STORAGE_ERROR_FUNCTION(name, arguments) \
    int name arguments                                     \
    {                                                      \
        return TABOS_ENOTSUP;                              \
    }

TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_open,
                                  (char drive, const char* path, int flags, uint32_t mode, platform_file_t* file))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_close, (platform_file_t file))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_read,
                                  (platform_file_t file, void* buffer, size_t count, size_t* bytes_read))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_write,
                                  (platform_file_t file, const void* buffer, size_t count, size_t* bytes_written))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_seek,
                                  (platform_file_t file, tabos_off_t offset, int whence, tabos_off_t* position))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_stat, (char drive, const char* path, tabos_stat_t* status))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_fstat, (platform_file_t file, tabos_stat_t* status))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_unlink, (char drive, const char* path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_rename, (char drive, const char* old_path, const char* new_path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_mkdir, (char drive, const char* path, uint32_t mode))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_rmdir, (char drive, const char* path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_opendir, (char drive, const char* path, platform_dir_t* directory))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_readdir,
                                  (platform_dir_t directory, tabos_dirent_t* entry, bool* end))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(platform_storage_closedir, (platform_dir_t directory))
