#include <tabos/filesystem.h>

#include <tabos/config/filesystem.h>
#include <tabos/internal/filesystem.h>
#include <tabos/platform/storage.h>

#include <stdatomic.h>
#include <string.h>

typedef struct {
        platform_file_t platform_file;
        uint16_t generation;
        bool open;
} file_slot_t;

typedef struct {
        platform_dir_t platform_directory;
        uint16_t generation;
        bool open;
} directory_slot_t;

static file_slot_t files[TABOS_FILESYSTEM_MAX_FILES];
static directory_slot_t directories[TABOS_FILESYSTEM_MAX_DIRECTORIES];
static atomic_flag filesystem_lock = ATOMIC_FLAG_INIT;
static _Thread_local int filesystem_errno;
static bool filesystem_initialized;
static bool filesystem_mounted;
static char working_directory[TABOS_FS_PATH_MAX] = "A:/";

static void lock_filesystem(void)
{
    while (atomic_flag_test_and_set_explicit(&filesystem_lock, memory_order_acquire)) {}
}

static void unlock_filesystem(void)
{
    atomic_flag_clear_explicit(&filesystem_lock, memory_order_release);
}

static int fail(int error)
{
    filesystem_errno = error;
    return -1;
}

static bool resolve_path(const char* path, char resolved[TABOS_FS_PATH_MAX])
{
    if (!filesystem_initialized || !filesystem_mounted) {
        (void) fail(TABOS_ENODEV);
        return false;
    }
    if (!filesystem_normalize_path(path, working_directory, resolved, TABOS_FS_PATH_MAX)) {
        (void) fail(path != NULL && strlen(path) >= TABOS_FS_PATH_MAX ? TABOS_ENAMETOOLONG : TABOS_EINVAL);
        return false;
    }
    if (!platform_storage_has_drive(resolved[0])) {
        (void) fail(TABOS_ENODEV);
        return false;
    }
    return true;
}

static tabos_fd_t encode_file(size_t index, uint16_t generation)
{
    return (tabos_fd_t) (((uint32_t) generation << 8U) | (uint32_t) (index + 1U));
}

static file_slot_t* file_for_descriptor(tabos_fd_t descriptor)
{
    const uint32_t value         = (uint32_t) descriptor;
    const uint32_t encoded_index = value & 0xffU;
    const uint16_t generation    = (uint16_t) (value >> 8U);
    if (descriptor <= 0 || encoded_index == 0U || encoded_index > TABOS_FILESYSTEM_MAX_FILES) {
        return NULL;
    }
    file_slot_t* slot = &files[encoded_index - 1U];
    return slot->open && slot->generation == generation ? slot : NULL;
}

static tabos_dir_t encode_directory(size_t index, uint16_t generation)
{
    return (tabos_dir_t) (((uint32_t) generation << 8U) | (uint32_t) (index + 1U));
}

static directory_slot_t* directory_for_descriptor(tabos_dir_t descriptor)
{
    const uint32_t value         = (uint32_t) descriptor;
    const uint32_t encoded_index = value & 0xffU;
    const uint16_t generation    = (uint16_t) (value >> 8U);
    if (descriptor <= 0 || encoded_index == 0U || encoded_index > TABOS_FILESYSTEM_MAX_DIRECTORIES) {
        return NULL;
    }
    directory_slot_t* slot = &directories[encoded_index - 1U];
    return slot->open && slot->generation == generation ? slot : NULL;
}

int* tabos_errno_location(void)
{
    return &filesystem_errno;
}

size_t tabos_fs_drive_count(void)
{
    return filesystem_initialized ? platform_storage_drive_count() : 0U;
}

bool tabos_fs_drive_info(size_t index, tabos_drive_info_t* info)
{
    if (!filesystem_initialized || info == NULL) {
        return false;
    }
    platform_storage_info_t platform_info;
    if (!platform_storage_info(index, &platform_info)) {
        return false;
    }
    *info = (tabos_drive_info_t) {
        .total_bytes = platform_info.total_bytes,
        .free_bytes  = platform_info.free_bytes,
        .name        = platform_info.name,
        .letter      = platform_info.letter,
        .mounted     = platform_info.mounted,
        .removable   = platform_info.removable,
    };
    return true;
}

