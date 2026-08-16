#define _POSIX_C_SOURCE 200809L

#include <tabos/platform/storage.h>
#include <tabos/platform/storage_backend.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

enum { PLATFORM_STORAGE_MAX_DRIVES = 26 };

typedef struct {
    char root[TABOS_FS_PATH_MAX];
    bool mounted;
    bool removable;
    const char *name;
    char letter;
} storage_drive_t;

static storage_drive_t storage_drives[PLATFORM_STORAGE_MAX_DRIVES];
static size_t storage_drive_count;

static storage_drive_t *find_drive(char letter)
{
    for (size_t index = 0U; index < storage_drive_count; ++index) {
        if (storage_drives[index].mounted && storage_drives[index].letter == letter) {
            return &storage_drives[index];
        }
    }
    return NULL;
}

static int map_error(int error)
{
    switch (error) {
        case 0: return 0;
        case EPERM: return TABOS_EPERM;
        case ENOENT: return TABOS_ENOENT;
        case EIO: return TABOS_EIO;
        case EBADF: return TABOS_EBADF;
        case EAGAIN: return TABOS_EAGAIN;
        case ENOMEM: return TABOS_ENOMEM;
        case EACCES: return TABOS_EACCES;
        case EEXIST: return TABOS_EEXIST;
        case ENODEV: return TABOS_ENODEV;
        case ENOTDIR: return TABOS_ENOTDIR;
        case EISDIR: return TABOS_EISDIR;
        case EINVAL: return TABOS_EINVAL;
        case ENFILE: return TABOS_ENFILE;
        case EMFILE: return TABOS_EMFILE;
        case ENOSPC: return TABOS_ENOSPC;
        case EROFS: return TABOS_EROFS;
        case ENAMETOOLONG: return TABOS_ENAMETOOLONG;
        case ENOTEMPTY: return TABOS_ENOTEMPTY;
#ifdef ENOTSUP
        case ENOTSUP: return TABOS_ENOTSUP;
#endif
        default: return TABOS_EIO;
    }
}

static bool translate_path(char drive_letter, const char *path,
                           char output[TABOS_FS_PATH_MAX], const char **drive_root)
{
    storage_drive_t *drive = find_drive(drive_letter);
    if (drive == NULL || path == NULL || path[0] != '/') return false;
    const size_t root_length = strlen(drive->root);
    const size_t path_length = strlen(path);
    if (root_length + path_length >= TABOS_FS_PATH_MAX) return false;
    memcpy(output, drive->root, root_length);
    memcpy(output + root_length, path, path_length + 1U);
    if (drive_root != NULL) *drive_root = drive->root;
    return true;
}

static int reject_symlink_components(const char *root, const char *translated,
                                     bool allow_missing_final)
{
#ifdef ESP_PLATFORM
    (void)root;
    (void)translated;
    (void)allow_missing_final;
    /* ESP-IDF FAT VFS has no symbolic-link support or lstat(). */
    return 0;
#else
    const size_t root_length = strlen(root);
    char partial[TABOS_FS_PATH_MAX];
    const size_t length = strlen(translated);
    if (length >= sizeof(partial)) return TABOS_ENAMETOOLONG;
    memcpy(partial, translated, length + 1U);
    for (size_t index = root_length + 1U; index <= length; ++index) {
        if (partial[index] != '/' && partial[index] != '\0') continue;
        const char saved = partial[index];
        partial[index] = '\0';
        struct stat status;
        if (lstat(partial, &status) != 0) {
            const int error = errno;
            partial[index] = saved;
            if (allow_missing_final && saved == '\0' && error == ENOENT) return 0;
            return map_error(error);
        }
        partial[index] = saved;
        if (S_ISLNK(status.st_mode)) return TABOS_EACCES;
    }
    return 0;
#endif
}

static uint32_t map_mode(mode_t mode)
{
    if (S_ISDIR(mode)) return TABOS_S_IFDIR;
    return TABOS_S_IFREG;
}

static void map_status(const struct stat *source, tabos_stat_t *destination)
{
    *destination = (tabos_stat_t){
        .mode = map_mode(source->st_mode),
        .size = source->st_size >= 0 ? (uint64_t)source->st_size : 0U,
        .modified_time = (int64_t)source->st_mtime,
    };
}

