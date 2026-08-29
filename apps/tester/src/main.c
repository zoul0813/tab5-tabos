#include <tester/test.h>

#include <tabos/process.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    PROCESS_LEAK_DESCRIPTOR_COUNT = 8,
};

static int run_resource_failure_fixture(void)
{
    void* allocation = malloc(4096U);
    if (allocation == NULL) {
        return 74;
    }
    memset(allocation, 0x5a, 4096U);

    for (unsigned int index = 0U; index < PROCESS_LEAK_DESCRIPTOR_COUNT; ++index) {
        const int descriptor = open("T:/tabos-process-resource.tmp", O_CREAT | O_RDWR, 0644);
        if (descriptor < 0) {
            return 75;
        }
    }
    return 73;
}

static int run_process_fixture(int argc, char** argv)
{
    if (argc != 2 || argv == NULL) {
        return -1;
    }
    if (strcmp(argv[1], "--process-leaf") == 0) {
        return 23;
    }
    if (strcmp(argv[1], "--process-resource-failure") == 0) {
        return run_resource_failure_fixture();
    }
    if (strcmp(argv[1], "--process-child") != 0) {
        return -1;
    }

    const char* const arguments[] = {
        "T:/bin/tester",
        "--process-leaf",
        NULL,
    };
    const int status = tabos_exec(arguments[0], 2, arguments);
    return status == 23 ? 37 : 38;
}

int main(int argc, char** argv)
{
    const int fixture_status = run_process_fixture(argc, argv);
    if (fixture_status >= 0) {
        return fixture_status;
    }

    static const tester_test_t tests[] = {
        {                       "Arguments",  tester_test_arguments},
        {                    "Standard I/O",      tester_test_stdio},
        {                            "Heap",       tester_test_heap},
        {"Filesystem and working directory", tester_test_filesystem},
        {               "Nonblocking input",      tester_test_input},
        {        "Nested process execution",    tester_test_process},
        {     "Time and system information",    tester_test_runtime},
        {              "TCP/UDP networking",    tester_test_network},
        {             "Fullscreen graphics",   tester_test_graphics},
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
