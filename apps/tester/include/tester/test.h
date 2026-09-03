#ifndef TABOS_TESTER_TEST_H
#define TABOS_TESTER_TEST_H

#include <stdbool.h>

typedef struct {
        int argc;
        char** argv;
        unsigned int assertions;
        unsigned int failures;
} tester_context_t;

typedef void (*tester_test_fn)(tester_context_t* context);

typedef struct {
        const char* name;
        tester_test_fn run;
} tester_test_t;

void tester_expect(tester_context_t* context, bool condition, const char* message);
void tester_run_test(tester_context_t* context, const tester_test_t* test);

void tester_test_arguments(tester_context_t* context);
void tester_test_stdio(tester_context_t* context);
void tester_test_heap(tester_context_t* context);
void tester_test_filesystem(tester_context_t* context);
void tester_test_input(tester_context_t* context);
void tester_test_process(tester_context_t* context);
void tester_test_runtime(tester_context_t* context);
void tester_test_device(tester_context_t* context);
void tester_test_battery(tester_context_t* context);
void tester_test_audio(tester_context_t* context);
void tester_test_pointer(tester_context_t* context);
void tester_test_camera(tester_context_t* context);
void tester_test_graphics(tester_context_t* context);
void tester_test_network(tester_context_t* context);

#endif
