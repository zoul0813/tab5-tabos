#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        const char* contents;
        size_t offset;
        size_t fail_after;
        int close_error;
        bool read_error;
        bool error_checked;
} MockFile;

static MockFile mock_file;

static MockFile* mock_fopen(const char* path, const char* mode)
{
    if (strcmp(mode, "rb") != 0) {
        errno = EINVAL;
        return NULL;
    }
    if (strcmp(path, "missing") == 0) {
        errno = ENOENT;
        return NULL;
    }

    mock_file = (MockFile) {
        .contents   = "one two\nthree\n",
        .fail_after = SIZE_MAX,
    };
    if (strcmp(path, "read-error") == 0) {
        mock_file.fail_after = 8U;
    } else if (strcmp(path, "close-error") == 0) {
        mock_file.close_error = ENOSPC;
    }
    return &mock_file;
}

static int mock_fgetc(MockFile* file)
{
    if (file->offset == file->fail_after) {
        file->read_error = true;
        errno            = EIO;
        return EOF;
    }
    const unsigned char value = (unsigned char) file->contents[file->offset];
    if (value == '\0') {
        return EOF;
    }
    ++file->offset;
    return value;
}

static int mock_ferror(MockFile* file)
{
    file->error_checked = true;
    return file->read_error;
}

static int mock_fclose(MockFile* file)
{
    if (file->close_error != 0) {
        errno = file->close_error;
        return EOF;
    }
    return 0;
}

#define FILE   MockFile
#define fclose mock_fclose
#define ferror mock_ferror
#define fgetc  mock_fgetc
#define fopen  mock_fopen
#define main   tabos_wc_main
#include "apps/coreutils/src/wc/main.c"
#undef FILE
#undef fclose
#undef ferror
#undef fgetc
#undef fopen
#undef main

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "coreutils wc test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    char* missing_arguments[] = {"wc", "missing", "good"};
    check(tabos_wc_main(3, missing_arguments) != 0, "open failure affects exit status");

    char* read_arguments[] = {"wc", "read-error"};
    check(tabos_wc_main(2, read_arguments) != 0, "read failure affects exit status");
    check(mock_file.error_checked, "read failure checks stream error");

    char* close_arguments[] = {"wc", "close-error"};
    check(tabos_wc_main(2, close_arguments) != 0, "close failure affects exit status");

    char* good_arguments[] = {"wc", "good"};
    check(tabos_wc_main(2, good_arguments) == 0, "successful input returns success");
    return EXIT_SUCCESS;
}
