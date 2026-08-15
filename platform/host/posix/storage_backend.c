#define _POSIX_C_SOURCE 200809L

#include <tabos/platform/storage_backend.h>

#include <tabos/config/filesystem.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

bool storage_backend_mount(char *root, size_t root_size,
                               bool *removable, const char **name)
{
    if (root == NULL || removable == NULL || name == NULL) return false;
    if (mkdir(TABOS_HOST_ROOTFS, 0755) != 0 && errno != EEXIST) return false;
    const size_t length = strlen(TABOS_HOST_ROOTFS);
    if (length == 0U || length >= root_size) return false;
    memcpy(root, TABOS_HOST_ROOTFS, length + 1U);
    *removable = false;
    *name = "Host root filesystem";
    return true;
}

void storage_backend_unmount(void)
{
}

bool storage_backend_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (total_bytes == NULL || free_bytes == NULL) return false;
    struct statvfs status;
    if (statvfs(TABOS_HOST_ROOTFS, &status) != 0) return false;
    *total_bytes = (uint64_t)status.f_blocks * (uint64_t)status.f_frsize;
    *free_bytes = (uint64_t)status.f_bavail * (uint64_t)status.f_frsize;
    return true;
}
