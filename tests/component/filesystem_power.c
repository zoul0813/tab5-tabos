#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/filesystem.h>
#include <tabos/internal/filesystem.h>
#include <tabos/platform/platform.h>
#include <tabos/platform/storage.h>
#include <tabos/platform/storage_backend.h>

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char root[] = "/tmp/tabos-storage-power-XXXXXX";
static atomic_bool hold_io;
static atomic_bool io_entered;
static atomic_bool hold_sync;
static atomic_bool sync_entered;
static int backend_error;
static unsigned int sync_calls;
static tabos_fd_t retained;
static bool write_operation;
static tabos_ssize_t io_result;
static int metadata_result;
static atomic_bool shutdown_finished;

int power_storage_read(platform_file_t file, void* buffer, size_t count, size_t* bytes_read);
int power_storage_write(platform_file_t file, const void* buffer, size_t count, size_t* bytes_written);

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "Filesystem power: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void delay_io(void)
{
    if (atomic_load(&hold_io)) {
        atomic_store(&io_entered, true);
        while (atomic_load(&hold_io)) {
            SDL_Delay(1U);
        }
    }
}

int platform_storage_read(platform_file_t file, void* buffer, size_t count, size_t* bytes_read)
{
    delay_io();
    return power_storage_read(file, buffer, count, bytes_read);
}

int platform_storage_write(platform_file_t file, const void* buffer, size_t count, size_t* bytes_written)
{
    delay_io();
    return power_storage_write(file, buffer, count, bytes_written);
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* path, size_t size, bool* removable, const char** name)
{
    if (index != 0U || strlen(root) >= size) {
        return false;
    }
    strcpy(path, root);
    *letter    = 'T';
    *removable = true;
    *name      = "Power test";
    return true;
}

void storage_backend_unmount(char letter)
{
    check(letter == 'T' && !atomic_load(&hold_io) && !atomic_load(&hold_sync), "no unmount during work");
}

bool storage_backend_info(char letter, uint64_t* total, uint64_t* free_bytes)
{
    (void) letter;
    *total      = 1024U;
    *free_bytes = 512U;
    return true;
}

int storage_backend_sync(char letter)
{
    check(letter == 'T', "retained mount");
    ++sync_calls;
    atomic_store(&sync_entered, true);
    while (atomic_load(&hold_sync)) {
        SDL_Delay(1U);
    }
    /* A deterministic namespace-barrier model, not proof of host durability.
     * The production POSIX layer has already fsynced retained writable files. */
    return backend_error;
}

static void await_flag(atomic_bool* flag)
{
    const uint64_t limit = SDL_GetTicks() + 2000U;
    while (!atomic_load(flag) && SDL_GetTicks() < limit) {
        SDL_Delay(1U);
    }
    check(atomic_load(flag), "worker reached test barrier");
}

static void finish_sync(uint64_t now)
{
    const uint64_t limit = SDL_GetTicks() + 2000U;
    do {
        filesystem_power_update(now);
        const filesystem_power_state_t state = filesystem_power_status().state;
        if (state == FILESYSTEM_POWER_READY || state == FILESYSTEM_POWER_FAILED) {
            return;
        }
        SDL_Delay(1U);
    } while (SDL_GetTicks() < limit);
    check(false, "sync finished");
}

static void* run_io(void* unused)
{
    (void) unused;
    char byte;
    if (write_operation) {
        io_result = tabos_fs_write(retained, "x", 1U);
    } else {
        io_result = tabos_fs_read(retained, &byte, 1U);
    }
    return NULL;
}

static void* run_metadata(void* unused)
{
    (void) unused;
    tabos_stat_t status;
    metadata_result = tabos_fs_stat("T:/data", &status);
    return NULL;
}

static void* run_shutdown(void* unused)
{
    (void) unused;
    filesystem_shutdown();
    atomic_store(&shutdown_finished, true);
    return NULL;
}

