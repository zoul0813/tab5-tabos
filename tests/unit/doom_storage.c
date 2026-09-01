#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "doom_tabos_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    MAX_CALLS = 16,
};

static const char* available_iwads[MAX_CALLS];
static size_t available_iwad_count;
static const char* mkdir_paths[MAX_CALLS];
static size_t mkdir_count;
static const char* stat_paths[MAX_CALLS];
static size_t stat_count;
static const char* chdir_path;
static int mkdir_failure_call;
static int mkdir_failure_errno;
static bool chdir_failure;
static bool open_failure;

static bool is_available(const char* path)
{
    for (size_t index = 0U; index < available_iwad_count; ++index) {
        if (strcmp(path, available_iwads[index]) == 0) {
            return true;
        }
    }
    return false;
}

int mkdir(const char* path, mode_t mode)
{
    (void) mode;
    mkdir_paths[mkdir_count++] = path;
    if (mkdir_failure_call == (int) mkdir_count) {
        errno = mkdir_failure_errno;
        return -1;
    }
    return 0;
}

int chdir(const char* path)
{
    chdir_path = path;
    return chdir_failure ? -1 : 0;
}

int stat(const char* path, struct stat* status)
{
    stat_paths[stat_count++] = path;
    if (!is_available(path)) {
        errno = ENOENT;
        return -1;
    }
    *status = (struct stat) {
        .st_mode = S_IFREG,
        .st_size = 1,
    };
    return 0;
}

int open(const char* path, int flags, ...)
{
    (void) flags;
    return is_available(path) && !open_failure ? 7 : -1;
}

int close(int descriptor)
{
    return descriptor == 7 ? 0 : -1;
}

static void reset_fakes(void)
{
    errno                = 0;
    available_iwad_count = 0U;
    mkdir_count          = 0U;
    stat_count           = 0U;
    chdir_path           = NULL;
    mkdir_failure_call   = 0;
    mkdir_failure_errno  = 0;
    chdir_failure        = false;
    open_failure         = false;
}

static int test_default_search(void)
{
    reset_fakes();
    available_iwads[0]   = "doom2.wad";
    available_iwads[1]   = "freedoom1.wad";
    available_iwad_count = 2U;
    char* argv[]         = {"doom", "-skill", "3", NULL};
    doom_tabos_launch_t launch;

    if (doom_tabos_storage_prepare(3, argv, &launch) != DOOM_TABOS_STORAGE_OK || mkdir_count != 2U ||
        strcmp(mkdir_paths[0], "T:/data") != 0 || strcmp(mkdir_paths[1], DOOM_TABOS_GAME_DIRECTORY) != 0 ||
        chdir_path == NULL || strcmp(chdir_path, DOOM_TABOS_GAME_DIRECTORY) != 0 || stat_count != 3U ||
        strcmp(stat_paths[0], "doom1.wad") != 0 || strcmp(stat_paths[1], "doom.wad") != 0 ||
        strcmp(stat_paths[2], "doom2.wad") != 0 || launch.argc != 5 || !launch.owns_argv || launch.argv[0] != argv[0] ||
        launch.argv[1] != argv[1] || launch.argv[2] != argv[2] || strcmp(launch.argv[3], "-iwad") != 0 ||
        strcmp(launch.argv[4], "doom2.wad") != 0 || launch.argv[5] != NULL ||
        strcmp(launch.iwad_path, "doom2.wad") != 0) {
        doom_tabos_storage_release(&launch);
        return 1;
    }

    doom_tabos_storage_release(&launch);
    return launch.argv == NULL && launch.argc == 0 && !launch.owns_argv ? 0 : 1;
}

static int test_explicit_iwad(void)
{
    reset_fakes();
    mkdir_failure_call   = 1;
    mkdir_failure_errno  = EEXIST;
    const char* path     = "T:/data/doom/custom.wad";
    available_iwads[0]   = path;
    available_iwad_count = 1U;
    char* argv[]         = {"doom", "-IWAD", (char*) path, NULL};
    doom_tabos_launch_t launch;

    if (doom_tabos_storage_prepare(3, argv, &launch) != DOOM_TABOS_STORAGE_OK || launch.argc != 3 ||
        launch.argv != argv || launch.owns_argv || launch.iwad_path != path || stat_count != 1U ||
        strcmp(stat_paths[0], path) != 0) {
        return 1;
    }
    doom_tabos_storage_release(&launch);
    return 0;
}

static int test_failures(void)
{
    doom_tabos_launch_t launch;
    char* argv[] = {"doom", NULL};

    reset_fakes();
    mkdir_failure_call  = 1;
    mkdir_failure_errno = EACCES;
    if (doom_tabos_storage_prepare(1, argv, &launch) != DOOM_TABOS_STORAGE_DIRECTORY_FAILED || mkdir_count != 1U) {
        return 1;
    }

    reset_fakes();
    chdir_failure = true;
    if (doom_tabos_storage_prepare(1, argv, &launch) != DOOM_TABOS_STORAGE_DIRECTORY_FAILED || mkdir_count != 2U) {
        return 1;
    }

    reset_fakes();
    if (doom_tabos_storage_prepare(1, argv, &launch) != DOOM_TABOS_STORAGE_IWAD_NOT_FOUND || stat_count != 5U) {
        return 1;
    }

    reset_fakes();
    char* explicit_argv[] = {"doom", "-iwad", NULL};
    if (doom_tabos_storage_prepare(2, explicit_argv, &launch) != DOOM_TABOS_STORAGE_IWAD_NOT_FOUND ||
        stat_count != 0U) {
        return 1;
    }

    reset_fakes();
    available_iwads[0]   = "doom1.wad";
    available_iwad_count = 1U;
    open_failure         = true;
    if (doom_tabos_storage_prepare(1, argv, &launch) != DOOM_TABOS_STORAGE_IWAD_NOT_FOUND || stat_count != 5U) {
        return 1;
    }

    return doom_tabos_storage_prepare(0, NULL, &launch) == DOOM_TABOS_STORAGE_INVALID_ARGUMENTS ? 0 : 1;
}

int main(void)
{
    return test_default_search() == 0 && test_explicit_iwad() == 0 && test_failures() == 0 ? 0 : 1;
}
