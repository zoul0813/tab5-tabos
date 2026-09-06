#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <shell/history.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Only history_file.c is compiled with these substitutions. Failures exercise
// real temporary files while leaving this test's fixture operations unaffected.
static bool fail_write;
static bool fail_close;
static bool fail_rename;

int test_history_fprintf(FILE* stream, const char* format, ...);
int test_history_fclose(FILE* stream);
int test_history_rename(const char* old_path, const char* new_path);

int test_history_fprintf(FILE* stream, const char* format, ...)
{
    if (fail_write) {
        errno = ENOSPC;
        return -1;
    }
    va_list arguments;
    va_start(arguments, format);
    const int result = vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int test_history_fclose(FILE* stream)
{
    const int result = fclose(stream);
    if (fail_close) {
        errno = EIO;
        return EOF;
    }
    return result;
}

int test_history_rename(const char* old_path, const char* new_path)
{
    if (fail_rename) {
        errno = EACCES;
        return -1;
    }
    return rename(old_path, new_path);
}

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "shell history file test failed: %s (errno %d)\n", message, errno);
        exit(EXIT_FAILURE);
    }
}

static void previous_file_preserved(void)
{
    shell_history_t loaded;
    check(shell_history_load(&loaded) == 0, "reload prior file");
    check(loaded.count == 1U && strcmp(loaded.entries[0], "original") == 0, "prior file preserved");
    check(access("T:/user/.history.txt.tmp", F_OK) != 0, "failed temporary removed");
}

int main(void)
{
    char original_directory[4096];
    check(getcwd(original_directory, sizeof(original_directory)) != NULL, "original directory");
    char root[] = "/tmp/tabos-history-XXXXXX";
    check(mkdtemp(root) != NULL && chdir(root) == 0, "temporary root");
    shell_history_t history;
    check(shell_history_load(&history) == 0 && history.count == 0U, "missing file");
    check(shell_history_add(&history, "original"), "seed");
    check(shell_history_save(&history) == -1, "missing drive");
    check(mkdir("T:", 0700) == 0, "drive");
    check(shell_history_save(&history) == 0, "create user directory and save");
    check(shell_history_add(&history, "new"), "changed");
    fail_write = true;
    check(shell_history_save(&history) == -1 && errno == ENOSPC, "write failure");
    fail_write = false;
    previous_file_preserved();
    fail_close = true;
    check(shell_history_save(&history) == -1 && errno == EIO, "close failure");
    fail_close = false;
    previous_file_preserved();
    fail_rename = true;
    check(shell_history_save(&history) == -1 && errno == EACCES, "rename failure");
    fail_rename = false;
    previous_file_preserved();
    check(mkdir("T:/user/.history.txt.tmp", 0700) == 0, "block temporary open");
    check(shell_history_save(&history) == -1, "open failure");
    check(rmdir("T:/user/.history.txt.tmp") == 0, "remove blocker");
    previous_file_preserved();
    check(shell_history_save(&history) == 0, "recover and replace");
    shell_history_t loaded;
    check(shell_history_load(&loaded) == 0 && loaded.count == 2U, "round trip");

    FILE* file = fopen("T:/user/history.txt", "wb");
    check(file != NULL, "fixture");
    check(fputs("\n   \nhello \"two words\"\r\nhello \"two words\"\n", file) >= 0, "CRLF and duplicate");
    for (unsigned int index = 0U; index < 300U; ++index) {
        check(fputc('x', file) != EOF, "oversized");
    }
    check(fputs("\ninvalid\rtext\n", file) >= 0, "invalid CR");
    check(fwrite("a\0b\n", 1U, 4U, file) == 4U, "embedded null");
    check(fputs("last", file) >= 0 && fclose(file) == 0, "unterminated final line");
    check(shell_history_load(&loaded) == 0 && loaded.count == 2U, "skip invalid lines");
    check(strcmp(loaded.entries[0], "hello \"two words\"") == 0 && strcmp(loaded.entries[1], "last") == 0,
          "preserve valid lines");
    file = fopen("T:/user/history.txt", "wb");
    check(file != NULL, "large fixture");
    for (unsigned int index = 0U; index < 40U; ++index) {
        check(fprintf(file, "entry %u\n", index) > 0, "many entries");
    }
    check(fclose(file) == 0 && shell_history_load(&loaded) == 0, "load large file");
    check(loaded.count == 32U && strcmp(loaded.entries[0], "entry 8") == 0 &&
              strcmp(loaded.entries[31], "entry 39") == 0,
          "latest 32");
    check(shell_history_save(&loaded) == 0, "compact");
    file = fopen("T:/user/history.txt", "rb");
    check(file != NULL, "compacted file");
    unsigned int lines = 0U;
    int byte;
    while ((byte = fgetc(file)) != EOF) {
        lines += byte == '\n' ? 1U : 0U;
    }
    check(lines == 32U && fclose(file) == 0, "bounded persisted snapshot");
    check(unlink("T:/user/history.txt") == 0, "remove history");
    check(rmdir("T:/user") == 0 && rmdir("T:") == 0, "remove fixture directories");
    check(chdir(original_directory) == 0 && rmdir(root) == 0, "remove temporary root");
    return EXIT_SUCCESS;
}
