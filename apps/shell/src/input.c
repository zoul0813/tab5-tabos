#include <shell/input.h>

bool shell_input_filter(shell_input_filter_t* filter, uint8_t input, char* output)
{
    switch (filter->state) {
        case SHELL_INPUT_TEXT:
            if (input == 0x1BU) {
                filter->state = SHELL_INPUT_ESCAPE;
            } else if (input >= 0x20U && input <= 0x7EU) {
                *output = (char) input;
                return true;
            }
            break;
        case SHELL_INPUT_ESCAPE:
            if (input == '[') {
                filter->state = SHELL_INPUT_CSI;
            } else if (input == ']' || input == 'P' || input == 'X' || input == '^' || input == '_') {
                filter->state = SHELL_INPUT_STRING;
            } else if (input < 0x20U || input > 0x2FU) {
                filter->state = SHELL_INPUT_TEXT;
            }
            break;
        case SHELL_INPUT_CSI:
            if (input == 0x1BU) {
                filter->state = SHELL_INPUT_ESCAPE;
            } else if (input >= 0x40U && input <= 0x7EU) {
                filter->state = SHELL_INPUT_TEXT;
            }
            break;
        case SHELL_INPUT_STRING:
            if (input == 0x07U) {
                filter->state = SHELL_INPUT_TEXT;
            } else if (input == 0x1BU) {
                filter->state = SHELL_INPUT_STRING_ESCAPE;
            }
            break;
        case SHELL_INPUT_STRING_ESCAPE:
            if (input == '\\') {
                filter->state = SHELL_INPUT_TEXT;
            } else if (input != 0x1BU) {
                filter->state = SHELL_INPUT_STRING;
            }
            break;
    }
    return false;
}
