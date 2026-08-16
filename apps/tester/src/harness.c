#include <tester/test.h>

#include <stdio.h>

void tester_expect(tester_context_t *context, bool condition, const char *message)
{
    context->assertions++;
    if (condition) return;
    context->failures++;
    printf("    [FAIL] %s\n", message);
}

void tester_run_test(tester_context_t *context, const tester_test_t *test)
{
    const unsigned int failures_before = context->failures;
    printf("[TEST] %s\n", test->name);
    test->run(context);
    printf("  [%s] %s\n",
           context->failures == failures_before ? "PASS" : "FAIL", test->name);
}
