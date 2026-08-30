#include "doom_tabos_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static const char* const iwad_names[] = {
    "doom1.wad", "doom.wad", "doom2.wad", "freedoom1.wad", "freedoom2.wad",
};

static bool option_equals(const char* value, const char* expected)
{
    if (value == NULL || expected == NULL) {
        return false;
    }

    while (*value != '\0' && *expected != '\0') {
        char left  = *value++;
        char right = *expected++;
        if (left >= 'A' && left <= 'Z') {
            left = (char) (left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char) (right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return *value == '\0' && *expected == '\0';
}

static bool ensure_directory(const char* path)
{
    if (mkdir(path, 0755U) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static bool usable_iwad(const char* path)
{
    struct stat status;
    if (stat(path, &status) != 0 || !S_ISREG(status.st_mode)) {
        return false;
    }

    const int file = open(path, O_RDONLY);
    if (file < 0) {
        return false;
    }
    return close(file) == 0;
}

static const char* explicit_iwad(int argc, char** argv, bool* present)
{
    *present = false;
    for (int index = 1; index < argc; ++index) {
        if (!option_equals(argv[index], "-iwad")) {
            continue;
        }
        *present = true;
        return index + 1 < argc ? argv[index + 1] : NULL;
    }
    return NULL;
}

static const char* default_iwad(void)
{
    for (size_t index = 0U; index < sizeof(iwad_names) / sizeof(iwad_names[0]); ++index) {
        if (usable_iwad(iwad_names[index])) {
            return iwad_names[index];
        }
    }
    return NULL;
}

doom_tabos_storage_status_t doom_tabos_storage_prepare(int argc, char** argv, doom_tabos_launch_t* launch)
{
    if (argc < 1 || argv == NULL || launch == NULL) {
        return DOOM_TABOS_STORAGE_INVALID_ARGUMENTS;
    }

    *launch = (doom_tabos_launch_t) {
        .argc = argc,
        .argv = argv,
    };

    if (!ensure_directory("T:/data") || !ensure_directory(DOOM_TABOS_GAME_DIRECTORY) ||
        chdir(DOOM_TABOS_GAME_DIRECTORY) != 0) {
        return DOOM_TABOS_STORAGE_DIRECTORY_FAILED;
    }

    bool has_explicit_iwad;
    const char* iwad = explicit_iwad(argc, argv, &has_explicit_iwad);
    if (has_explicit_iwad) {
        if (iwad == NULL || !usable_iwad(iwad)) {
            return DOOM_TABOS_STORAGE_IWAD_NOT_FOUND;
        }
        launch->iwad_path = iwad;
        return DOOM_TABOS_STORAGE_OK;
    }

    iwad = default_iwad();
    if (iwad == NULL) {
        return DOOM_TABOS_STORAGE_IWAD_NOT_FOUND;
    }

    char** launch_argv = calloc((size_t) argc + 3U, sizeof(*launch_argv));
    if (launch_argv == NULL) {
        return DOOM_TABOS_STORAGE_NO_MEMORY;
    }
    for (int index = 0; index < argc; ++index) {
        launch_argv[index] = argv[index];
    }
    launch_argv[argc]     = "-iwad";
    launch_argv[argc + 1] = (char*) iwad;

    launch->argc      = argc + 2;
    launch->argv      = launch_argv;
    launch->iwad_path = iwad;
    launch->owns_argv = true;
    return DOOM_TABOS_STORAGE_OK;
}

void doom_tabos_storage_release(doom_tabos_launch_t* launch)
{
    if (launch == NULL) {
        return;
    }
    if (launch->owns_argv) {
        free(launch->argv);
    }
    *launch = (doom_tabos_launch_t) {0};
}
