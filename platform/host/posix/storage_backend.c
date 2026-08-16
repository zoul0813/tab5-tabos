#define _POSIX_C_SOURCE 200809L

#include <tabos/platform/storage_backend.h>

#include <tabos/config/filesystem.h>
#include <tabos/filesystem.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

/* Host mirrors the currently supported Tab5 volume as the default drive. */
static const char drive_letters[] = {'T', 'A'};

size_t storage_backend_drive_count(void)
{
    return sizeof(drive_letters);
}

bool storage_backend_mount(size_t index, char *letter, char *root, size_t root_size,
                           bool *removable, const char **name)
{
    if (index >= sizeof(drive_letters) || letter == NULL || root == NULL ||
        removable == NULL || name == NULL) return false;
    if (mkdir(TABOS_HOST_ROOTFS, 0755) != 0 && errno != EEXIST) return false;
    const int length = snprintf(root, root_size, "%s/%c", TABOS_HOST_ROOTFS,
                                drive_letters[index]);
    if (length <= 0 || (size_t)length >= root_size) return false;
    if (mkdir(root, 0755) != 0 && errno != EEXIST) return false;
    *letter = drive_letters[index];
    *removable = *letter == 'T';
    *name = "HostFS";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void)letter;
}

bool storage_backend_info(char letter, uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (total_bytes == NULL || free_bytes == NULL) return false;
    char root[TABOS_FS_PATH_MAX];
    const int length = snprintf(root, sizeof(root), "%s/%c", TABOS_HOST_ROOTFS, letter);
    if (length <= 0 || (size_t)length >= sizeof(root)) return false;
    struct statvfs status;
    if (statvfs(root, &status) != 0) return false;
    *total_bytes = (uint64_t)status.f_blocks * (uint64_t)status.f_frsize;
    *free_bytes = (uint64_t)status.f_bavail * (uint64_t)status.f_frsize;
    return true;
}
