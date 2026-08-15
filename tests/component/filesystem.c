#define _POSIX_C_SOURCE 200809L

#include <tabos/filesystem.h>

#include <tabos/internal/filesystem.h>
#include <tabos/platform/storage.h>

#include "platform_test.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool has_entry(tabos_dir_t directory, const char *name)
{
    tabos_dirent_t entry;
    int result;
    while ((result = tabos_fs_readdir(directory, &entry)) > 0) {
        if (strcmp(entry.name, name) == 0) return true;
    }
    return false;
}

int main(void)
{
    char normalized[TABOS_FS_PATH_MAX];
    if (!tab_fs_normalize_path("/apps//./bin/../hello", "/", normalized,
                               sizeof(normalized)) ||
        strcmp(normalized, "/apps/hello") != 0 ||
        !tab_fs_normalize_path("../../etc", "/home/user", normalized,
                               sizeof(normalized)) ||
        strcmp(normalized, "/etc") != 0 ||
        tab_fs_normalize_path("", "/", normalized, sizeof(normalized))) {
        return 1;
    }

    if (!tab_fs_init() || !tab_fs_is_mounted()) return 1;
    tab_platform_storage_info_t storage;
    if (!tab_platform_storage_info(&storage) || !storage.mounted ||
        storage.total_bytes == 0U || storage.name == NULL) {
        return 1;
    }

    if (tabos_fs_mkdir("/apps", 0755U) != 0 || tabos_fs_chdir("/apps") != 0) return 1;
    char working_directory[TABOS_FS_PATH_MAX];
    if (tabos_fs_getcwd(working_directory, sizeof(working_directory)) == NULL ||
        strcmp(working_directory, "/apps") != 0) {
        return 1;
    }

    const tabos_fd_t descriptor = tabos_fs_open(
        "hello.txt", TABOS_O_CREAT | TABOS_O_RDWR | TABOS_O_TRUNC, 0644U);
    static const char message[] = "Hello from TabOS filesystem";
    if (descriptor < 0 ||
        tabos_fs_write(descriptor, message, sizeof(message)) != (tabos_ssize_t)sizeof(message) ||
        tabos_fs_seek(descriptor, 0, TABOS_SEEK_SET) != 0) {
        return 1;
    }
    char buffer[sizeof(message)] = {0};
    if (tabos_fs_read(descriptor, buffer, sizeof(buffer)) != (tabos_ssize_t)sizeof(buffer) ||
        memcmp(buffer, message, sizeof(message)) != 0) {
        return 1;
    }
    tabos_stat_t status;
    if (tabos_fs_fstat(descriptor, &status) != 0 || status.size != sizeof(message) ||
        (status.mode & TABOS_S_IFREG) == 0U || tabos_fs_close(descriptor) != 0 ||
        tabos_fs_close(descriptor) == 0 || *tabos_errno_location() != TABOS_EBADF) {
        return 1;
    }

    if (tabos_fs_rename("hello.txt", "renamed.txt") != 0 ||
        tabos_fs_stat("/apps/renamed.txt", &status) != 0 || status.size != sizeof(message)) {
        return 1;
    }
    tabos_dir_t directory = tabos_fs_opendir(".");
    if (directory < 0 || !has_entry(directory, "renamed.txt") ||
        tabos_fs_closedir(directory) != 0 || tabos_fs_closedir(directory) == 0) {
        return 1;
    }

    char symlink_path[TABOS_FS_PATH_MAX];
    const int printed = snprintf(symlink_path, sizeof(symlink_path), "%s/apps/escape",
                                 tab_test_storage_root());
    if (printed <= 0 || (size_t)printed >= sizeof(symlink_path) ||
        symlink("/tmp", symlink_path) != 0 ||
        tabos_fs_stat("escape", &status) == 0 || *tabos_errno_location() != TABOS_EACCES ||
        unlink(symlink_path) != 0) {
        return 1;
    }

    if (tabos_fs_unlink("renamed.txt") != 0 || tabos_fs_chdir("/") != 0 ||
        tabos_fs_rmdir("/apps") != 0 || tabos_fs_stat("/apps", &status) == 0 ||
        *tabos_errno_location() != TABOS_ENOENT) {
        return 1;
    }

    tab_fs_shutdown();
    return 0;
}
