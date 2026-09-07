#include <tester/test.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <tabos/process.h>
#include <tabos/runtime_time.h>
#include <stdio.h>
#include <string.h>

void tester_test_input(tester_context_t* context)
{
    tabos_tty_size_t size;
    tester_expect(context, ioctl(0, TABOS_TTY_GET_SIZE, &size) == 0 && size.rows > 0U && size.columns > 0U,
                  "TTY copied geometry");
    tester_expect(context, ioctl(3, TABOS_TTY_GET_SIZE, &size) < 0 && errno == ENOTTY, "TTY rejects file descriptor");
    const tabos_wait_source_t source = tabos_input_wait_source();
    tester_expect(context, source >= 0 && tabos_input_wait_source() == source,
                  "keyboard wait source stable and process-owned");
    tabos_wait_item_t item = {.source = source, .events = TABOS_WAIT_WRITABLE};
    tester_expect(context, tabos_wait(&item, 1U, 0U) < 0 && errno == EINVAL, "keyboard rejects write readiness");
    item.events = TABOS_WAIT_READABLE;
    tester_expect(context, tabos_wait(&item, 1U, 0U) >= 0, "keyboard readiness poll");
    const int original_flags = fcntl(STDIN_FILENO, F_GETFL);
    tester_expect(context, original_flags >= 0, "fcntl reads stdin flags");
    if (original_flags < 0) {
        return;
    }

    tester_expect(context, fcntl(STDIN_FILENO, F_SETFL, original_flags | O_NONBLOCK) == 0, "fcntl enables O_NONBLOCK");
    bool reached_empty = false;
    for (unsigned int attempt = 0; attempt < 64U; ++attempt) {
        char byte;
        errno                = 0;
        const ssize_t result = read(STDIN_FILENO, &byte, 1U);
        if (result < 0 && errno == EAGAIN) {
            reached_empty = true;
            break;
        }
        if (result < 0) {
            break;
        }
    }
    tester_expect(context, reached_empty, "empty nonblocking stdin reports EAGAIN");
    tester_expect(context, fcntl(STDIN_FILENO, F_SETFL, original_flags) == 0, "fcntl restores stdin flags");
    const char* executable = context->argv[0];
    // The shell preserves a bare command in argv[0]; tabos_exec does not search PATH.
    if (strchr(executable, '/') == NULL && strchr(executable, ':') == NULL) {
        executable = "T:/bin/tester";
    }
    char handle[32];
    (void) snprintf(handle, sizeof(handle), "%ld", (long) source);
    const char* foreign_args[] = {executable, "--foreign-wait-source", handle};
    tester_expect(context, tabos_exec(executable, 3, foreign_args) == 79, "child rejects parent's keyboard source");
    uint32_t inherited = 0U;
    tester_expect(context, ioctl(0, TABOS_TTY_GET_MODE, &inherited) == 0, "read parent TTY policy");
    const char* leak_args[] = {executable, "--input-source"};
    const int stale         = tabos_exec(executable, 2, leak_args);
    item.source             = stale;
    tester_expect(context, stale > 0 && tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF,
                  "child keyboard source invalid after teardown");
    uint32_t restored = UINT32_MAX;
    tester_expect(context, ioctl(0, TABOS_TTY_GET_MODE, &restored) == 0 && restored == inherited,
                  "child TTY changes preserve parent policy");
    item.source = source;
    tester_expect(context, tabos_wait(&item, 1U, 0U) >= 0, "parent source still valid after child");
    // Operator input can make a finite wait immediately ready; timeout must never
    // be reported before the monotonic deadline.
    const uint64_t before = tabos_monotonic_ms();
    const int waited      = tabos_wait(&item, 1U, 20U);
    tester_expect(context, waited == 1 || (waited == 0 && tabos_monotonic_ms() - before >= 20U),
                  "keyboard finite deadline or queued readiness");
}
