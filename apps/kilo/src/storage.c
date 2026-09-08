#include <kilo/storage.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    KILO_PATH_MAX      = 512,
    KILO_TEMP_ATTEMPTS = 64
};
static int storage_open(const char* path, int flags, int mode)
{
    return open(path, flags, mode);
}
const kilo_storage_t kilo_storage_default = {storage_open, read, write, close, stat, rename, unlink};
static void error_message(kilo_editor_t* e, const char* action, int error)
{
    (void) snprintf(e->message, sizeof(e->message), "%s: %s", action, strerror(error));
}
bool kilo_load(kilo_editor_t* e, const kilo_storage_t* io)
{
    kilo_editor_t loaded;
    if (!kilo_init(&loaded, e->filename)) {
        kilo_status(e, "Out of memory");
        return false;
    }
    loaded.allocate = e->allocate;
    bool ok         = false;
    char* line      = loaded.allocate(KILO_LINE_MAX + 2U);
    if (line == NULL) {
        kilo_status(e, "Out of memory");
        kilo_dispose(&loaded);
        return false;
    }
    struct stat info;
    const int metadata = io->stat(e->filename, &info);
    if (metadata == 0 && S_ISDIR(info.st_mode)) {
        errno = EISDIR;
        goto failed;
    }
    if (metadata < 0 && errno != ENOENT) {
        goto failed;
    }
    const int fd = io->open(e->filename, O_RDONLY, 0);
    if (fd < 0) {
        if (errno == ENOENT) {
            ok = kilo_append_row(&loaded, "", 0U, 0U);
            goto done;
        }
        goto failed;
    }
    size_t length = 0U, total = 0U;
    bool style_seen = false;
    ok              = true;
    char block[512];
    while (ok) {
        const ssize_t count = io->read(fd, block, sizeof(block));
        if (count < 0) {
            error_message(e, "Read failed", errno);
            ok = false;
            break;
        }
        if (count == 0) {
            break;
        }
        if ((size_t) count > sizeof(block)) {
            kilo_status(e, "Invalid read result");
            ok = false;
            break;
        }
        for (size_t i = 0U; i < (size_t) count; ++i) {
            const unsigned char c = (unsigned char) block[i];
            if (++total > KILO_BYTES_MAX || c == 0U) {
                kilo_status(e, c == 0U ? "Binary file contains NUL; not opened" : "File exceeds 256 KiB");
                ok = false;
                break;
            }
            if (c == '\n') {
                uint8_t ending = 1U;
                if (length > 0U && line[length - 1U] == '\r') {
                    --length;
                    ending = 2U;
                }
                if (!style_seen) {
                    loaded.newline = ending;
                    style_seen     = true;
                }
                if (!kilo_append_row(&loaded, line, length, ending)) {
                    kilo_status(e, loaded.message);
                    ok = false;
                    break;
                }
                length = 0U;
            } else {
                if (length == KILO_LINE_MAX + 1U || (length == KILO_LINE_MAX && c != '\r')) {
                    kilo_status(e, "Line exceeds 16 KiB");
                    ok = false;
                    break;
                }
                line[length++] = (char) c;
            }
        }
    }
    if (io->close(fd) != 0) {
        error_message(e, "Close failed", errno);
        ok = false;
    }
    if (ok && !kilo_append_row(&loaded, line, length, 0U)) {
        kilo_status(e, loaded.message);
        ok = false;
    }
    goto done;
failed:
    error_message(e, "Open failed", errno);
done:
    free(line);
    if (ok) {
        kilo_dispose(e);
        *e = loaded;
        kilo_highlight(e, 0U);
    } else {
        kilo_dispose(&loaded);
    }
    return ok;
}
static bool same_basename(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        unsigned char a = (unsigned char) *left++;
        unsigned char b = (unsigned char) *right++;
        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char) (a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char) (b + ('a' - 'A'));
        }
        if (a != b) {
            return false;
        }
    }
    return *left == *right;
}