bool platform_storage_init(void)
{
    if (storage_drive_count > 0U) return true;
    memset(storage_drives, 0, sizeof(storage_drives));
    const size_t count = storage_backend_drive_count();
    for (size_t index = 0U; index < count && storage_drive_count < PLATFORM_STORAGE_MAX_DRIVES;
         ++index) {
        storage_drive_t drive = {.name = "Filesystem"};
        if (!storage_backend_mount(index, &drive.letter, drive.root, sizeof(drive.root),
                                   &drive.removable, &drive.name)) continue;
        if (drive.letter < 'A' || drive.letter > 'Z' || find_drive(drive.letter) != NULL) {
            storage_backend_unmount(drive.letter);
            continue;
        }
        drive.mounted = true;
        storage_drives[storage_drive_count++] = drive;
    }
    return storage_drive_count > 0U;
}

void platform_storage_shutdown(void)
{
    for (size_t index = storage_drive_count; index > 0U; --index) {
        storage_backend_unmount(storage_drives[index - 1U].letter);
    }
    memset(storage_drives, 0, sizeof(storage_drives));
    storage_drive_count = 0U;
}

size_t platform_storage_drive_count(void)
{
    return storage_drive_count;
}

bool platform_storage_info(size_t index, platform_storage_info_t *info)
{
    if (info == NULL || index >= storage_drive_count) return false;
    const storage_drive_t *drive = &storage_drives[index];
    *info = (platform_storage_info_t){
        .mounted = drive->mounted,
        .removable = drive->removable,
        .name = drive->name,
        .letter = drive->letter,
    };
    return storage_backend_info(drive->letter, &info->total_bytes, &info->free_bytes);
}

bool platform_storage_has_drive(char letter)
{
    return find_drive(letter) != NULL;
}

char platform_storage_default_drive(void)
{
    return storage_drive_count > 0U ? storage_drives[0].letter : '\0';
}

int platform_storage_open(char drive, const char *path, int flags, uint32_t mode,
                              platform_file_t *file)
{
    if (file == NULL || (flags & TABOS_O_ACCMODE) > TABOS_O_RDWR) return TABOS_EINVAL;
    char translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, path, translated, &root)) return TABOS_ENODEV;
    int error = reject_symlink_components(root, translated, (flags & TABOS_O_CREAT) != 0);
    if (error != 0) return error;
    int native_flags = O_RDONLY;
    if ((flags & TABOS_O_ACCMODE) == TABOS_O_WRONLY) native_flags = O_WRONLY;
    if ((flags & TABOS_O_ACCMODE) == TABOS_O_RDWR) native_flags = O_RDWR;
    if ((flags & TABOS_O_CREAT) != 0) native_flags |= O_CREAT;
    if ((flags & TABOS_O_EXCL) != 0) native_flags |= O_EXCL;
    if ((flags & TABOS_O_TRUNC) != 0) native_flags |= O_TRUNC;
    if ((flags & TABOS_O_APPEND) != 0) native_flags |= O_APPEND;
#ifdef O_NOFOLLOW
    native_flags |= O_NOFOLLOW;
#endif
    const int descriptor = open(translated, native_flags, (mode_t)(mode & 0777U));
    if (descriptor < 0) return map_error(errno);
    *file = (platform_file_t)(unsigned int)descriptor;
    return 0;
}

int platform_storage_close(platform_file_t file)
{
    return close((int)file) == 0 ? 0 : map_error(errno);
}

int platform_storage_read(platform_file_t file, void *buffer, size_t count,
                              size_t *bytes_read)
{
    if (bytes_read == NULL) return TABOS_EINVAL;
    const ssize_t result = read((int)file, buffer, count);
    if (result < 0) return map_error(errno);
    *bytes_read = (size_t)result;
    return 0;
}

int platform_storage_write(platform_file_t file, const void *buffer, size_t count,
                               size_t *bytes_written)
{
    if (bytes_written == NULL) return TABOS_EINVAL;
    const ssize_t result = write((int)file, buffer, count);
    if (result < 0) return map_error(errno);
    *bytes_written = (size_t)result;
    return 0;
}

