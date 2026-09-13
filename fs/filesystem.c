#include <tabos/filesystem.h>

#include <tabos/config/filesystem.h>
#include <tabos/internal/filesystem.h>
#include <tabos/platform/storage.h>
#include <tabos/platform/platform.h>
#include <tabos/internal/service_admission.h>
#include <tabos/internal/time.h>

#include <stdatomic.h>
#include <string.h>

typedef struct {
        platform_file_t platform_file;
        uint64_t fallback_device_id;
        uint64_t fallback_file_id;
        uint16_t generation;
        bool open;
        bool writable;
} file_slot_t;

typedef struct {
        platform_dir_t platform_directory;
        uint16_t generation;
        bool open;
} directory_slot_t;

static file_slot_t files[TABOS_FILESYSTEM_MAX_FILES];
static directory_slot_t directories[TABOS_FILESYSTEM_MAX_DIRECTORIES];
static platform_mutex_t* filesystem_lock;
static platform_signal_t* filesystem_drained;
static service_admission_t admission;
static _Thread_local bool operation_mutates;
static _Thread_local int filesystem_errno;
static atomic_bool filesystem_initialized;
static bool filesystem_mounted;
static char working_directory[TABOS_FS_PATH_MAX] = "A:/";
static filesystem_power_status_t power_status;
static platform_work_t* sync_work;
static int sync_error;
static uint64_t sync_completed_ms;

static bool lock_filesystem(bool mutation)
{
    if (!filesystem_initialized) {
        filesystem_errno = TABOS_ENODEV;
        return false;
    }
    if (!service_admission_enter(&admission, mutation)) {
        filesystem_errno = TABOS_EBUSY;
        return false;
    }
    operation_mutates = mutation;
    platform_mutex_lock(filesystem_lock);
    return true;
}

static void unlock_filesystem(void)
{
    service_admission_leave(&admission, operation_mutates);
    const unsigned int state = atomic_load(&admission.state);
    if ((state & SERVICE_ADMISSION_MASK) == 0U && (state & SERVICE_ADMISSION_FROZEN) != 0U) {
        platform_signal_notify(filesystem_drained);
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
    }
    platform_mutex_unlock(filesystem_lock);
}

static int fail(int error)
{
    filesystem_errno = error;
    return -1;
}

static uint64_t fallback_file_id(const char* resolved)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    while (*resolved != '\0') {
        unsigned char byte = (unsigned char) *resolved++;
        if (byte >= (unsigned char) 'A' && byte <= (unsigned char) 'Z') {
            byte = (unsigned char) (byte - (unsigned char) 'A' + (unsigned char) 'a');
        }
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return hash == 0U ? 1U : hash;
}

static uint64_t fallback_device_id(char drive)
{
    return (uint64_t) (drive - 'A' + 1);
}

static void supply_fallback_identity(const char* resolved, tabos_stat_t* status)
{
    if (status->file_id != 0U) {
        return;
    }
    status->device_id = fallback_device_id(resolved[0]);
    status->file_id   = fallback_file_id(resolved);
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
    if (!lock_filesystem(false)) {
        return 0U;
    }
    const size_t count = platform_storage_drive_count();
    unlock_filesystem();
    return count;
}

