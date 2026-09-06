#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

enum {
    SHELL_HISTORY_CAPACITY = 32,
    SHELL_LINE_CAPACITY    = 256,
};

typedef struct {
        char entries[SHELL_HISTORY_CAPACITY][SHELL_LINE_CAPACITY];
        size_t count;
        size_t cursor;
        char draft[SHELL_LINE_CAPACITY];
} shell_history_t;

bool shell_history_add(shell_history_t* history, const char* line);
const char* shell_history_previous(shell_history_t* history, const char* draft);
const char* shell_history_next(shell_history_t* history);
void shell_history_reset_navigation(shell_history_t* history);
int shell_history_load(shell_history_t* history);
int shell_history_save(const shell_history_t* history);

#endif
