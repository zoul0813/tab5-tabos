#ifndef TABOS_CONSOLE_H
#define TABOS_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/input.h>

typedef struct {
    uint32_t token;
} tabos_console_session_t;

/* Claim the foreground console. Only one session may own it at a time. */
bool tabos_console_acquire(tabos_console_session_t *session);
void tabos_console_release(tabos_console_session_t *session);
bool tabos_console_is_foreground(const tabos_console_session_t *session);

/* Foreground output is rendered and presented immediately. */
bool tabos_console_write(const tabos_console_session_t *session, const char *text);
bool tabos_console_write_line(const tabos_console_session_t *session, const char *text);
bool tabos_console_clear(const tabos_console_session_t *session);
bool tabos_console_get_cursor(const tabos_console_session_t *session,
                              size_t *column, size_t *row);

/* Move scrollback viewport. New output automatically returns to live end. */
bool tabos_console_scroll_lines(const tabos_console_session_t *session, int lines);
bool tabos_console_page_up(const tabos_console_session_t *session);
bool tabos_console_page_down(const tabos_console_session_t *session);
bool tabos_console_scroll_to_start(const tabos_console_session_t *session);
bool tabos_console_scroll_to_end(const tabos_console_session_t *session);
bool tabos_console_is_at_end(const tabos_console_session_t *session);
bool tabos_console_get_history_line_count(const tabos_console_session_t *session,
                                          size_t *line_count);

/* Foreground input reads normalized events from the shared input queue. */
bool tabos_console_poll(const tabos_console_session_t *session, tabos_input_event_t *event);
bool tabos_console_wait(const tabos_console_session_t *session, tabos_input_event_t *event);

#endif