bool filesystem_init(void)
{
    if (filesystem_initialized) {
        return true;
    }
    memset(files, 0, sizeof(files));
    memset(directories, 0, sizeof(directories));
    memcpy(working_directory, "A:/", 4U);
    filesystem_errno         = 0;
    filesystem_initialized   = true;
    filesystem_mounted       = platform_storage_init();
    const char default_drive = platform_storage_default_drive();
    if (filesystem_mounted && default_drive != '\0') {
        working_directory[0] = default_drive;
    }
    return true;
}

void filesystem_shutdown(void)
{
    if (!filesystem_initialized) {
        return;
    }
    lock_filesystem();
    for (size_t index = 0; index < TABOS_FILESYSTEM_MAX_FILES; ++index) {
        if (files[index].open) {
            (void) platform_storage_close(files[index].platform_file);
        }
    }
    for (size_t index = 0; index < TABOS_FILESYSTEM_MAX_DIRECTORIES; ++index) {
        if (directories[index].open) {
            (void) platform_storage_closedir(directories[index].platform_directory);
        }
    }
    memset(files, 0, sizeof(files));
    memset(directories, 0, sizeof(directories));
    platform_storage_shutdown();
    filesystem_mounted     = false;
    filesystem_initialized = false;
    unlock_filesystem();
}

bool filesystem_is_mounted(void)
{
    return filesystem_initialized && filesystem_mounted;
}

tabos_fd_t tabos_fs_open(const char* path, int flags, uint32_t mode)
{
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    size_t index = 0U;
    while (index < TABOS_FILESYSTEM_MAX_FILES && files[index].open) {
        ++index;
    }
    if (index == TABOS_FILESYSTEM_MAX_FILES) {
        unlock_filesystem();
        return fail(TABOS_EMFILE);
    }
    platform_file_t platform_file = 0U;
    const int error               = platform_storage_open(resolved[0], resolved + 2U, flags, mode, &platform_file);
    if (error != 0) {
        unlock_filesystem();
        return fail(error);
    }
    file_slot_t* slot = &files[index];
    if (++slot->generation == 0U) {
        ++slot->generation;
    }
    slot->platform_file         = platform_file;
    slot->open                  = true;
    const tabos_fd_t descriptor = encode_file(index, slot->generation);
    filesystem_errno            = 0;
    unlock_filesystem();
    return descriptor;
}

