#include <tester/test.h>

#include <tabos/process.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

void tester_test_concurrent_process(tester_context_t* context)
{
    const char* const first_args[]  = {"T:/bin/tester", "--concurrent-peer", "1", NULL};
    const char* const second_args[] = {"T:/bin/tester", "--concurrent-peer", "2", NULL};
    for (unsigned int round = 0U; round < 3U; ++round) {
        const int first  = tabos_spawn(first_args[0], 3, first_args);
        const int second = tabos_spawn(second_args[0], 3, second_args);
        tester_expect(context, first > 0 && second > 0 && first != second, "concurrent children have distinct PIDs");
        int first_status  = -1;
        int second_status = -1;
        if (first > 0) {
            tester_expect(context, tabos_waitpid(first, &first_status) == first, "wait reaps first actual PID");
            tester_expect(context, tabos_waitpid(first, NULL) == -ECHILD, "second reap rejected");
        }
        if (second > 0) {
            tester_expect(context, tabos_waitpid(second, &second_status) == second, "wait reaps second actual PID");
        }
        tester_expect(context, first_status == 91 && second_status == 92,
                      "both independent RV32 children progress while parent waits; background display denied");
        (void) unlink("T:/tabos-concurrent-1.tmp");
        (void) unlink("T:/tabos-concurrent-2.tmp");
    }
}

void tester_test_process(tester_context_t* context)
{
    tester_test_concurrent_process(context);
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
        const int status = tabos_exec(resource_arguments[0], 2, resource_arguments);
        if (status != 73) {
            printf("    resource child %u returned %d; expected 73\n", index + 1U, status);
            failures_returned = false;
            break;
        }
    }
    tester_expect(context, failures_returned,
                  "failed children return status after leaking owned files, heap, sockets, and subscriptions");

    const int descriptor = open("T:/tabos-process-resource.tmp", O_RDWR);
    tester_expect(context, descriptor >= 3, "child descriptor resources are reclaimed after failure");
    if (descriptor >= 0) {
        (void) close(descriptor);
    }
    tester_expect(context, unlink("T:/tabos-process-resource.tmp") == 0,
                  "parent resumes and removes failed child fixture");
}
