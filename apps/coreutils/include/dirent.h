#ifndef TABOS_COREUTILS_DIRENT_H
#define TABOS_COREUTILS_DIRENT_H

#include <tabos/posix_compat.h>

struct dirent {
        uint8_t d_type;
        char d_name[TABOS_FS_NAME_MAX + 1];
};

#define DT_UNKNOWN 0
#define DT_REG     1
#define DT_DIR     2

typedef tabos_posix_dir_t DIR;

#define opendir            tabos_posix_opendir
#define readdir(directory) ((struct dirent*) tabos_posix_readdir(directory))
#define closedir           tabos_posix_closedir

#endif
