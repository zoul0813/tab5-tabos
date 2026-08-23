#include <tester/test.h>

#include <stdio.h>

void tester_test_arguments(tester_context_t* context)
{
    tester_expect(context, context->argc >= 1, "argc includes executable name");
    tester_expect(context, context->argv != NULL, "argv is available");
    if (context->argv == NULL) {
        return;
    }

    for (int index = 0; index < context->argc; ++index) {
        tester_expect(context, context->argv[index] != NULL, "argument is not null");
        if (context->argv[index] != NULL) {
            printf("    argv[%d] = %s\n", index, context->argv[index]);
        }
    }
    tester_expect(context, context->argv[context->argc] == NULL, "argv is null terminated");
}
