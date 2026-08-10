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
    unlock_console();
    return true;
}

void tabos_console_release(tabos_console_session_t *session)
{
    if (session == NULL) {
        return;
    }

    lock_console();
    if (owns_console(session)) {
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
