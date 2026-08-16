#include <tabos/console.h>

#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/time.h>

#include <tabos/config/console.h>

#include <tabos/platform/platform.h>

#include <stdatomic.h>

static terminal_t *active_terminal;
static uint32_t foreground_token;
static uint32_t next_token = 1U;
static atomic_flag console_lock = ATOMIC_FLAG_INIT;
static tabos_timer_t cursor_timer;
static bool cursor_phase_visible = true;

static bool restart_cursor_blink(void)
{
    const bool changed = !cursor_phase_visible;
    cursor_phase_visible = true;
    if (active_terminal != NULL) {
        terminal_set_cursor_phase(active_terminal, true);
    }
    tabos_timer_start(&cursor_timer, TABOS_CURSOR_BLINK_INTERVAL_MS,
                      TABOS_CURSOR_BLINK_INTERVAL_MS);
    return changed;
}

static void lock_console(void)
{
    while (atomic_flag_test_and_set_explicit(&console_lock, memory_order_acquire)) {
        platform_input_wait();
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

void console_init(terminal_t *terminal)
{
    lock_console();
    active_terminal = terminal;
    foreground_token = 0U;
    next_token = 1U;
    tabos_timer_cancel(&cursor_timer);
    cursor_phase_visible = true;
    unlock_console();
}

void console_rebind(terminal_t *terminal)
{
    lock_console();
    active_terminal = terminal;
    if (foreground_token != 0U) {
        restart_cursor_blink();
    }
    unlock_console();
}

void console_shutdown(void)
{
    lock_console();
    active_terminal = NULL;
    foreground_token = 0U;
    tabos_timer_cancel(&cursor_timer);
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
    terminal_set_cursor_visible(active_terminal, true);
    restart_cursor_blink();
    const bool presented = display_present();
    if (!presented) {
        terminal_set_cursor_visible(active_terminal, false);
        foreground_token = 0U;
        session->token = 0U;
        tabos_timer_cancel(&cursor_timer);
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
        terminal_set_cursor_visible(active_terminal, false);
        tabos_timer_cancel(&cursor_timer);
        (void)display_present();
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
    terminal_write(active_terminal, text);
    restart_cursor_blink();
    const bool presented = display_present();
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
    terminal_write_line(active_terminal, text);
    restart_cursor_blink();
    const bool presented = display_present();
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
    terminal_clear(active_terminal);
    restart_cursor_blink();
    const bool presented = display_present();
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
                            bool (*operation)(terminal_t *terminal))
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL || !operation(active_terminal)) {
        unlock_console();
        return false;
    }
    const bool presented = display_present();
    unlock_console();
    return presented;
}

bool tabos_console_scroll_lines(const tabos_console_session_t *session, int lines)
{
    lock_console();
    if (!owns_console(session) || active_terminal == NULL ||
        !terminal_scroll_lines(active_terminal, lines)) {
        unlock_console();
        return false;
    }
    const bool presented = display_present();
    unlock_console();
    return presented;
}

bool tabos_console_page_up(const tabos_console_session_t *session)
{
    return update_viewport(session, terminal_page_up);
}

bool tabos_console_page_down(const tabos_console_session_t *session)
{
    return update_viewport(session, terminal_page_down);
}

bool tabos_console_scroll_to_start(const tabos_console_session_t *session)
{
    return update_viewport(session, terminal_scroll_to_start);
}

bool tabos_console_scroll_to_end(const tabos_console_session_t *session)
{
    return update_viewport(session, terminal_scroll_to_end);
}

bool tabos_console_is_at_end(const tabos_console_session_t *session)
{
    lock_console();
    const bool at_end = owns_console(session) && active_terminal != NULL &&
        terminal_is_at_end(active_terminal);
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
    *line_count = terminal_history_line_count(active_terminal);
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
    if (received) {
        if (restart_cursor_blink()) {
            (void)display_present();
        }
    }
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
            if (restart_cursor_blink()) {
                (void)display_present();
            }
            unlock_console();
            return true;
        }
        unlock_console();
        platform_input_wait();
    }
}

void console_update(void)
{
    lock_console();
    if (active_terminal != NULL && foreground_token != 0U &&
        tabos_timer_poll(&cursor_timer)) {
        cursor_phase_visible = !cursor_phase_visible;
        terminal_set_cursor_phase(active_terminal, cursor_phase_visible);
        (void)display_present();
    }
    unlock_console();
}
