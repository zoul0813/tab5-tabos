#include <tester/test.h>

#include <tabos/process.h>

#include <stdio.h>
#include <string.h>

static int run_process_fixture(int argc, char **argv)
{
    if (argc != 2 || argv == NULL) return -1;
    if (strcmp(argv[1], "--process-leaf") == 0) return 23;
    if (strcmp(argv[1], "--process-child") != 0) return -1;

    const char *const arguments[] = {
        "T:/bin/tester",
        "--process-leaf",
        NULL,
    };
    const int status = tabos_exec(arguments[0], 2, arguments);
    return status == 23 ? 37 : 38;
}

int main(int argc, char **argv)
{
    const int fixture_status = run_process_fixture(argc, argv);
    if (fixture_status >= 0) return fixture_status;

    static const tester_test_t tests[] = {
        {"Arguments", tester_test_arguments},
        {"Standard I/O", tester_test_stdio},
        {"Heap", tester_test_heap},
        {"Filesystem and working directory", tester_test_filesystem},
        {"Nonblocking input", tester_test_input},
        {"Nested process execution", tester_test_process},
        {"Time and system information", tester_test_runtime},
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
