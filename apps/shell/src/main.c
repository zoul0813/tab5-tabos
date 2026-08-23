#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/stat.h>
#include <string.h>
#include <tabos/process.h>

#include <shell/parser.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <tabos/tty.h>
#include <stdbool.h>

enum {
    SHELL_LINE_CAPACITY     = 256,
    SHELL_IO_CAPACITY       = 1024,
    SHELL_ARGUMENT_CAPACITY = TABOS_PROCESS_ARG_MAX,
};

static char shell_path[256] = "T:/bin";

static uint32_t string_length(const char* text)
{
    uint32_t length = 0U;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int string_equal(const char* left, const char* right)
{
    uint32_t index = 0U;
    while (left[index] != '\0' && left[index] == right[index]) {
        index++;
    }
    return left[index] == right[index];
}

static int string_starts_with(const char* text, const char* prefix)
{
    uint32_t index = 0U;
    while (prefix[index] != '\0' && text[index] == prefix[index]) {
        index++;
    }
    return prefix[index] == '\0';
}

static void copy_string(char* destination, uint32_t capacity, const char* source)
{
    uint32_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static void append_string(char* destination, uint32_t capacity, const char* source)
{
    uint32_t used  = string_length(destination);
    uint32_t index = 0U;
    while (used + 1U < capacity && source[index] != '\0') {
        destination[used++] = source[index++];
    }
    destination[used] = '\0';
}

static void write_error(const char* operation, int status)
{
    char number[16];
    char message[96];
    uint32_t used      = 0U;
    unsigned int value = (unsigned int) (status < 0 ? -status : status);
    do {
        number[used++]  = (char) ('0' + (value % 10U));
        value          /= 10U;
    } while (value != 0U && used < sizeof(number) - 1U);
    if (status < 0 && used < sizeof(number) - 1U) {
        number[used++] = '-';
    }
    for (uint32_t left = 0U, right = used - 1U; left < right; left++, right--) {
        const char temporary = number[left];
        number[left]         = number[right];
        number[right]        = temporary;
    }
    number[used] = '\0';
    copy_string(message, sizeof(message), operation);
    append_string(message, sizeof(message), " failed: ");
    append_string(message, sizeof(message), number);
    printf("%s\n", message);
}

static void write_exit_status(int status)
{
    char number[16];
    char message[40];
    uint32_t used      = 0U;
    unsigned int value = (unsigned int) (status < 0 ? -status : status);
    do {
        number[used++]  = (char) ('0' + (value % 10U));
        value          /= 10U;
    } while (value != 0U && used < sizeof(number) - 1U);
    if (status < 0 && used < sizeof(number) - 1U) {
        number[used++] = '-';
    }
    for (uint32_t left = 0U, right = used - 1U; left < right; left++, right--) {
        const char temporary = number[left];
        number[left]         = number[right];
        number[right]        = temporary;
    }
    number[used] = '\0';
    copy_string(message, sizeof(message), "Exit status: ");
    append_string(message, sizeof(message), number);
    printf("%s\n", message);
}

static void execute_command(char* line)
{
    char* argv[SHELL_ARGUMENT_CAPACITY];
    const int parsed = shell_parse_arguments(line, argv, SHELL_ARGUMENT_CAPACITY);
    if (parsed == 0) {
        return;
    }
    if (parsed < 0) {
        printf("%s\n", parsed == SHELL_PARSE_UNTERMINATED_QUOTE ? "Unterminated quote" : "Too many arguments");
        return;
    }
    const uint32_t argc = (uint32_t) parsed;
    const char* command = argv[0];

    if (string_equal(command, "help")) {
        printf("Commands: help clear pwd cd set echo <program>\n");
        return;
    }
    if (string_equal(command, "set")) {
        if (argc == 2U && string_starts_with(argv[1], "PATH=")) {
            copy_string(shell_path, sizeof(shell_path), argv[1] + 5U);
        } else {
            printf("Usage: set PATH=<directories>\n");
        }
        return;
    }
    if (string_equal(command, "echo") && argc == 2U &&
        (string_equal(argv[1], "%PATH%") || string_equal(argv[1], "$PATH"))) {
        printf("%s\n", shell_path);
        return;
    }
    if (string_equal(command, "clear")) {
        printf("\033[2J\033[H");
        return;
    }
    if (string_equal(command, "pwd")) {
        char path[512];
        if (getcwd(path, sizeof(path)) != NULL) {
            printf("%s\n", path);
        } else {
            write_error("pwd", -errno);
        }
        return;
    }
    if (string_equal(command, "cd")) {
        if (argc != 2U) {
            printf("Usage: cd <path>\n");
            return;
        }
        if (chdir(argv[1]) != 0) {
            write_error("cd", -errno);
        }
        return;
    }

    char path[512];
    if (strchr(command, '/') != NULL) {
        copy_string(path, sizeof(path), command);
    } else {
        bool found        = false;
        const char* start = shell_path;
        while (*start != '\0' && !found) {
            const char* end     = strchr(start, ';');
            const size_t length = end == NULL ? string_length(start) : (size_t) (end - start);
            if (length + 1U + string_length(command) + 1U < sizeof(path)) {
                memcpy(path, start, length);
                path[length] = '/';
                copy_string(path + length + 1U, sizeof(path) - length - 1U, command);
                struct stat status;
                found = stat(path, &status) == 0 && S_ISREG(status.st_mode);
            }
            if (end == NULL) {
                break;
            }
            start = end + 1U;
        }
        if (!found) {
            copy_string(path, sizeof(path), command);
        }
    }
    const int pid = tabos_spawn(path, (int) argc, (const char* const*) argv);
    int status    = pid < 0 ? pid : 0;
    if (pid >= 0 && waitpid(pid, &status, 0) < 0) {
        status = -errno;
    }
    if (status != 0) {
        write_exit_status(status);
    }
}

static void prompt(void)
{
    char path[512];
    if (getcwd(path, sizeof(path)) != NULL) {
        printf("%s> ", path);
    } else {
        printf("> ");
    }
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    char line[SHELL_LINE_CAPACITY];
    uint32_t used = 0U;
    line[0]       = '\0';

    // disable buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    if (ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, (uint32_t) TABOS_TTY_MODE_SCROLL_KEYS) != 0) {
        fprintf(stderr, "shell: could not enable terminal scrollback\n");
    }

    printf("TabOS shell\n");
    prompt();
    for (;;) {
        char input[16];
        const int count = (int) read(STDIN_FILENO, input, sizeof(input));
        if (count <= 0) {
            if (count < 0 && errno != EAGAIN) {
                break;
            }
            continue;
        }
        for (int index = 0; index < count; ++index) {
            const char character = input[index];
            if (character == '\n') {
                line[used] = '\0';
                putchar('\n');
                execute_command(line);
                used    = 0U;
                line[0] = '\0';
                prompt();
            } else if (character == '\b') {
                if (used > 0U) {
                    used--;
                    line[used] = '\0';
                    putchar('\b');
                }
            } else if ((unsigned char) character >= 32U && used + 1U < sizeof(line)) {
                line[used++] = character;
                line[used]   = '\0';
                putchar((unsigned char) character);
            }
        }
    }
}
