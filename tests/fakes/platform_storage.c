#include <tabos/platform/storage.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

bool tab_platform_storage_init(void)
{
    return false;
}

void tab_platform_storage_shutdown(void)
{
}

bool tab_platform_storage_info(tab_platform_storage_info_t *info)
{
    if (info == NULL) return false;
    *info = (tab_platform_storage_info_t){0};
    return true;
}

#define TABOS_FAKE_STORAGE_ERROR_FUNCTION(name, arguments) \
    int name arguments { return TABOS_ENOTSUP; }

TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_open,
    (const char *path, int flags, uint32_t mode, tab_platform_file_t *file))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_close, (tab_platform_file_t file))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_read,
    (tab_platform_file_t file, void *buffer, size_t count, size_t *bytes_read))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_write,
    (tab_platform_file_t file, const void *buffer, size_t count, size_t *bytes_written))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_seek,
    (tab_platform_file_t file, tabos_off_t offset, int whence, tabos_off_t *position))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_stat,
    (const char *path, tabos_stat_t *status))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_fstat,
    (tab_platform_file_t file, tabos_stat_t *status))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_unlink, (const char *path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_rename,
    (const char *old_path, const char *new_path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_mkdir,
    (const char *path, uint32_t mode))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_rmdir, (const char *path))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_opendir,
    (const char *path, tab_platform_dir_t *directory))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_readdir,
    (tab_platform_dir_t directory, tabos_dirent_t *entry, bool *end))
TABOS_FAKE_STORAGE_ERROR_FUNCTION(tab_platform_storage_closedir,
    (tab_platform_dir_t directory))
