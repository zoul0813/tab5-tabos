#ifndef TABOS_POSIX_FCNTL_H
#define TABOS_POSIX_FCNTL_H

#include <tabos/posix_compat.h>

#define O_RDONLY   TABOS_O_RDONLY
#define O_WRONLY   TABOS_O_WRONLY
#define O_RDWR     TABOS_O_RDWR
#define O_ACCMODE  TABOS_O_ACCMODE
#define O_CREAT    TABOS_O_CREAT
#define O_EXCL     TABOS_O_EXCL
#define O_TRUNC    TABOS_O_TRUNC
#define O_APPEND   TABOS_O_APPEND
#define O_NONBLOCK TABOS_O_NONBLOCK

#define open tabos_posix_open

#endif