int tabos_fs_close(tabos_fd_t descriptor)
{
    lock_filesystem();
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    const int error = platform_storage_close(slot->platform_file);
    slot->open      = false;
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

tabos_ssize_t tabos_fs_read(tabos_fd_t descriptor, void* buffer, size_t count)
{
    if (buffer == NULL && count != 0U) {
        return fail(TABOS_EINVAL);
    }
    lock_filesystem();
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    size_t bytes_read = 0U;
    const int error   = platform_storage_read(slot->platform_file, buffer, count, &bytes_read);
    unlock_filesystem();
    return error == 0 ? (tabos_ssize_t) bytes_read : fail(error);
}

tabos_ssize_t tabos_fs_write(tabos_fd_t descriptor, const void* buffer, size_t count)
{
    if (buffer == NULL && count != 0U) {
        return fail(TABOS_EINVAL);
    }
    lock_filesystem();
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    size_t bytes_written = 0U;
    const int error      = platform_storage_write(slot->platform_file, buffer, count, &bytes_written);
    unlock_filesystem();
    return error == 0 ? (tabos_ssize_t) bytes_written : fail(error);
}

tabos_off_t tabos_fs_seek(tabos_fd_t descriptor, tabos_off_t offset, int whence)
{
    lock_filesystem();
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    tabos_off_t position = 0;
    const int error      = platform_storage_seek(slot->platform_file, offset, whence, &position);
    unlock_filesystem();
    return error == 0 ? position : fail(error);
}

int tabos_fs_stat(const char* path, tabos_stat_t* status)
{
    if (status == NULL) {
        return fail(TABOS_EINVAL);
    }
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    const int error = platform_storage_stat(resolved[0], resolved + 2U, status);
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_fstat(tabos_fd_t descriptor, tabos_stat_t* status)
{
    if (status == NULL) {
        return fail(TABOS_EINVAL);
    }
    lock_filesystem();
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    const int error = platform_storage_fstat(slot->platform_file, status);
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

static int path_operation(const char* path, int (*operation)(char, const char*))
{
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    const int error = operation(resolved[0], resolved + 2U);
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_unlink(const char* path)
{
    return path_operation(path, platform_storage_unlink);
}

int tabos_fs_rmdir(const char* path)
{
    return path_operation(path, platform_storage_rmdir);
}

int tabos_fs_mkdir(const char* path, uint32_t mode)
{
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    const int error = platform_storage_mkdir(resolved[0], resolved + 2U, mode);
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_rename(const char* old_path, const char* new_path)
{
    char old_resolved[TABOS_FS_PATH_MAX];
    char new_resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(old_path, old_resolved) || !resolve_path(new_path, new_resolved)) {
        unlock_filesystem();
        return -1;
    }
    if (old_resolved[0] != new_resolved[0]) {
        unlock_filesystem();
        return fail(TABOS_EXDEV);
    }
    const int error = platform_storage_rename(old_resolved[0], old_resolved + 2U, new_resolved + 2U);
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_chdir(const char* path)
{
    tabos_stat_t status;
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    const int error = platform_storage_stat(resolved[0], resolved + 2U, &status);
    if (error != 0 || (status.mode & TABOS_S_IFDIR) == 0U) {
        unlock_filesystem();
        return fail(error != 0 ? error : TABOS_ENOTDIR);
    }
    memcpy(working_directory, resolved, strlen(resolved) + 1U);
    unlock_filesystem();
    return 0;
}

char* tabos_fs_getcwd(char* buffer, size_t size)
{
    if (buffer == NULL || size == 0U) {
        (void) fail(TABOS_EINVAL);
        return NULL;
    }
    lock_filesystem();
    const size_t required = strlen(working_directory) + 1U;
    if (required > size) {
        unlock_filesystem();
        (void) fail(TABOS_ENAMETOOLONG);
        return NULL;
    }
    memcpy(buffer, working_directory, required);
    unlock_filesystem();
    return buffer;
}

tabos_dir_t tabos_fs_opendir(const char* path)
{
    char resolved[TABOS_FS_PATH_MAX];
    lock_filesystem();
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    size_t index = 0U;
    while (index < TABOS_FILESYSTEM_MAX_DIRECTORIES && directories[index].open) {
        ++index;
    }
    if (index == TABOS_FILESYSTEM_MAX_DIRECTORIES) {
        unlock_filesystem();
        return fail(TABOS_EMFILE);
    }
    platform_dir_t platform_directory = 0U;
    const int error                   = platform_storage_opendir(resolved[0], resolved + 2U, &platform_directory);
    if (error != 0) {
        unlock_filesystem();
        return fail(error);
    }
    directory_slot_t* slot = &directories[index];
    if (++slot->generation == 0U) {
        ++slot->generation;
    }
    slot->platform_directory     = platform_directory;
    slot->open                   = true;
    const tabos_dir_t descriptor = encode_directory(index, slot->generation);
    unlock_filesystem();
    return descriptor;
}

int tabos_fs_readdir(tabos_dir_t directory, tabos_dirent_t* entry)
{
    if (entry == NULL) {
        return fail(TABOS_EINVAL);
    }
    lock_filesystem();
    directory_slot_t* slot = directory_for_descriptor(directory);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    bool end        = false;
    const int error = platform_storage_readdir(slot->platform_directory, entry, &end);
    unlock_filesystem();
    return error == 0 ? (end ? 0 : 1) : fail(error);
}

int tabos_fs_closedir(tabos_dir_t directory)
{
    lock_filesystem();
    directory_slot_t* slot = directory_for_descriptor(directory);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    const int error = platform_storage_closedir(slot->platform_directory);
    slot->open      = false;
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}
