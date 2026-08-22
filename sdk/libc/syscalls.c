#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *strerror(int error)
{
    switch (error) {
        case EPERM: return "Operation not permitted";
        case ENOENT: return "No such file or directory";
        case EIO: return "I/O error";
        case EBADF: return "Bad file descriptor";
        case EAGAIN: return "Resource temporarily unavailable";
        case ENOMEM: return "Out of memory";
        case EACCES: return "Permission denied";
        case EEXIST: return "File exists";
        case ENODEV: return "No such device";
        case ENOTDIR: return "Not a directory";
        case EISDIR: return "Is a directory";
        case EINVAL: return "Invalid argument";
        case ENFILE: return "Too many open files";
        case EMFILE: return "Too many open files";
        case ENOSPC: return "No space left on device";
        case EROFS: return "Read-only filesystem";
        case ENAMETOOLONG: return "File name too long";
        case TABOS_ENOTEMPTY: return "Directory not empty";
        case ENOTSUP: return "Operation not supported";
        default: return "Unknown error";
    }
}

extern const tabos_elf_api_t *tabos_runtime_api;

enum {
    TABOS_RUNTIME_O_RDONLY = 0x0000,
    TABOS_RUNTIME_O_WRONLY = 0x0001,
    TABOS_RUNTIME_O_RDWR = 0x0002,
    TABOS_RUNTIME_O_CREAT = 0x0010,
    TABOS_RUNTIME_O_EXCL = 0x0020,
    TABOS_RUNTIME_O_TRUNC = 0x0040,
    TABOS_RUNTIME_O_APPEND = 0x0080,
    TABOS_RUNTIME_O_NONBLOCK = 0x0100,
};

static int fail_result(int result)
{
    if (result >= 0) return result;
    errno = -result;
    return -1;
}

static int runtime_flags(int flags)
{
    int result = flags & O_ACCMODE;
    if ((flags & O_CREAT) != 0) result |= TABOS_RUNTIME_O_CREAT;
    if ((flags & O_EXCL) != 0) result |= TABOS_RUNTIME_O_EXCL;
    if ((flags & O_TRUNC) != 0) result |= TABOS_RUNTIME_O_TRUNC;
    if ((flags & O_APPEND) != 0) result |= TABOS_RUNTIME_O_APPEND;
#ifdef O_NONBLOCK
    if ((flags & O_NONBLOCK) != 0) result |= TABOS_RUNTIME_O_NONBLOCK;
#endif
    return result;
}

int _open(const char *path, int flags, int mode)
{
    return fail_result(tabos_runtime_api->fd_open(path, runtime_flags(flags), (uint32_t)mode));
}

int _close(int descriptor)
{
    return fail_result(tabos_runtime_api->fd_close(descriptor));
}

ssize_t _read(int descriptor, void *buffer, size_t count)
{
    for (;;) {
        const int result = tabos_runtime_api->fd_read(descriptor, buffer, (uint32_t)count);
        if (result != -EAGAIN) return (ssize_t)fail_result(result);
        const int flags = tabos_runtime_api->fd_get_flags(descriptor);
        if (flags < 0 || (flags & TABOS_RUNTIME_O_NONBLOCK) != 0) {
            errno = flags < 0 ? -flags : EAGAIN;
            return -1;
        }
        tabos_runtime_api->yield();
    }
}

int sched_yield(void)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->yield == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_runtime_api->yield();
    return 0;
}

ssize_t _write(int descriptor, const void *buffer, size_t count)
{
    return (ssize_t)fail_result(
        tabos_runtime_api->fd_write(descriptor, buffer, (uint32_t)count));
}

off_t _lseek(int descriptor, off_t offset, int whence)
{
    if (offset > INT32_MAX || offset < INT32_MIN) {
        errno = EOVERFLOW;
        return (off_t)-1;
    }
    int32_t position = 0;
    const int result = tabos_runtime_api->fd_seek(
        descriptor, (int32_t)offset, whence, &position);
    if (result < 0) {
        errno = -result;
        return (off_t)-1;
    }
    return (off_t)position;
}

