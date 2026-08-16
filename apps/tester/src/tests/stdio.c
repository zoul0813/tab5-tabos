#include <tester/test.h>

#include <stdio.h>
#include <string.h>

void tester_test_stdio(tester_context_t *context)
{
    char buffer[64];
    const int length = snprintf(buffer, sizeof(buffer), "%s %d %02x", "format", 42, 0xa5);
    tester_expect(context, length == 12, "snprintf reports formatted length");
    tester_expect(context, strcmp(buffer, "format 42 a5") == 0, "snprintf formats values");
    tester_expect(context, fputs("    stdout through fputs\n", stdout) >= 0,
                  "fputs writes stdout");
    tester_expect(context, fputc('.', stdout) == '.', "fputc writes one byte");
    tester_expect(context, putchar('\n') == '\n', "putchar writes newline");
    tester_expect(context, fprintf(stderr, "    stderr through fprintf\n") > 0,
                  "fprintf writes stderr");
}
