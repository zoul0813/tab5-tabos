#include <tester/test.h>

#include <tabos/process.h>

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

void tester_test_process(tester_context_t* context)
{
    const char* const arguments[] = {
        "T:/bin/tester",
        "--process-child",
        NULL,
    };

    const int first_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, first_status == 37, "child returns status after grandchild exits");

    const int second_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, second_status == 37, "nested process chain reloads after cleanup");

    const char* const resource_arguments[] = {
        "T:/bin/tester",
        "--process-resource-failure",
        NULL,
    };
    bool failures_returned = true;
    for (unsigned int index = 0U; index < 6U; ++index) {
        if (tabos_exec(resource_arguments[0], 2, resource_arguments) != 73) {
            failures_returned = false;
            break;
        }
    }
    tester_expect(context, failures_returned, "failed children return status after leaking owned resources");

    const int descriptor = open("T:/tabos-process-resource.tmp", O_RDWR);
    tester_expect(context, descriptor >= 3, "child descriptor resources are reclaimed after failure");
    if (descriptor >= 0) {
        (void) close(descriptor);
    }
    tester_expect(context, unlink("T:/tabos-process-resource.tmp") == 0,
                  "parent resumes and removes failed child fixture");
}