static void copy_stat(struct stat *destination, const tabos_elf_stat_t *source)
{
    *destination = (struct stat){0};
    if ((source->mode & 0x0001U) != 0U) destination->st_mode = S_IFREG;
    if ((source->mode & 0x0002U) != 0U) destination->st_mode = S_IFDIR;
    destination->st_size = (off_t)((uint64_t)source->size_low |
        (uint64_t)source->size_high << 32U);
    destination->st_mtime = (time_t)((uint64_t)(uint32_t)source->modified_time_low |
        (uint64_t)(uint32_t)source->modified_time_high << 32U);
}

int _stat(const char *path, struct stat *status)
{
    tabos_elf_stat_t runtime_status;
    const int result = tabos_runtime_api->fs_stat(path, &runtime_status);
    if (result < 0) return fail_result(result);
    copy_stat(status, &runtime_status);
    return 0;
}

int _fstat(int descriptor, struct stat *status)
{
    tabos_elf_stat_t runtime_status;
    const int result = tabos_runtime_api->fd_stat(descriptor, &runtime_status);
    if (result < 0) return fail_result(result);
    copy_stat(status, &runtime_status);
    if (descriptor >= 0 && descriptor <= 2) status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int descriptor)
{
    if (descriptor >= 0 && descriptor <= 2) return 1;
    errno = ENOTTY;
    return 0;
}

int _mkdir(const char *path, int mode)
{
    return fail_result(tabos_runtime_api->fs_mkdir(path, (uint32_t)mode));
}

int mkdir(const char *path, mode_t mode)
{
    return _mkdir(path, (int)mode);
}

int _unlink(const char *path)
{
    return fail_result(tabos_runtime_api->fs_unlink(path));
}

int _rmdir(const char *path)
{
    return fail_result(tabos_runtime_api->fs_rmdir(path));
}

int rmdir(const char *path)
{
    return _rmdir(path);
}

int _rename(const char *old_path, const char *new_path)
{
    return fail_result(tabos_runtime_api->fs_rename(old_path, new_path));
}

int _chdir(const char *path)
{
    return fail_result(tabos_runtime_api->fs_chdir(path));
}

int chdir(const char *path)
{
    return _chdir(path);
}

char *_getcwd(char *buffer, size_t size)
{
    const int result = tabos_runtime_api->fs_getcwd(buffer, (uint32_t)size);
    if (result < 0) {
        errno = -result;
        return NULL;
    }
    return buffer;
}

char *getcwd(char *buffer, size_t size)
{
    return _getcwd(buffer, size);
}

void *_sbrk(ptrdiff_t increment)
{
    if (increment > INT32_MAX || increment < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }
    void *result = tabos_runtime_api->heap_sbrk((int32_t)increment);
    if (result == (void *)-1) errno = ENOMEM;
    return result;
}

int _getpid(void)
{
    return 1;
}

int _kill(int process, int signal)
{
    (void)process;
    (void)signal;
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    tabos_runtime_api->request_exit(status);
    for (;;) tabos_runtime_api->yield();
}

int fcntl(int descriptor, int command, ...)
{
    if (command == F_GETFL) {
        const int result = tabos_runtime_api->fd_get_flags(descriptor);
        if (result < 0) return fail_result(result);
        int flags = result & 3;
#ifdef O_NONBLOCK
        if ((result & TABOS_RUNTIME_O_NONBLOCK) != 0) flags |= O_NONBLOCK;
#endif
        return flags;
    }
    if (command == F_SETFL) {
        va_list arguments;
        va_start(arguments, command);
        const int flags = va_arg(arguments, int);
        va_end(arguments);
        return fail_result(tabos_runtime_api->fd_set_flags(descriptor, runtime_flags(flags)));
    }
    errno = EINVAL;
    return -1;
}