static int reserve(kilo_editor_t* e, const kilo_storage_t* io, char path[KILO_PATH_MAX], const char* suffix)
{
    const char* slash   = strrchr(e->filename, '/');
    const size_t parent = slash == NULL ? 0U : (size_t) (slash - e->filename) + 1U;
    if (parent > KILO_PATH_MAX - 40U) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (unsigned int attempt = 0U; attempt < KILO_TEMP_ATTEMPTS; ++attempt) {
        memcpy(path, e->filename, parent);
        (void) snprintf(path + parent, KILO_PATH_MAX - parent, ".kilo-%u.%s", attempt, suffix);
        const char* basename = slash == NULL ? e->filename : slash + 1U;
        if (same_basename(basename, path + parent)) {
            continue;
        }
        const int fd = io->open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || errno != EEXIST) {
            return fd;
        }
    }
    errno = EEXIST;
    return -1;
}
static bool write_all(const kilo_storage_t* io, int fd, const char* data, size_t size)
{
    while (size != 0U) {
        const ssize_t count = io->write(fd, data, size);
        if (count <= 0 || (size_t) count > size) {
            if (count >= 0) {
                errno = EIO;
            }
            return false;
        }
        data += count;
        size -= (size_t) count;
    }
    return true;
}
bool kilo_save(kilo_editor_t* e, const kilo_storage_t* io)
{
    char staged[KILO_PATH_MAX], backup[KILO_PATH_MAX];
    const int fd = reserve(e, io, staged, "tmp");
    if (fd < 0) {
        error_message(e, "Cannot stage save", errno);
        return false;
    }
    bool ok   = true;
    int error = 0;
    for (size_t i = 0U; i < e->numrows && ok; ++i) {
        const kilo_row_t* row = &e->row[i];
        ok                    = write_all(io, fd, row->chars, row->size);
        if (ok && row->ending != 0U) {
            ok = write_all(io, fd, row->ending == 2U ? "\r\n" : "\n", row->ending);
        }
        if (!ok) {
            error = errno;
        }
    }
    if (io->close(fd) != 0 && ok) {
        ok    = false;
        error = errno;
    }
    if (!ok) {
        (void) io->unlink(staged);
        error_message(e, "Save staging failed; original unchanged", error);
        return false;
    }
    struct stat info;
    const int metadata  = io->stat(e->filename, &info);
    const bool original = metadata == 0;
    if ((metadata < 0 && errno != ENOENT) || (original && S_ISDIR(info.st_mode))) {
        error = original ? EISDIR : errno;
        (void) io->unlink(staged);
        error_message(e, "Cannot replace destination", error);
        return false;
    }
    if (original) {
        const int backup_fd = reserve(e, io, backup, "bak");
        if (backup_fd < 0) {
            error = errno;
            (void) io->unlink(staged);
            error_message(e, "Cannot reserve backup", error);
            return false;
        }
        const int closed = io->close(backup_fd);
        if (closed != 0 || io->unlink(backup) != 0) {
            error = errno;
            (void) io->unlink(staged);
            error_message(e, "Cannot prepare backup", error);
            return false;
        }
        // Both host rename and FatFs f_rename accept an absent destination. No
        // other TabOS application runs concurrently; external writers are unsupported.
        if (io->rename(e->filename, backup) != 0) {
            error = errno;
            (void) io->unlink(staged);
            error_message(e, "Cannot move original to backup", error);
            return false;
        }
    }
    if (io->rename(staged, e->filename) != 0) {
        error = errno;
        if (original && io->rename(backup, e->filename) != 0) {
            (void) snprintf(e->message, sizeof(e->message), "SAVE FAILED: original at %s; edited copy at %s", backup,
                            staged);
            (void) snprintf(e->recovery, sizeof(e->recovery), "%s", e->message);
            return false;
        }
        // Retain the complete edited copy for recovery even when rollback succeeded.
        (void) snprintf(e->message, sizeof(e->message), "Save failed (%s); edited copy at %s", strerror(error), staged);
        (void) snprintf(e->recovery, sizeof(e->recovery), "%s", e->message);
        return false;
    }
    e->dirty = false;
    if (original && io->unlink(backup) != 0) {
        (void) snprintf(e->message, sizeof(e->message), "Saved; remove leftover backup: %s", backup);
        (void) snprintf(e->recovery, sizeof(e->recovery), "%s", e->message);
    } else {
        kilo_status(e, "Saved");
    }
    return true;
}
