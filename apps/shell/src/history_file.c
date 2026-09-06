#include <shell/history.h>

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static const char history_directory[] = "T:/user";
static const char history_path[]      = "T:/user/history.txt";
static const char temporary_path[]    = "T:/user/.history.txt.tmp";

int shell_history_load(shell_history_t* history)
{
    *history   = (shell_history_t) {0};
    FILE* file = fopen(history_path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? 0 : -1;
    }
    char line[SHELL_LINE_CAPACITY];
    size_t used          = 0U;
    bool invalid         = false;
    bool carriage_return = false;
    int character;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            if (!invalid) {
                line[used] = '\0';
                (void) shell_history_add(history, line);
            }
            used            = 0U;
            invalid         = false;
            carriage_return = false;
        } else if (character == '\r' && !carriage_return) {
            carriage_return = true;
        } else if (carriage_return || character < 0x20 || character > 0x7e || used == SHELL_LINE_CAPACITY - 1U) {
            invalid = true;
        } else {
            line[used++] = (char) character;
        }
    }
    int error = ferror(file) ? EIO : 0;
    if (error == 0 && !invalid && !carriage_return && used > 0U) {
        line[used] = '\0';
        (void) shell_history_add(history, line);
    }
    if (fclose(file) != 0 && error == 0) {
        error = errno;
    }
    shell_history_reset_navigation(history);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

int shell_history_save(const shell_history_t* history)
{
    if (mkdir(history_directory, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    FILE* file = fopen(temporary_path, "wb");
    if (file == NULL) {
        return -1;
    }
    int error = 0;
    for (size_t index = 0U; index < history->count; ++index) {
        if (fprintf(file, "%s\n", history->entries[index]) < 0) {
            error = errno != 0 ? errno : EIO;
            break;
        }
    }
    if (fclose(file) != 0 && error == 0) {
        error = errno != 0 ? errno : EIO;
    }
    if (error == 0 && rename(temporary_path, history_path) != 0) {
        error = errno;
    }
    if (error != 0) {
        (void) unlink(temporary_path);
        errno = error;
        return -1;
    }
    return 0;
}