int main(void)
{
    check(mkdtemp(root) != NULL && SDL_Init(SDL_INIT_EVENTS), "test environment");
    check(filesystem_init(), "filesystem init");
    retained = tabos_fs_open("T:/data", TABOS_O_CREAT | TABOS_O_RDWR, 0600U);
    check(retained > 0 && tabos_fs_write(retained, "abc", 3U) == 3, "open writable file");
    check(tabos_fs_seek(retained, 1, TABOS_SEEK_SET) == 1, "retained offset");
    tabos_stat_t before;
    check(tabos_fs_fstat(retained, &before) == 0, "identity before freeze");
    const tabos_dir_t directory = tabos_fs_opendir("T:/");
    tabos_dirent_t entry;
    check(directory > 0 && tabos_fs_readdir(directory, &entry) == 1, "retained directory cursor");
    for (unsigned int cycle = 0U; cycle < 3U; ++cycle) {
        check(filesystem_power_begin(100U) && !filesystem_power_begin(100U), "one transition");
        check(tabos_fs_close(retained) == -1 && *tabos_errno_location() == TABOS_EBUSY, "close frozen");
        check(tabos_fs_open("T:/new", TABOS_O_CREAT | TABOS_O_WRONLY, 0600U) == -1, "create frozen");
        check(tabos_fs_rename("T:/data", "T:/renamed") == -1, "rename frozen");
        check(tabos_fs_closedir(directory) == -1, "directory close frozen");
        finish_sync(100U);
        check(filesystem_power_status().state == FILESYSTEM_POWER_READY, "synced without closing handles");
        filesystem_power_abort();
    }
    tabos_stat_t after;
    check(tabos_fs_fstat(retained, &after) == 0 && before.device_id == after.device_id &&
              before.file_id == after.file_id,
          "file identity retained");
    check(tabos_fs_seek(retained, 0, TABOS_SEEK_CUR) == 1, "offset retained");
    check(tabos_fs_readdir(directory, &entry) == 0, "directory not rewound");
    char cwd[TABOS_FS_PATH_MAX];
    check(tabos_fs_getcwd(cwd, sizeof(cwd)) != NULL && strcmp(cwd, "T:/") == 0, "working directory retained");

    for (unsigned int mutation = 0U; mutation < 2U; ++mutation) {
        atomic_store(&hold_io, true);
        atomic_store(&io_entered, false);
        write_operation = mutation != 0U;
        pthread_t thread;
        check(pthread_create(&thread, NULL, run_io, NULL) == 0, "start admitted I/O");
        await_flag(&io_entered);
        pthread_t metadata;
        check(pthread_create(&metadata, NULL, run_metadata, NULL) == 0, "queue metadata behind disk I/O");
        const uint64_t limit = SDL_GetTicks() + 2000U;
        while (filesystem_power_status().operations != 2U && SDL_GetTicks() < limit) {
            SDL_Delay(1U);
        }
        const unsigned int before_sync = sync_calls;
        check(filesystem_power_begin(1000U), "freeze during I/O");
        filesystem_power_update(2999U);
        const filesystem_power_status_t status = filesystem_power_status();
        check(status.state == FILESYSTEM_POWER_DRAINING && status.operations == 2U && status.mutations == mutation,
              "read and mutation accounted during drain");
        check(sync_calls == before_sync, "no sync races active I/O");
        filesystem_power_update(3000U);
        check(filesystem_power_status().state == FILESYSTEM_POWER_FAILED &&
                  filesystem_power_status().error == TABOS_ETIMEDOUT,
              "exact drain timeout");
        atomic_store(&hold_io, false);
        check(pthread_join(thread, NULL) == 0 && io_result == 1, "timed-out drain leaves operation intact");
        check(pthread_join(metadata, NULL) == 0 && metadata_result == 0, "queued metadata completes normally");
        check(filesystem_power_status().operations == 0U && filesystem_power_status().mutations == 0U,
              "all counters released");
    }

    backend_error = TABOS_EIO;
    check(filesystem_power_begin(4000U), "retry after drain timeout");
    finish_sync(4000U);
    check(filesystem_power_status().error == TABOS_EIO && tabos_fs_fstat(retained, &after) == 0,
          "sync failure restores admission");
    backend_error = TABOS_ENOTSUP;
    check(filesystem_power_begin(5000U), "unsupported backend");
    finish_sync(5000U);
    check(filesystem_power_status().error == TABOS_ENOTSUP, "unsupported is not successful quiescence");
    backend_error = 0;
    atomic_store(&hold_sync, true);
    atomic_store(&sync_entered, false);
    check(filesystem_power_begin(6000U), "slow sync start");
    filesystem_power_update(6000U);
    await_flag(&sync_entered);
    filesystem_power_update(8000U);
    check(filesystem_power_status().state == FILESYSTEM_POWER_ABORTING, "timeout retains worker ownership");
    check(tabos_fs_close(retained) == -1 && !filesystem_power_begin(8000U), "cannot close borrowed handle or overlap");
    atomic_store(&hold_sync, false);
    finish_sync(8000U);
    check(filesystem_power_status().error == TABOS_ETIMEDOUT, "late success cannot erase timeout");
    check(tabos_fs_fstat(retained, &after) == 0, "late worker released admission");
    atomic_store(&hold_sync, true);
    atomic_store(&sync_entered, false);
    const uint64_t late_start = platform_time_ms();
    check(filesystem_power_begin(late_start), "late callback before runtime deadline dispatch");
    filesystem_power_update(late_start);
    await_flag(&sync_entered);
    SDL_Delay(2010U);
    atomic_store(&hold_sync, false);
    /* Deliberately pass the old dispatch time. Completion's own monotonic time
     * must reject a late success even if the deadline event has not run yet. */
    finish_sync(late_start);
    check(filesystem_power_status().error == TABOS_ETIMEDOUT, "late completion cannot bypass deadline");
    check(filesystem_power_begin(9000U), "fresh barrier after worker completion");
    finish_sync(9000U);
    check(filesystem_power_status().state == FILESYSTEM_POWER_READY, "recovery");
    filesystem_power_abort();
    check(tabos_fs_closedir(directory) == 0 && tabos_fs_close(retained) == 0, "original handles close");
    check(tabos_fs_unlink("T:/data") == 0, "cleanup data");
    atomic_store(&hold_sync, true);
    atomic_store(&sync_entered, false);
    check(filesystem_power_begin(10000U), "shutdown with pending sync");
    filesystem_power_update(10000U);
    await_flag(&sync_entered);
    pthread_t shutdown_thread;
    check(pthread_create(&shutdown_thread, NULL, run_shutdown, NULL) == 0, "begin safe shutdown");
    SDL_Delay(10U);
    check(!atomic_load(&shutdown_finished), "shutdown retains worker resources");
    atomic_store(&hold_sync, false);
    check(pthread_join(shutdown_thread, NULL) == 0 && atomic_load(&shutdown_finished), "shutdown joins callback");
    SDL_Quit();
    check(rmdir(root) == 0, "cleanup root");
    return EXIT_SUCCESS;
}
