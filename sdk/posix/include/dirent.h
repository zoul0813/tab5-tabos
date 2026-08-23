#ifndef TABOS_POSIX_DIRENT_H
#define TABOS_POSIX_DIRENT_H

#include <tabos/posix_compat.h>

enum {
    DT_UNKNOWN = 0,
    DT_REG     = 1,
    DT_DIR     = 2,
};

struct dirent {
        uint8_t d_type;
        char d_name[TABOS_FS_NAME_MAX + 1];
};

typedef tabos_posix_dir_t DIR;

#define opendir            tabos_posix_opendir
#define readdir(directory) ((struct dirent*) tabos_posix_readdir(directory))
#define closedir           tabos_posix_closedir

#endif
