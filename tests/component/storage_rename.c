#define _POSIX_C_SOURCE 200809L

#include <tabos/filesystem.h>

#include <tabos/internal/filesystem.h>

#include "platform_test.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef enum {
    RENAME_WORKS,
    RENAME_FAILS_BACKUP,
    RENAME_FAILS_INSTALL,
} rename_mode_t;

static rename_mode_t rename_mode;
static unsigned int rename_calls;

int test_storage_rename(const char* old_path, const char* new_path);

int test_storage_rename(const char* old_path, const char* new_path)
{
    ++rename_calls;
    if ((rename_mode == RENAME_FAILS_BACKUP && rename_calls == 2U) ||
        (rename_mode == RENAME_FAILS_INSTALL && rename_calls == 3U)) {
        errno = EIO;
        return -1;
    }
    struct stat status;
    if (lstat(new_path, &status) == 0) {
        errno = EEXIST;
        return -1;
    }
    return rename(old_path, new_path);
}

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "storage rename test failed: %s (errno %d)\n", message, *tabos_errno_location());
        exit(EXIT_FAILURE);
    }
}

static void write_file(const char* path, const char* contents)
{
    const tabos_fd_t file = tabos_fs_open(path, TABOS_O_WRONLY | TABOS_O_CREAT | TABOS_O_TRUNC, 0600U);
    const size_t length   = strlen(contents);
    check(file >= 0 && tabos_fs_write(file, contents, length) == (tabos_ssize_t) length && tabos_fs_close(file) == 0,
          "write fixture");
}

static void expect_file(const char* path, const char* contents)
{
    const tabos_fd_t file = tabos_fs_open(path, TABOS_O_RDONLY, 0U);
    char buffer[32]       = {0};
    const size_t length   = strlen(contents);
    check(file >= 0 && tabos_fs_read(file, buffer, sizeof(buffer)) == (tabos_ssize_t) length &&
              memcmp(buffer, contents, length) == 0 && tabos_fs_close(file) == 0,
          "read expected contents");
}

static void expect_no_recovery_file(void)
{
    DIR* directory = opendir(test_storage_root());
    check(directory != NULL, "open storage root");
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        check(strncmp(entry->d_name, ".tabos-rename-", 14U) != 0, "recovery backup cleaned");
    }
    check(closedir(directory) == 0, "close storage root");
}

static void prepare(const char* old_contents, const char* new_contents)
{
    (void) tabos_fs_unlink("/staged");
    (void) tabos_fs_unlink("/live");
    write_file("/live", old_contents);
    write_file("/staged", new_contents);
    rename_calls = 0U;
}

int main(void)
{
    check(filesystem_init(), "initialize filesystem");

    prepare("old", "new");
    rename_mode = RENAME_WORKS;
    check(tabos_fs_rename("/live", "/live") == 0 && rename_calls == 1U, "same path is a no-op");
    expect_file("/live", "old");
    rename_calls = 0U;
    check(tabos_fs_rename("/staged", "/live") == 0 && rename_calls == 3U, "replace on no-replace backend");
    expect_file("/live", "new");
    expect_no_recovery_file();

    prepare("old-backup", "new-backup");
    rename_mode = RENAME_FAILS_BACKUP;
    check(tabos_fs_rename("/staged", "/live") == -1 && *tabos_errno_location() == TABOS_EIO, "report backup failure");
    expect_file("/live", "old-backup");
    expect_file("/staged", "new-backup");
    expect_no_recovery_file();

    prepare("old-install", "new-install");
    rename_mode = RENAME_FAILS_INSTALL;
    check(tabos_fs_rename("/staged", "/live") == -1 && *tabos_errno_location() == TABOS_EIO, "report install failure");
    expect_file("/live", "old-install");
    expect_file("/staged", "new-install");
    expect_no_recovery_file();

    check(tabos_fs_mkdir("/source-directory", 0700U) == 0 && tabos_fs_mkdir("/destination-directory", 0700U) == 0,
          "create replacement directories");
    rename_mode  = RENAME_WORKS;
    rename_calls = 0U;
    check(tabos_fs_rename("/source-directory", "/destination-directory") == 0 && rename_calls == 3U,
          "replace empty directory");
    tabos_stat_t status;
    check(tabos_fs_stat("/destination-directory", &status) == 0 && (status.mode & TABOS_S_IFDIR) != 0U,
          "replacement directory exists");
    check(tabos_fs_rmdir("/destination-directory") == 0, "remove replacement directory");

    check(tabos_fs_mkdir("/source-directory", 0700U) == 0 && tabos_fs_mkdir("/destination-directory", 0700U) == 0,
          "create nonempty replacement directories");
    write_file("/destination-directory/item", "kept");
    rename_calls = 0U;
    check(tabos_fs_rename("/source-directory", "/destination-directory") == -1 &&
              *tabos_errno_location() == TABOS_ENOTEMPTY && rename_calls == 1U,
          "reject nonempty destination directory");
    expect_file("/destination-directory/item", "kept");
    check(tabos_fs_unlink("/destination-directory/item") == 0 && tabos_fs_rmdir("/destination-directory") == 0 &&
              tabos_fs_rmdir("/source-directory") == 0,
          "remove directory fixtures");

    check(tabos_fs_unlink("/staged") == 0 && tabos_fs_unlink("/live") == 0, "remove fixtures");
    filesystem_shutdown();
    return EXIT_SUCCESS;
}
