#ifndef TABOS_POSIX_SYS_STAT_H
#define TABOS_POSIX_SYS_STAT_H

#include <tabos/posix_compat.h>

typedef uint32_t mode_t;

struct stat {
        uint32_t st_mode;
        uint64_t st_size;
        int64_t st_mtime;
};

#define S_IFREG       TABOS_S_IFREG
#define S_IFDIR       TABOS_S_IFDIR
#define S_ISREG(mode) (((mode) & S_IFREG) != 0U)
#define S_ISDIR(mode) (((mode) & S_IFDIR) != 0U)

#define stat(path, status)        tabos_posix_stat((path), (status))
#define fstat(descriptor, status) tabos_posix_fstat((descriptor), (status))
#define mkdir                     tabos_posix_mkdir

#endif
