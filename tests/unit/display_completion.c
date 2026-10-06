#define _POSIX_C_SOURCE 200809L

#include "display_completion.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int semaphore;
static BaseType_t completion_result = pdTRUE;
static unsigned int wait_calls;
static volatile sig_atomic_t timeout_observed;
static volatile sig_atomic_t fault_logged;

BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t timeout)
{
    assert(handle == &semaphore);
    assert(timeout == pdMS_TO_TICKS(2000U));
    assert(timeout != 0U && timeout != portMAX_DELAY);
    ++wait_calls;
    timeout_observed = completion_result == pdFALSE;
    return completion_result;
}

void fake_display_log(const char* tag, const char* format, ...)
{
    assert(tag != NULL);
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    (void) vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    assert(strstr(message, "completion timed out") != NULL);
    assert(strstr(message, "DMA ownership is uncertain") != NULL);
    fault_logged = 1;
}

static void fatal_signal(int signal_number)
{
    // Avoid a crash report/core file while proving the real abort path is taken.
    _Exit(signal_number == SIGABRT && timeout_observed && fault_logged ? 91 : 92);
}

static void test_missing_completion(const char* operation)
{
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(signal(SIGABRT, fatal_signal) != SIG_ERR);
        completion_result = pdFALSE;
        (void) tab5_display_wait_completion(&semaphore, operation);
        // Neither fallback nor buffer reclamation may become reachable.
        _Exit(93);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 91);
}

int main(void)
{
    assert(tab5_display_wait_completion(&semaphore, "VSYNC"));
    assert(tab5_display_wait_completion(&semaphore, "PPA rotation"));
    assert(tab5_display_wait_completion(&semaphore, "PPA graphics"));
    assert(wait_calls == 3U && !fault_logged);
    test_missing_completion("VSYNC");
    test_missing_completion("PPA rotation");
    test_missing_completion("PPA graphics");
    assert(wait_calls == 3U && !fault_logged);
    return 0;
}
