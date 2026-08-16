#include <tester/test.h>

#include <stdio.h>

int main(int argc, char **argv)
{
    static const tester_test_t tests[] = {
        {"Arguments", tester_test_arguments},
        {"Standard I/O", tester_test_stdio},
        {"Heap", tester_test_heap},
        {"Filesystem and working directory", tester_test_filesystem},
        {"Nonblocking input", tester_test_input},
    };
    tester_context_t context = {.argc = argc, .argv = argv};

    puts("TabOS SDK tester");
    for (unsigned int index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        tester_run_test(&context, &tests[index]);
    }
    printf("\nAssertions: %u; failures: %u\n", context.assertions, context.failures);
    puts(context.failures == 0U ? "[PASS] TabOS SDK tester" : "[FAIL] TabOS SDK tester");
    return context.failures == 0U ? 0 : 1;
}
