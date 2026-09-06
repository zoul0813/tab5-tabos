#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <shell/history.h>
#include <shell/line.h>
#include <tabos/internal/terminal.h>
#include <tabos/tty.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int shell_test_main(int argc, char** argv);
ssize_t test_shell_read(int descriptor, void* buffer, size_t size);
char* test_shell_getcwd(char* buffer, size_t size);
int test_shell_ioctl(int descriptor, unsigned long request, ...);
int test_shell_spawn(const char* path, int argc, const char* const argv[]);
int test_shell_waitpid(int pid, int* status, int options);

static const char* input;
static unsigned int launches;
static unsigned int scroll_mode_requests;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "shell session test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

ssize_t test_shell_read(int descriptor, void* buffer, size_t size)
{
    check(descriptor == STDIN_FILENO && size > 0U, "stdin request");
    if (*input == '\0') {
        errno = EIO;
        return -1;
    }
    // Deliver one byte at a time to split every escape sequence.
    *(char*) buffer = *input++;
    return 1;
}

char* test_shell_getcwd(char* buffer, size_t size)
{
    check(size >= 4U, "cwd buffer");
    strcpy(buffer, "T:/");
    return buffer;
}

int test_shell_ioctl(int descriptor, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    const uint32_t mode = va_arg(arguments, uint32_t);
    va_end(arguments);
    check(descriptor == STDIN_FILENO && request == TABOS_TTY_SET_MODE && mode == TABOS_TTY_MODE_SCROLL_KEYS,
          "scrollback policy unchanged");
    ++scroll_mode_requests;
    return 0;
}

int test_shell_spawn(const char* path, int argc, const char* const argv[])
{
    shell_history_t saved;
    check(shell_history_load(&saved) == 0 && saved.count > 0U, "saved before spawn");
    const char* last = saved.entries[saved.count - 1U];
    if (strcmp(path, "probe") == 0) {
        check(argc == 2 && strcmp(argv[1], "two words") == 0, "recalled arguments");
        check(strcmp(last, "probe \"two words\"") == 0, "save before parser mutation");
    } else {
        check(strcmp(path, "reboot") == 0 && strcmp(last, "reboot") == 0, "save before reboot");
    }
    ++launches;
    return 1;
}

int test_shell_waitpid(int pid, int* status, int options)
{
    check(pid == 1 && options == 0, "wait child");
    *status = 0;
    return pid;
}

static void session(const char* commands)
{
    input = commands;
    check(shell_test_main(0, NULL) == 0, "shell loop");
}

static void wrapped_recall(void)
{
    shell_history_t history = {0};
    shell_line_t line       = {0};
    char command[SHELL_LINE_CAPACITY];
    memset(command, 'x', sizeof(command) - 1U);
    command[sizeof(command) - 1U] = '\0';
    check(shell_history_add(&history, command), "long command");
    FILE* output = tmpfile();
    check(output != NULL, "terminal transcript");
    check(fputs("T:/> ", output) >= 0, "prompt");
    const char* keys = "draft\033[A\033[A\033[B\033[B";
    while (*keys != '\0') {
        check(!shell_line_feed(&line, &history, (uint8_t) *keys++, output), "edit");
    }
    check(strcmp(line.text, "draft") == 0, "draft after maximum recall");
    rewind(output);
    static platform_pixel_t pixels[160U * 300U];
    platform_framebuffer_t framebuffer = {
        .pixels        = pixels,
        .width         = 160U,
        .height        = 300U,
        .stride_pixels = 160U,
    };
    terminal_t terminal;
    check(terminal_init(&terminal, &framebuffer, 1U), "terminal init");
    int byte;
    while ((byte = fgetc(output)) != EOF) {
        const char text[] = {(char) byte, '\0'};
        terminal_write(&terminal, text);
    }
    const char* expected = "T:/> draft";
    check(terminal.column == strlen(expected) && terminal.current_line == 0U, "wrapped cursor restored");
    for (size_t index = 0U; index < strlen(expected); ++index) {
        check(terminal.cells[index].character == (uint8_t) expected[index], "prompt and draft intact");
    }
    for (size_t index = strlen(expected); index < 260U; ++index) {
        check(terminal.cells[index].character == 0U, "long recall erased");
    }
    terminal_shutdown(&terminal);
    check(fclose(output) == 0, "close transcript");
}

int main(void)
{
    wrapped_recall();
    char original_directory[4096];
    check(getcwd(original_directory, sizeof(original_directory)) != NULL, "original directory");
    char root[] = "/tmp/tabos-shell-session-XXXXXX";
    check(mkdtemp(root) != NULL && chdir(root) == 0 && mkdir("T:", 0700) == 0, "temporary drive");
    FILE* output = tmpfile();
    check(output != NULL, "session output");
    const int saved_stdout = dup(STDOUT_FILENO);
    check(saved_stdout >= 0 && dup2(fileno(output), STDOUT_FILENO) >= 0, "capture stdout");
    session("probe \"two words\"\n\033[A\nhistory\nreboot\n");
    session("\033[A\n");
    check(fflush(stdout) == 0 && dup2(saved_stdout, STDOUT_FILENO) >= 0, "restore stdout");
    check(close(saved_stdout) == 0, "close saved stdout");
    rewind(output);
    char transcript[4096];
    const size_t count = fread(transcript, 1U, sizeof(transcript) - 1U, output);
    transcript[count]  = '\0';
    check(strstr(transcript, "1  probe \"two words\"\n2  history\n") != NULL, "numbered history includes itself");
    check(launches == 4U && scroll_mode_requests == 2U, "recall executes across restart");
    shell_history_t saved;
    check(shell_history_load(&saved) == 0 && saved.count == 3U, "consecutive recall deduplicated");
    check(fclose(output) == 0, "close output");
    check(unlink("T:/user/history.txt") == 0 && rmdir("T:/user") == 0 && rmdir("T:") == 0, "clean files");
    check(chdir(original_directory) == 0 && rmdir(root) == 0, "clean temporary root");
    return EXIT_SUCCESS;
}
