#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/platform/storage_backend.h>

#include "platform_test.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>

static char storage_root[] = "/tmp/tabos-filesystem-test.XXXXXX";

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char *letter, char *root, size_t root_size,
                           bool *removable, const char **name)
{
    if (index != 0U || letter == NULL || root == NULL || removable == NULL || name == NULL ||
        mkdtemp(storage_root) == NULL) {
        return false;
    }
    const size_t length = strlen(storage_root);
    if (length >= root_size) return false;
    memcpy(root, storage_root, length + 1U);
    *letter = 'A';
    *removable = false;
    *name = "Test filesystem";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void)letter;
    (void)rmdir(storage_root);
}

bool storage_backend_info(char letter, uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (letter != 'A' || total_bytes == NULL || free_bytes == NULL) return false;
    struct statvfs status;
    if (statvfs(storage_root, &status) != 0) return false;
    *total_bytes = (uint64_t)status.f_blocks * (uint64_t)status.f_frsize;
    *free_bytes = (uint64_t)status.f_bavail * (uint64_t)status.f_frsize;
    return true;
}

const char *test_storage_root(void)
{
    return storage_root;
}
