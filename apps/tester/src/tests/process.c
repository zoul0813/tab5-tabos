#include <tester/test.h>

#include <tabos/process.h>

#include <stddef.h>

void tester_test_process(tester_context_t *context)
{
    const char *const arguments[] = {
        "T:/bin/tester",
        "--process-child",
        NULL,
    };

    const int first_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, first_status == 37,
                  "child returns status after grandchild exits");

    const int second_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, second_status == 37,
                  "nested process chain reloads after cleanup");
}