bool tabos_fs_drive_info(size_t index, tabos_drive_info_t* info)
{
    if (info == NULL || !lock_filesystem(false)) {
        return false;
    }
    platform_storage_info_t platform_info;
    if (!platform_storage_info(index, &platform_info)) {
        unlock_filesystem();
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
    unlock_filesystem();
    return true;
}

static void sync_storage(void* unused)
{
    (void) unused;
    platform_mutex_lock(filesystem_lock);
    platform_file_t retained[TABOS_FILESYSTEM_MAX_FILES];
    size_t count = 0U;
    for (size_t index = 0U; index < TABOS_FILESYSTEM_MAX_FILES; ++index) {
        if (files[index].open && files[index].writable) {
            retained[count++] = files[index].platform_file;
        }
    }
    sync_error = platform_storage_sync(retained, count);
    platform_mutex_unlock(filesystem_lock);
    sync_completed_ms = platform_time_ms();
}

filesystem_power_status_t filesystem_power_status(void)
{
    filesystem_power_status_t status = power_status;
    const unsigned int state         = atomic_load(&admission.state);
    status.operations                = state & SERVICE_ADMISSION_MASK;
    status.mutations                 = (state / SERVICE_ADMISSION_MUTATION) & SERVICE_ADMISSION_MASK;
    return status;
}

uint64_t filesystem_power_next_deadline(void)
{
    return power_status.deadline_ms;
}

bool filesystem_power_is_frozen(void)
{
    return (atomic_load(&admission.state) & SERVICE_ADMISSION_FROZEN) != 0U;
}

bool filesystem_power_begin(uint64_t now_ms)
{
    if (!filesystem_initialized || sync_work != NULL ||
        (power_status.state != FILESYSTEM_POWER_ACTIVE && power_status.state != FILESYSTEM_POWER_FAILED)) {
        return false;
    }
    service_admission_freeze(&admission, true);
    power_status = (filesystem_power_status_t) {.state       = FILESYSTEM_POWER_DRAINING,
                                                .deadline_ms = time_deadline_after(now_ms, 2000U)};
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
    return true;
}

static void finish_power(int error)
{
    power_status.error       = error;
    power_status.deadline_ms = TIME_DEADLINE_NONE;
    if (error == 0) {
        power_status.state = FILESYSTEM_POWER_READY;
    } else {
        power_status.state = FILESYSTEM_POWER_FAILED;
        service_admission_freeze(&admission, false);
    }
}

void filesystem_power_abort(void)
{
    if (sync_work != NULL) {
        /* The worker still borrows retained handles. Do not admit close/unmount
         * or claim rollback finished before its completion is acknowledged. */
        power_status.state       = FILESYSTEM_POWER_ABORTING;
        power_status.error       = TABOS_ECANCELED;
        power_status.deadline_ms = TIME_DEADLINE_NONE;
        return;
    }
    service_admission_freeze(&admission, false);
    power_status = (filesystem_power_status_t) {.state = FILESYSTEM_POWER_ACTIVE, .deadline_ms = TIME_DEADLINE_NONE};
}

void filesystem_power_update(uint64_t now_ms)
{
    if (sync_work != NULL) {
        if (!platform_work_complete(sync_work)) {
            if (power_status.state == FILESYSTEM_POWER_SYNCING && now_ms >= power_status.deadline_ms) {
                power_status.state       = FILESYSTEM_POWER_ABORTING;
                power_status.error       = TABOS_ETIMEDOUT;
                power_status.deadline_ms = TIME_DEADLINE_NONE;
            }
            return;
        }
        platform_work_release(sync_work);
        sync_work = NULL;
        int error = sync_error;
        if (power_status.state == FILESYSTEM_POWER_ABORTING) {
            error = power_status.error;
        } else if (sync_completed_ms >= power_status.deadline_ms) {
            error = TABOS_ETIMEDOUT;
        }
        finish_power(error);
        return;
    }
    if (power_status.state != FILESYSTEM_POWER_DRAINING) {
        return;
    }
    if (now_ms >= power_status.deadline_ms) {
        finish_power(TABOS_ETIMEDOUT);
        return;
    }
    if ((atomic_load(&admission.state) & SERVICE_ADMISSION_MASK) != 0U) {
        return;
    }
    sync_work = platform_work_start(sync_storage, NULL);
    if (sync_work == NULL) {
        finish_power(TABOS_ENOMEM);
    } else {
        power_status.state = FILESYSTEM_POWER_SYNCING;
    }
}

void filesystem_power_finish_for_shutdown(void)
{
    filesystem_power_abort();
    platform_work_wait(sync_work);
    filesystem_power_update(platform_time_ms());
    filesystem_power_abort();
}

bool filesystem_init(void)
{
    if (filesystem_initialized) {
        return true;
    }
    filesystem_lock    = platform_mutex_create();
    filesystem_drained = platform_signal_create();
    if (filesystem_lock == NULL || filesystem_drained == NULL) {
        platform_mutex_destroy(filesystem_lock);
        platform_signal_destroy(filesystem_drained);
        filesystem_lock    = NULL;
        filesystem_drained = NULL;
        return false;
    }
    atomic_store(&admission.state, 0U);
    power_status = (filesystem_power_status_t) {.state = FILESYSTEM_POWER_ACTIVE, .deadline_ms = TIME_DEADLINE_NONE};
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
    service_admission_freeze(&admission, true);
    platform_work_wait(sync_work);
    platform_work_release(sync_work);
    sync_work = NULL;
    while ((atomic_load(&admission.state) & SERVICE_ADMISSION_MASK) != 0U) {
        platform_signal_wait(filesystem_drained, UINT32_MAX);
    }
    platform_mutex_lock(filesystem_lock);
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
    platform_mutex_unlock(filesystem_lock);
    platform_mutex_destroy(filesystem_lock);
    platform_signal_destroy(filesystem_drained);
    filesystem_lock    = NULL;
    filesystem_drained = NULL;
}

bool filesystem_is_mounted(void)
{
    return filesystem_initialized && filesystem_mounted;
}

tabos_fd_t tabos_fs_open(const char* path, int flags, uint32_t mode)
{
    char resolved[TABOS_FS_PATH_MAX];
    if (!lock_filesystem((flags & (TABOS_O_CREAT | TABOS_O_TRUNC)) != 0)) {
        return -1;
    }
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
    slot->fallback_device_id    = fallback_device_id(resolved[0]);
    slot->fallback_file_id      = fallback_file_id(resolved);
    slot->open                  = true;
    slot->writable              = (flags & TABOS_O_ACCMODE) != TABOS_O_RDONLY;
    const tabos_fd_t descriptor = encode_file(index, slot->generation);
    filesystem_errno            = 0;
    unlock_filesystem();
    return descriptor;
}

int tabos_fs_close(tabos_fd_t descriptor)
{
    if (!lock_filesystem(true)) {
        return -1;
    }
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
    if (!lock_filesystem(false)) {
        return -1;
    }
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
    if (!lock_filesystem(true)) {
        return -1;
    }
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
    if (!lock_filesystem(false)) {
        return -1;
    }
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
    if (!lock_filesystem(false)) {
        return -1;
    }
    if (!resolve_path(path, resolved)) {
        unlock_filesystem();
        return -1;
    }
    const int error = platform_storage_stat(resolved[0], resolved + 2U, status);
    if (error == 0) {
        supply_fallback_identity(resolved, status);
    }
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_fstat(tabos_fd_t descriptor, tabos_stat_t* status)
{
    if (status == NULL) {
        return fail(TABOS_EINVAL);
    }
    if (!lock_filesystem(false)) {
        return -1;
    }
    file_slot_t* slot = file_for_descriptor(descriptor);
    if (slot == NULL) {
        unlock_filesystem();
        return fail(TABOS_EBADF);
    }
    const int error = platform_storage_fstat(slot->platform_file, status);
    if (error == 0 && status->file_id == 0U) {
        status->device_id = slot->fallback_device_id;
        status->file_id   = slot->fallback_file_id;
    }
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

static int path_operation(const char* path, int (*operation)(char, const char*))
{
    char resolved[TABOS_FS_PATH_MAX];
    if (!lock_filesystem(true)) {
        return -1;
    }
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
    if (!lock_filesystem(true)) {
        return -1;
    }
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
    if (!lock_filesystem(true)) {
        return -1;
    }
    if (!resolve_path(old_path, old_resolved) || !resolve_path(new_path, new_resolved)) {
        unlock_filesystem();
        return -1;
    }
    if (old_resolved[0] != new_resolved[0]) {
        unlock_filesystem();
        return fail(TABOS_EXDEV);
    }
    const int error = platform_storage_rename(old_resolved[0], old_resolved + 2U, new_resolved + 2U);
    if (error == 0) {
        const uint64_t old_file_id = fallback_file_id(old_resolved);
        const uint64_t new_file_id = fallback_file_id(new_resolved);
        const uint64_t device_id   = fallback_device_id(old_resolved[0]);
        for (size_t index = 0U; index < TABOS_FILESYSTEM_MAX_FILES; ++index) {
            if (files[index].open && files[index].fallback_device_id == device_id &&
                files[index].fallback_file_id == old_file_id) {
                files[index].fallback_file_id = new_file_id;
            }
        }
    }
    unlock_filesystem();
    return error == 0 ? 0 : fail(error);
}

int tabos_fs_chdir(const char* path)
{
    tabos_stat_t status;
    char resolved[TABOS_FS_PATH_MAX];
    if (!lock_filesystem(false)) {
        return -1;
    }
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
    if (!lock_filesystem(false)) {
        return NULL;
    }
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
    if (!lock_filesystem(false)) {
        return -1;
    }
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
    if (!lock_filesystem(false)) {
        return -1;
    }
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
    if (!lock_filesystem(true)) {
        return -1;
    }
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
