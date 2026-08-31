#include <tabos/ansi.h>
#include <tester/test.h>

#include <stdio.h>

void tester_expect(tester_context_t* context, bool condition, const char* message)
{
    context->assertions++;
    if (condition) {
        return;
    }
    context->failures++;
    printf("    [FAIL] %s\n", message);
}

void tester_run_test(tester_context_t* context, const tester_test_t* test)
{
    const unsigned int failures_before = context->failures;
    printf("[" ANSI_YELLOW "TEST" ANSI_RESET "] %s\n", test->name);
    test->run(context);
    printf("  [%s" ANSI_RESET "] %s\n", context->failures == failures_before ? ANSI_GREEN "PASS" : ANSI_RED "FAIL", test->name);
}
