#ifndef SHELL_LINE_H
#define SHELL_LINE_H

#include <shell/history.h>
#include <shell/input.h>
#include <stdio.h>

typedef struct {
        char text[SHELL_LINE_CAPACITY];
        size_t used;
        shell_input_filter_t filter;
} shell_line_t;

// Returns true after Enter; caller records the unparsed text, executes and resets.
bool shell_line_feed(shell_line_t* line, shell_history_t* history, uint8_t byte, FILE* output);
void shell_line_reset(shell_line_t* line, shell_history_t* history);

#endif
