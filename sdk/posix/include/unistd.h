#ifndef TABOS_POSIX_UNISTD_H
#define TABOS_POSIX_UNISTD_H

#include <tabos/posix_compat.h>

typedef tabos_ssize_t ssize_t;
typedef tabos_off_t off_t;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET TABOS_SEEK_SET
#define SEEK_CUR TABOS_SEEK_CUR
#define SEEK_END TABOS_SEEK_END

#define close  tabos_posix_close
#define read   tabos_posix_read
#define write  tabos_posix_write
#define lseek  tabos_posix_lseek
#define unlink tabos_posix_unlink
#define rename tabos_posix_rename
#define rmdir  tabos_posix_rmdir
#define chdir  tabos_posix_chdir
#define getcwd tabos_posix_getcwd

#endif
