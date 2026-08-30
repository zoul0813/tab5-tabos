#ifndef TABOS_POSIX_ERRNO_H
#define TABOS_POSIX_ERRNO_H

#include <tabos/filesystem.h>

#define errno (*tabos_errno_location())

#define EPERM        TABOS_EPERM
#define ENOENT       TABOS_ENOENT
#define EIO          TABOS_EIO
#define EBADF        TABOS_EBADF
#define EAGAIN       TABOS_EAGAIN
#define ENOMEM       TABOS_ENOMEM
#define EACCES       TABOS_EACCES
#define EEXIST       TABOS_EEXIST
#define ENODEV       TABOS_ENODEV
#define ENOTDIR      TABOS_ENOTDIR
#define EISDIR       TABOS_EISDIR
#define EINVAL       TABOS_EINVAL
#define ENFILE       TABOS_ENFILE
#define EMFILE       TABOS_EMFILE
#define ENOSPC       TABOS_ENOSPC
#define EROFS        TABOS_EROFS
#define ENAMETOOLONG TABOS_ENAMETOOLONG
#define ENOTEMPTY    TABOS_ENOTEMPTY
#define ENOTSUP      TABOS_ENOTSUP
#define ECANCELED    TABOS_ECANCELED

#endif
