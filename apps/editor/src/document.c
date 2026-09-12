#include <editor/document.h>
#include <tabos/filesystem.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

bool editor_document_open(editor_document_t* document, const char* path)
{
    if (path == NULL || strlen(path) >= EDITOR_PATH_CAPACITY) {
        return false;
    }
    const tabos_fd_t descriptor = open(path, O_RDONLY, 0U);
    if (descriptor < 0) {
        return false;
    }
    char* replacement = malloc(EDITOR_TEXT_CAPACITY);
    size_t used       = 0U;
    bool valid        = replacement != NULL;
    while (valid && used < EDITOR_TEXT_CAPACITY) {
        const tabos_ssize_t count = read(descriptor, replacement + used, EDITOR_TEXT_CAPACITY - used);
        if (count < 0) {
            valid = false;
            break;
        }
        if (count == 0) {
            break;
        }
        used += (size_t) count;
    }
    if (close(descriptor) != 0 || used >= EDITOR_TEXT_CAPACITY) {
        valid = false;
    }
    if (valid) {
        for (size_t index = 0U; index < used; ++index) {
            const unsigned char value = (unsigned char) replacement[index];
            if (value == 0U || (value < 32U && value != '\n' && value != '\r' && value != '\t')) {
                valid = false;
                break;
            }
        }
    }
    if (valid) {
        replacement[used] = '\0';
        memcpy(document->text, replacement, used + 1U);
        strcpy(document->path, path);
        document->dirty = false;
    }
    free(replacement);
    return valid;
}

bool editor_document_save(editor_document_t* document, const char* path)
{
    strcpy(document->message, "Save failed. Unsaved text retained; check path, space and storage.");
    if (path == NULL || path[0] == '\0' || strlen(path) >= EDITOR_PATH_CAPACITY) {
        return false;
    }
    char temporary[256];
    const int length = snprintf(temporary, sizeof(temporary), "%s.tabos-save-%d", path, (int) getpid());
    if (length < 0 || (size_t) length >= sizeof(temporary)) {
        return false;
    }
    const tabos_fd_t descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return false;
    }
    size_t used        = 0U;
    const size_t bytes = strlen(document->text);
    bool valid         = true;
    while (used < bytes) {
        const tabos_ssize_t count = write(descriptor, document->text + used, bytes - used);
        if (count <= 0) {
            valid = false;
            break;
        }
        used += (size_t) count;
    }
    if (close(descriptor) != 0) {
        valid = false;
    }
    if (!valid) {
        (void) unlink(temporary);
        return false;
    }
    /* FatFs does not replace an existing destination. Keep a recoverable original
     * until the complete edited file has moved into place. */
    struct stat original_info;
    const int original_status = stat(path, &original_info);
    const bool original       = original_status == 0;
    if ((!original && errno != ENOENT) || (original && !S_ISREG(original_info.st_mode))) {
        (void) unlink(temporary);
        return false;
    }
    char backup[256];
    (void) snprintf(backup, sizeof(backup), "%s.tabos-backup-%d", path, (int) getpid());
    if (original) {
        const int reserved = open(backup, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (reserved < 0) {
            (void) unlink(temporary);
            return false;
        }
        const int closed = close(reserved);
        if (closed != 0 || unlink(backup) != 0 || rename(path, backup) != 0) {
            (void) unlink(temporary);
            return false;
        }
    }
    if (rename(temporary, path) != 0) {
        if (original && rename(backup, path) != 0) {
            (void) snprintf(document->message, sizeof(document->message),
                            "Recovery: original has .tabos-backup-%d suffix; edited copy has .tabos-save-%d suffix.",
                            (int) getpid(), (int) getpid());
        } else {
            (void) unlink(temporary);
        }
        return false;
    }
    strcpy(document->message, "Saved");
    if (original && unlink(backup) != 0) {
        (void) snprintf(document->message, sizeof(document->message),
                        "Saved. Old copy remains with .tabos-backup-%d suffix.", (int) getpid());
    }
    strcpy(document->path, path);
    document->dirty = false;
    return true;
}
