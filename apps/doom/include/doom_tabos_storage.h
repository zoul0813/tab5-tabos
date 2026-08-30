#ifndef DOOM_TABOS_STORAGE_H
#define DOOM_TABOS_STORAGE_H

#include <stdbool.h>

#define DOOM_TABOS_GAME_DIRECTORY "T:/data/doom"

typedef enum {
    DOOM_TABOS_STORAGE_OK,
    DOOM_TABOS_STORAGE_INVALID_ARGUMENTS,
    DOOM_TABOS_STORAGE_DIRECTORY_FAILED,
    DOOM_TABOS_STORAGE_IWAD_NOT_FOUND,
    DOOM_TABOS_STORAGE_NO_MEMORY,
} doom_tabos_storage_status_t;

typedef struct {
        int argc;
        char** argv;
        const char* iwad_path;
        bool owns_argv;
} doom_tabos_launch_t;

doom_tabos_storage_status_t doom_tabos_storage_prepare(int argc, char** argv, doom_tabos_launch_t* launch);
void doom_tabos_storage_release(doom_tabos_launch_t* launch);

#endif
