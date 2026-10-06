#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>

#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;

static int list_empty_directory(const char* path, char* buffer, uint32_t capacity)
{
    if (path == NULL || buffer == NULL || capacity == 0U) {
        return -TABOS_EINVAL;
    }
    buffer[0] = '\0';
    return 0;
}

static unsigned char records[4096];
static int record_size;

static int list_records(const char* path, char* buffer, uint32_t capacity)
{
    (void) path;
    assert(capacity == sizeof(records));
    memcpy(buffer, records, sizeof(records));
    return record_size;
}

static void expect_invalid_listing(int size)
{
    record_size                  = size;
    tabos_posix_dir_t* directory = tabos_posix_opendir("/records");
    assert(directory != NULL);
    errno = 0;
    assert(tabos_posix_readdir(directory) == NULL && errno == EIO);
    assert(tabos_posix_closedir(directory) == 0);
}

static void test_records(void)
{
    const tabos_elf_api_t api = {.fs_list = list_records};
    tabos_runtime_api         = &api;
    for (unsigned int byte = 1U; byte <= 255U; ++byte) {
        if (byte == '/') {
            continue;
        }
        records[0]                   = 'F';
        records[1]                   = 2U;
        records[2]                   = 0U;
        records[3]                   = 'x';
        records[4]                   = (unsigned char) byte;
        records[5]                   = 'D';
        records[6]                   = 1U;
        records[7]                   = 0U;
        records[8]                   = 'd';
        record_size                  = 9;
        tabos_posix_dir_t* directory = tabos_posix_opendir("/records");
        assert(directory != NULL && tabos_posix_readdir(directory) != NULL);
        assert(directory->entry.d_type == 1U && directory->entry.d_name[0] == 'x');
        assert((unsigned char) directory->entry.d_name[1] == byte && directory->entry.d_name[2] == '\0');
        assert(tabos_posix_readdir(directory) != NULL && directory->entry.d_type == 2U);
        assert(strcmp(directory->entry.d_name, "d") == 0);
        errno = 0;
        assert(tabos_posix_readdir(directory) == NULL && errno == 0);
        assert(tabos_posix_closedir(directory) == 0);
    }
    records[0] = 'F';
    records[1] = 255U;
    records[2] = 0U;
    memset(records + 3U, 'x', 255U);
    record_size                  = 258;
    tabos_posix_dir_t* directory = tabos_posix_opendir("/records");
    assert(directory != NULL && tabos_posix_readdir(directory) != NULL);
    assert(strlen(directory->entry.d_name) == TABOS_FS_NAME_MAX);
    assert(tabos_posix_closedir(directory) == 0);
    expect_invalid_listing(1);   /* truncated header */
    expect_invalid_listing(257); /* truncated name */
    records[2] = 1U;
    expect_invalid_listing(258); /* oversized name */
    records[2] = 0U;
    records[0] = 'X';
    expect_invalid_listing(258); /* invalid type */
    records[0] = 'F';
    records[1] = 0U;
    expect_invalid_listing(3); /* empty name */
    records[1] = 1U;
    records[3] = 0U;
    expect_invalid_listing(4); /* embedded NUL */
    records[3] = '/';
    expect_invalid_listing(4); /* path separator */
    record_size = 4097;
    for (unsigned int attempt = 0U; attempt < 16U; ++attempt) {
        assert(tabos_posix_opendir("/records") == NULL && errno == EIO);
    }
    record_size = 0;
    directory   = tabos_posix_opendir("/records");
    assert(directory != NULL && tabos_posix_readdir(directory) == NULL);
    assert(tabos_posix_closedir(directory) == 0);
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
    assert(listing != NULL && tabos_posix_readdir(listing) == NULL && tabos_posix_closedir(listing) == 0);
    test_records();
    return 0;
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
