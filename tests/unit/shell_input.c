#include <shell/input.h>

#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "shell input test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static uint32_t filter_bytes(shell_input_filter_t* filter, const uint8_t* input, uint32_t count, char* output)
{
    uint32_t used = 0U;
    for (uint32_t index = 0U; index < count; ++index) {
        if (shell_input_filter(filter, input[index], &output[used])) {
            used++;
        }
    }
    output[used] = '\0';
    return used;
}

int main(void)
{
    shell_input_filter_t filter = {.state = SHELL_INPUT_TEXT};
    char output[32];
    const uint8_t arrows[] = {'a', 0x1B, '[', 'A', 0x1B, '[', 'B', 0x1B, '[', 'C', 0x1B, '[', 'D', 'b'};
    check(filter_bytes(&filter, arrows, sizeof(arrows), output) == 2U, "arrow output length");
    check(output[0] == 'a' && output[1] == 'b', "arrows ignored");

    const uint8_t split_start[] = {0x1B, '[', '1', ';'};
    const uint8_t split_end[]   = {'5', '~', 'c'};
    check(filter_bytes(&filter, split_start, sizeof(split_start), output) == 0U, "split CSI start ignored");
    check(filter_bytes(&filter, split_end, sizeof(split_end), output) == 1U && output[0] == 'c',
          "split CSI end ignored");

    const uint8_t osc[] = {0x1B, ']', '0', ';', 't', 'i', 't', 'l', 'e', 0x1B, '\\', 'd'};
    check(filter_bytes(&filter, osc, sizeof(osc), output) == 1U && output[0] == 'd', "OSC ignored");

    const uint8_t non_ascii[] = {0x01, 0x7F, 0x80, 0xFF, 'e'};
    check(filter_bytes(&filter, non_ascii, sizeof(non_ascii), output) == 1U && output[0] == 'e', "non-ASCII ignored");
    return EXIT_SUCCESS;
}
