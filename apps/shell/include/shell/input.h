#ifndef SHELL_INPUT_H
#define SHELL_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SHELL_INPUT_TEXT,
    SHELL_INPUT_ESCAPE,
    SHELL_INPUT_CSI,
    SHELL_INPUT_STRING,
    SHELL_INPUT_STRING_ESCAPE,
} shell_input_state_t;

typedef struct {
        shell_input_state_t state;
} shell_input_filter_t;

bool shell_input_filter(shell_input_filter_t* filter, uint8_t input, char* output);

#endif
