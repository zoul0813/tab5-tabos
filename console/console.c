#include <tabos/console.h>

#include <tabos/internal/console.h>
#include <tabos/internal/display.h>

#include <stdatomic.h>

static tab_terminal_t *active_terminal;
static uint32_t foreground_token;
static uint32_t next_token = 1U;
static atomic_flag console_lock = ATOMIC_FLAG_INIT;

static void lock_console(void)
{
    while (atomic_flag_test_and_set_explicit(&console_lock, memory_order_acquire)) {
    }
}

static void unlock_console(void)
{
    atomic_flag_clear_explicit(&console_lock, memory_order_release);
}

static bool owns_console(const tabos_console_session_t *session)
{
    return session != NULL && session->token != 0U && session->token == foreground_token;
}

void tab_console_init(tab_terminal_t *terminal)
{
    lock_console();
    active_terminal = terminal;
    foreground_token = 0U;
    next_token = 1U;
    unlock_console();
}

void tab_console_rebind(tab_terminal_t *terminal)
{
    lock_console();
    active_terminal = terminal;
    unlock_console();
}

void tab_console_shutdown(void)
{
    lock_console();
    active_terminal = NULL;
    foreground_token = 0U;
    unlock_console();
}

bool tabos_console_acquire(tabos_console_session_t *session)
{
    if (session == NULL) {
        return false;
    }

    lock_console();
    if (active_terminal == NULL || foreground_token != 0U) {
        unlock_console();
        session->token = 0U;
        return false;
    }
    if (next_token == 0U) {
        ++next_token;
    }
    foreground_token = next_token++;
    session->token = foreground_token;
    tab_terminal_set_cursor_visible(active_terminal, true);
    const bool presented = tab_display_present();
    if (!presented) {
        tab_terminal_set_cursor_visible(active_terminal, false);
        foreground_token = 0U;
        session->token = 0U;
    }
    unlock_console();
    return presented;
}

void tabos_console_release(tabos_console_session_t *session)
{
    if (session == NULL) {
        return;
    }

    lock_console();
    if (owns_console(session)) {
        tab_terminal_set_cursor_visible(active_terminal, false);
        (void)tab_display_present();
        foreground_token = 0U;
    }
    session->token = 0U;
    unlock_console();
}

bool tabos_console_is_foreground(const tabos_console_session_t *session)
{
    lock_console();
    const bool foreground = owns_console(session) && active_terminal != NULL;
    unlock_console();
    return foreground;
}

bool tabos_console_write(const tabos_console_session_t *session, const char *text)
{
    if (text == NULL) {
        return false;
    }

    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    tab_terminal_write(active_terminal, text);
    const bool presented = tab_display_present();
    unlock_console();
    return presented;
}

bool tabos_console_write_line(const tabos_console_session_t *session, const char *text)
{
    if (text == NULL) {
        return false;
    }

    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    tab_terminal_write_line(active_terminal, text);
    const bool presented = tab_display_present();
    unlock_console();
    return presented;
}

bool tabos_console_clear(const tabos_console_session_t *session)
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    tab_terminal_clear(active_terminal);
    const bool presented = tab_display_present();
    unlock_console();
    return presented;
}

bool tabos_console_get_cursor(const tabos_console_session_t *session,
                              size_t *column, size_t *row)
{
    if (column == NULL || row == NULL) {
        return false;
    }

    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    *column = active_terminal->column;
    *row = active_terminal->row;
    unlock_console();
    return true;
}

static bool update_viewport(const tabos_console_session_t *session,
                            bool (*operation)(tab_terminal_t *terminal))
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL || !operation(active_terminal)) {
        unlock_console();
        return false;
    }
    const bool presented = tab_display_present();
    unlock_console();
    return presented;
}

bool tabos_console_scroll_lines(const tabos_console_session_t *session, int lines)
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL ||
        !tab_terminal_scroll_lines(active_terminal, lines)) {
        unlock_console();
        return false;
    }
    const bool presented = tab_display_present();
    unlock_console();
    return presented;
}

bool tabos_console_page_up(const tabos_console_session_t *session)
{
    return update_viewport(session, tab_terminal_page_up);
}

bool tabos_console_page_down(const tabos_console_session_t *session)
{
    return update_viewport(session, tab_terminal_page_down);
}

bool tabos_console_scroll_to_start(const tabos_console_session_t *session)
{
    return update_viewport(session, tab_terminal_scroll_to_start);
}

bool tabos_console_scroll_to_end(const tabos_console_session_t *session)
{
    return update_viewport(session, tab_terminal_scroll_to_end);
}

bool tabos_console_is_at_end(const tabos_console_session_t *session)
{
    lock_console();
    const bool at_end = owns_console(session) && active_terminal != NULL &&
        tab_terminal_is_at_end(active_terminal);
    unlock_console();
    return at_end;
}

bool tabos_console_get_history_line_count(const tabos_console_session_t *session,
                                          size_t *line_count)
{
    if (line_count == NULL) {
        return false;
    }
    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    *line_count = tab_terminal_history_line_count(active_terminal);
    unlock_console();
    return true;
}

bool tabos_console_poll(const tabos_console_session_t *session, tabos_input_event_t *event)
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL) {
        unlock_console();
        return false;
    }
    const bool received = tabos_input_poll(event);
    unlock_console();
    return received;
}

bool tabos_console_wait(const tabos_console_session_t *session, tabos_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    for (;;) {
        lock_console();
        if (!owns_console(session) || active_terminal == NULL) {
            unlock_console();
            return false;
        }
        if (tabos_input_poll(event)) {
            unlock_console();
            return true;
        }
        unlock_console();
        tab_platform_input_wait();
    }
}
