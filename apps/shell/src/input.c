#include <shell/input.h>

shell_input_action_t shell_input_filter(shell_input_filter_t* filter, uint8_t input, char* output)
{
    switch (filter->state) {
        case SHELL_INPUT_TEXT:
            if (input == 0x1BU) {
                filter->state = SHELL_INPUT_ESCAPE;
            } else if (input == '\n') {
                return SHELL_INPUT_ENTER;
            } else if (input == '\b') {
                return SHELL_INPUT_BACKSPACE;
            } else if (input >= 0x20U && input <= 0x7EU) {
                *output = (char) input;
                return SHELL_INPUT_CHARACTER;
            }
            break;
        case SHELL_INPUT_ESCAPE:
            if (input == '[') {
                filter->state          = SHELL_INPUT_CSI;
                filter->csi_parameters = false;
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
                if (!filter->csi_parameters && input == 'A') {
                    return SHELL_INPUT_UP;
                }
                if (!filter->csi_parameters && input == 'B') {
                    return SHELL_INPUT_DOWN;
                }
            } else {
                filter->csi_parameters = true;
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
    return SHELL_INPUT_NONE;
}
