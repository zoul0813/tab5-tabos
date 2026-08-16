#ifndef TABOS_SHELL_PARSER_H
#define TABOS_SHELL_PARSER_H

#include <stdint.h>

enum {
    SHELL_PARSE_TOO_MANY_ARGUMENTS = -1,
    SHELL_PARSE_UNTERMINATED_QUOTE = -2,
};

int shell_parse_arguments(char *line, char **argv, uint32_t capacity);

#endif
