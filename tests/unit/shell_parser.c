#include <shell/parser.h>

#include <string.h>

static int expect_arguments(char* line, int expected_argc, const char* const* expected)
{
    char* argv[16];
    const int argc = shell_parse_arguments(line, argv, 16U);
    if (argc != expected_argc) {
        return 1;
    }
    for (int index = 0; index < argc; ++index) {
        if (strcmp(argv[index], expected[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    char plain[]                         = "hello one two";
    const char* const plain_expected[]   = {"hello", "one", "two"};
    char quoted[]                        = "hello \"two words\" 'three words'";
    const char* const quoted_expected[]  = {"hello", "two words", "three words"};
    char escaped[]                       = "hello escaped\\ value \"quoted\\\"value\" ''";
    const char* const escaped_expected[] = {"hello", "escaped value", "quoted\"value", ""};
    char unterminated[]                  = "hello \"broken";
    char overflow[]                      = "one two";
    char* argv[4];
    char* small_argv[1];

    return expect_arguments(plain, 3, plain_expected) || expect_arguments(quoted, 3, quoted_expected) ||
           expect_arguments(escaped, 4, escaped_expected) ||
           shell_parse_arguments(unterminated, argv, 4U) != SHELL_PARSE_UNTERMINATED_QUOTE ||
           shell_parse_arguments(overflow, small_argv, 1U) != SHELL_PARSE_TOO_MANY_ARGUMENTS;
}