int platform_storage_seek(platform_file_t file, tabos_off_t offset, int whence,
                              tabos_off_t *position)
{
    if (position == NULL || (whence != TABOS_SEEK_SET && whence != TABOS_SEEK_CUR &&
                             whence != TABOS_SEEK_END)) return TABOS_EINVAL;
    const int native_whence = whence == TABOS_SEEK_SET ? SEEK_SET
        : (whence == TABOS_SEEK_CUR ? SEEK_CUR : SEEK_END);
    const off_t result = lseek((int)file, (off_t)offset, native_whence);
    if (result < 0) return map_error(errno);
    *position = (tabos_off_t)result;
    return 0;
}

int platform_storage_stat(char drive, const char *path, tabos_stat_t *status)
{
    if (status == NULL) return TABOS_EINVAL;
    char translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, path, translated, &root)) return TABOS_ENODEV;
    int error = reject_symlink_components(root, translated, false);
    if (error != 0) return error;
    struct stat native_status;
    if (stat(translated, &native_status) != 0) return map_error(errno);
    map_status(&native_status, status);
    return 0;
}

int platform_storage_fstat(platform_file_t file, tabos_stat_t *status)
{
    if (status == NULL) return TABOS_EINVAL;
    struct stat native_status;
    if (fstat((int)file, &native_status) != 0) return map_error(errno);
    map_status(&native_status, status);
    return 0;
}

static int translated_operation(char drive, const char *path, int (*operation)(const char *))
{
    char translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, path, translated, &root)) return TABOS_ENODEV;
    const int error = reject_symlink_components(root, translated, false);
    if (error != 0) return error;
    return operation(translated) == 0 ? 0 : map_error(errno);
}

int platform_storage_unlink(char drive, const char *path)
{
    return translated_operation(drive, path, unlink);
}

int platform_storage_rmdir(char drive, const char *path)
{
    return translated_operation(drive, path, rmdir);
}

int platform_storage_mkdir(char drive, const char *path, uint32_t mode)
{
    char translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, path, translated, &root)) return TABOS_ENODEV;
    const int error = reject_symlink_components(root, translated, true);
    if (error != 0) return error;
    return mkdir(translated, (mode_t)(mode & 0777U)) == 0 ? 0 : map_error(errno);
}

int platform_storage_rename(char drive, const char *old_path, const char *new_path)
{
    char old_translated[TABOS_FS_PATH_MAX];
    char new_translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, old_path, old_translated, &root) ||
        !translate_path(drive, new_path, new_translated, NULL)) {
        return TABOS_ENODEV;
    }
    int error = reject_symlink_components(root, old_translated, false);
    if (error == 0) error = reject_symlink_components(root, new_translated, true);
    if (error != 0) return error;
    return rename(old_translated, new_translated) == 0 ? 0 : map_error(errno);
}

int platform_storage_opendir(char drive, const char *path, platform_dir_t *directory)
{
    if (directory == NULL) return TABOS_EINVAL;
    char translated[TABOS_FS_PATH_MAX];
    const char *root = NULL;
    if (!translate_path(drive, path, translated, &root)) return TABOS_ENODEV;
    const int error = reject_symlink_components(root, translated, false);
    if (error != 0) return error;
    DIR *native_directory = opendir(translated);
    if (native_directory == NULL) return map_error(errno);
    *directory = (platform_dir_t)native_directory;
    return 0;
}

int platform_storage_readdir(platform_dir_t directory, tabos_dirent_t *entry,
                                 bool *end)
{
    if (entry == NULL || end == NULL) return TABOS_EINVAL;
    DIR *native_directory = (DIR *)directory;
    errno = 0;
    struct dirent *native_entry;
    do {
        native_entry = readdir(native_directory);
    } while (native_entry != NULL &&
             (strcmp(native_entry->d_name, ".") == 0 || strcmp(native_entry->d_name, "..") == 0));
    if (native_entry == NULL) {
        if (errno != 0) return map_error(errno);
        *end = true;
        return 0;
    }
    const size_t name_length = strlen(native_entry->d_name);
    if (name_length > TABOS_FS_NAME_MAX) return TABOS_ENAMETOOLONG;
    memcpy(entry->name, native_entry->d_name, name_length + 1U);
    entry->mode = native_entry->d_type == DT_DIR ? TABOS_S_IFDIR : TABOS_S_IFREG;
    *end = false;
    return 0;
}

int platform_storage_closedir(platform_dir_t directory)
{
    return closedir((DIR *)directory) == 0 ? 0 : map_error(errno);
}
