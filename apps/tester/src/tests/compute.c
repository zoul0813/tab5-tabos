#include <tester/test.h>
#include <tabos/compute.h>
#include <tabos/filesystem.h>
#include <tabos/process.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        uint32_t seed;
        uint32_t result;
} compute_data_t;

static void calculate(void* argument)
{
    compute_data_t* data = argument;
    uint32_t value       = data->seed;
    for (unsigned i = 0U; i < 10000U; ++i) {
        value = value * 1664525U + 1013904223U;
    }
    data->result = value;
}

static void forbidden(void* argument)
{
    (void) argument;
    (void) tabos_compute_poll();
}

void tester_test_compute(tester_context_t* context)
{
    compute_data_t* data = malloc(sizeof(*data));
    tester_expect(context, data != NULL, "compute heap allocation");
    if (data == NULL) {
        return;
    }
    *data                   = (compute_data_t) {.seed = 17U};
    compute_data_t expected = *data;
    calculate(&expected);
    const int submitted = tabos_compute_submit(calculate, data, sizeof(*data));
    if (submitted == -TABOS_ENOTSUP) {
        calculate(data);
        tester_expect(context, data->result == expected.result, "synchronous compute fallback matches");
        tester_expect(context, tabos_compute_poll() == -TABOS_ENOTSUP && tabos_compute_wait() == -TABOS_ENOTSUP,
                      "host reports unsupported compute gates");
    } else {
        tester_expect(context, submitted == 0, "native compute accepted");
        if (submitted == 0) {
            tester_expect(context, tabos_compute_submit(calculate, data, sizeof(*data)) == -TABOS_EBUSY,
                          "completion must be consumed before next submit");
            tester_expect(context, tabos_compute_wait() == 0 && data->result == expected.result,
                          "compute wait acquires identical result");
            tester_expect(context, tabos_compute_poll() == 1 && tabos_compute_wait() == 0, "idle compute calls");
            tester_expect(context, tabos_compute_submit(NULL, data, sizeof(*data)) == -TABOS_EINVAL,
                          "compute rejects null callback");
            tester_expect(context, tabos_compute_submit(calculate, &expected, sizeof(expected)) == -TABOS_EINVAL,
                          "compute rejects stack data");
            tester_expect(context, tabos_compute_submit(calculate, data, UINT32_MAX) == -TABOS_EINVAL,
                          "compute rejects overflowing heap range");
            tester_expect(context,
                          tabos_compute_submit(forbidden, data, sizeof(*data)) == 0 &&
                              tabos_compute_wait() == -TABOS_EPERM,
                          "compute rejects SDK services");
            data->result = 0U;
            tester_expect(context, tabos_compute_submit(calculate, data, sizeof(*data)) == 0,
                          "compute recovers after rejected service");
            const char* path = context->argv[0];
            if (strchr(path, '/') == NULL && strchr(path, ':') == NULL) {
                path = "T:/bin/tester";
            }
            const char* arguments[] = {path, "--process-leaf"};
            tester_expect(context, tabos_exec(path, 2, arguments) == 23 && data->result == expected.result,
                          "nested execution consumes outstanding compute");
            (void) tabos_compute_wait();
        }
    }
    free(data);
}
