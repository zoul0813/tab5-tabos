#include <shell/parser.h>

int shell_parse_arguments(char* line, char** argv, uint32_t capacity)
{
    uint32_t argc = 0U;
    char* read    = line;
    char* write   = line;
    while (*read != '\0') {
        while (*read == ' ') {
            read++;
        }
        if (*read == '\0') {
            break;
        }
        if (argc >= capacity) {
            return SHELL_PARSE_TOO_MANY_ARGUMENTS;
        }
        argv[argc++] = write;
        char quote   = '\0';
        while (*read != '\0') {
            if (*read == '\\' && read[1] != '\0') {
                *write++  = read[1];
                read     += 2;
            } else if (quote != '\0') {
                if (*read == quote) {
                    quote = '\0';
                    read++;
                } else {
                    *write++ = *read++;
                }
            } else if (*read == '\'' || *read == '"') {
                quote = *read++;
            } else if (*read == ' ') {
                break;
            } else {
                *write++ = *read++;
            }
        }
        if (quote != '\0') {
            return SHELL_PARSE_UNTERMINATED_QUOTE;
        }
        while (*read == ' ') {
            read++;
        }
        *write++ = '\0';
    }
    return (int) argc;
}
