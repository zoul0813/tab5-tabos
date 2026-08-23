#include <tester/test.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void tester_test_input(tester_context_t* context)
{
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
}
