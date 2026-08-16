#include <tabos/elf_api.h>

extern const tabos_elf_api_t *tabos_runtime_api;

#include <shell/parser.h>

#include <stdint.h>

enum {
    SHELL_LINE_CAPACITY = 256,
    SHELL_IO_CAPACITY = 1024,
    SHELL_ARGUMENT_CAPACITY = TABOS_ELF_ARG_MAX,
};

static uint32_t string_length(const char *text)
{
    uint32_t length = 0U;
    while (text[length] != '\0') length++;
    return length;
}

static int string_equal(const char *left, const char *right)
{
    uint32_t index = 0U;
    while (left[index] != '\0' && left[index] == right[index]) index++;
    return left[index] == right[index];
}

static int string_starts_with(const char *text, const char *prefix)
{
    uint32_t index = 0U;
    while (prefix[index] != '\0' && text[index] == prefix[index]) index++;
    return prefix[index] == '\0';
}

static void copy_string(char *destination, uint32_t capacity, const char *source)
{
    uint32_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static void append_string(char *destination, uint32_t capacity, const char *source)
{
    uint32_t used = string_length(destination);
    uint32_t index = 0U;
    while (used + 1U < capacity && source[index] != '\0') {
        destination[used++] = source[index++];
    }
    destination[used] = '\0';
}

static void write_error(const tabos_elf_api_t *api, const char *operation, int status)
{
    char number[16];
    char message[96];
    uint32_t used = 0U;
    unsigned int value = (unsigned int)(status < 0 ? -status : status);
    do {
        number[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && used < sizeof(number) - 1U);
    if (status < 0 && used < sizeof(number) - 1U) number[used++] = '-';
    for (uint32_t left = 0U, right = used - 1U; left < right; left++, right--) {
        const char temporary = number[left];
        number[left] = number[right];
        number[right] = temporary;
    }
    number[used] = '\0';
    copy_string(message, sizeof(message), operation);
    append_string(message, sizeof(message), " failed: ");
    append_string(message, sizeof(message), number);
    api->console_write(message);
}

static void write_exit_status(const tabos_elf_api_t *api, int status)
{
    char number[16];
    char message[40];
    uint32_t used = 0U;
    unsigned int value = (unsigned int)(status < 0 ? -status : status);
    do {
        number[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && used < sizeof(number) - 1U);
    if (status < 0 && used < sizeof(number) - 1U) number[used++] = '-';
    for (uint32_t left = 0U, right = used - 1U; left < right; left++, right--) {
        const char temporary = number[left];
        number[left] = number[right];
        number[right] = temporary;
    }
    number[used] = '\0';
    copy_string(message, sizeof(message), "Exit status: ");
    append_string(message, sizeof(message), number);
    api->console_write(message);
}

static void execute_command(const tabos_elf_api_t *api, char *line)
{
    char *argv[SHELL_ARGUMENT_CAPACITY];
    const int parsed = shell_parse_arguments(line, argv, SHELL_ARGUMENT_CAPACITY);
    if (parsed == 0) return;
    if (parsed < 0) {
        api->console_write(parsed == SHELL_PARSE_UNTERMINATED_QUOTE
            ? "Unterminated quote" : "Too many arguments");
        return;
    }
    const uint32_t argc = (uint32_t)parsed;
    const char *command = argv[0];

    if (string_equal(command, "help")) {
        api->console_write("Commands: help clear pwd cd ls <program>");
        return;
    }
    if (string_equal(command, "clear")) {
        (void)api->console_clear();
        return;
    }
    if (string_equal(command, "pwd")) {
        char path[512];
        const int result = api->fs_getcwd(path, sizeof(path));
        if (result == 0) api->console_write(path);
        else write_error(api, "pwd", result);
        return;
    }
    if (string_equal(command, "cd")) {
        if (argc != 2U) {
            api->console_write("Usage: cd <path>");
            return;
        }
        const int result = api->fs_chdir(argv[1]);
        if (result != 0) write_error(api, "cd", result);
        return;
    }
    if (string_equal(command, "ls")) {
        if (argc > 2U) {
            api->console_write("Usage: ls [path]");
            return;
        }
        const char *path = argc == 2U ? argv[1] : ".";
        char listing[SHELL_IO_CAPACITY];
        const int result = api->fs_list(path, listing, sizeof(listing));
        if (result == 0) api->console_write_raw(listing);
        else write_error(api, "ls", result);
        return;
    }

    char path[512];
    const uint32_t command_length = string_length(command);
    if (string_starts_with(command, "A:/") || string_starts_with(command, "T:/") ||
        command[0] == '/') {
        copy_string(path, sizeof(path), command);
    } else {
        copy_string(path, sizeof(path), "T:/bin/");
        append_string(path, sizeof(path), command);
        if (!string_starts_with(command + (command_length > 4U ? command_length - 4U : 0U),
                                ".bin")) {
            append_string(path, sizeof(path), ".bin");
        }
    }
    int status = TABOS_ELF_EXEC_PENDING;
    while (status == TABOS_ELF_EXEC_PENDING) {
        status = api->exec(path, argc, (const char *const *)argv);
        if (status == TABOS_ELF_EXEC_PENDING) api->yield();
    }
    write_exit_status(api, status);
}

int main(int argc, char **argv)
{
    const tabos_elf_api_t *api = tabos_runtime_api;
    (void)argc;
    (void)argv;
    if (api == 0 || api->abi_version != TABOS_ELF_API_VERSION ||
        api->console_read == 0 || api->console_write_raw == 0 || api->exec == 0) {
        return 2;
    }

    char line[SHELL_LINE_CAPACITY];
    uint32_t used = 0U;
    line[0] = '\0';
    api->console_write("TabOS shell");
    api->console_write_raw("> ");
    for (;;) {
        char input[16];
        const int count = api->console_read(input, sizeof(input));
        if (count <= 0) {
            api->yield();
            continue;
        }
        for (int index = 0; index < count; ++index) {
            const char character = input[index];
            if (character == '\n') {
                line[used] = '\0';
                api->console_write_raw("\n");
                execute_command(api, line);
                used = 0U;
                line[0] = '\0';
                api->console_write_raw("> ");
            } else if (character == '\b') {
                if (used > 0U) {
                    used--;
                    line[used] = '\0';
                    api->console_write_raw("\b");
                }
            } else if ((unsigned char)character >= 32U && used + 1U < sizeof(line)) {
                line[used++] = character;
                line[used] = '\0';
                char echoed[2] = {character, '\0'};
                api->console_write_raw(echoed);
            }
        }
    }
}
