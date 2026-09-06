#include <shell/history.h>

#include <string.h>

bool shell_history_add(shell_history_t* history, const char* line)
{
    size_t length = 0U;
    bool nonblank = false;
    while (line[length] != '\0') {
        const unsigned char character = (unsigned char) line[length];
        if (length >= SHELL_LINE_CAPACITY - 1U || character < 0x20U || character > 0x7eU) {
            return false;
        }
        nonblank = nonblank || character != ' ';
        ++length;
    }
    if (!nonblank || (history->count > 0U && strcmp(history->entries[history->count - 1U], line) == 0)) {
        return false;
    }
    if (history->count == SHELL_HISTORY_CAPACITY) {
        memmove(history->entries[0], history->entries[1], (SHELL_HISTORY_CAPACITY - 1U) * SHELL_LINE_CAPACITY);
        --history->count;
    }
    memcpy(history->entries[history->count++], line, length + 1U);
    shell_history_reset_navigation(history);
    return true;
}

void shell_history_reset_navigation(shell_history_t* history)
{
    history->cursor   = history->count;
    history->draft[0] = '\0';
}

const char* shell_history_previous(shell_history_t* history, const char* draft)
{
    if (history->cursor == 0U) {
        return NULL;
    }
    if (history->cursor == history->count) {
        // The editor always supplies a bounded, terminated line.
        strcpy(history->draft, draft);
    }
    return history->entries[--history->cursor];
}

const char* shell_history_next(shell_history_t* history)
{
    if (history->cursor == history->count) {
        return NULL;
    }
    ++history->cursor;
    if (history->cursor == history->count) {
        return history->draft;
    }
    return history->entries[history->cursor];
}
