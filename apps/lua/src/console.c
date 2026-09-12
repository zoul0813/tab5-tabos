#include <lua_tabos/runtime.h>
#include <tabos/tty.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>

bool lua_tabos_console_open(lua_tabos_runtime_t* rt)
{
    if (ioctl(0, TABOS_TTY_GET_MODE, &rt->inherited_mode) != 0) {
        return false;
    }
    if (ioctl(0, TABOS_TTY_SET_MODE,
              rt->inherited_mode & ~(uint32_t) (TABOS_TTY_MODE_RAW_INPUT | TABOS_TTY_MODE_SCROLL_KEYS)) != 0) {
        return false;
    }
    rt->mode_changed = true;
    rt->source       = tabos_input_wait_source();
    return rt->source != TABOS_WAIT_SOURCE_INVALID;
}
int lua_tabos_console_close(lua_tabos_runtime_t* rt)
{
    if (rt->mode_changed) {
        rt->mode_changed = false;
        return ioctl(0, TABOS_TTY_SET_MODE, rt->inherited_mode);
    }
    return 0;
}
void lua_tabos_console_poll(lua_tabos_runtime_t* rt)
{
    tabos_input_event_t event;
    // Control commands remain detectable even when typeahead is full.
    for (size_t i = 0U; i < 64U && tabos_input_poll(&event); ++i) {
        bool control = event.type == TABOS_INPUT_KEY_DOWN && (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U;
        bool interrupt =
            (control && event.key == TABOS_KEY_C) || (event.type == TABOS_INPUT_TEXT && event.text[0] == 3);
        bool eof = (control && event.key == TABOS_KEY_D) || (event.type == TABOS_INPUT_TEXT && event.text[0] == 4);
        if (interrupt || (rt->interactive_session && eof)) {
            rt->interrupted = true;
            if (rt->interactive_session) {
                rt->exit_requested = true;
            }
        } else if ((control && event.key == TABOS_KEY_U) || (event.type == TABOS_INPUT_TEXT && event.text[0] == 21)) {
            rt->cancelled = true;
        } else if (event.type != TABOS_INPUT_KEY_UP) {
            if (rt->count == LUA_TABOS_QUEUE_SIZE) {
                rt->overflow = true; // Drop newest; reject pending line visibly.
            } else {
                rt->events[(rt->head + rt->count) % LUA_TABOS_QUEUE_SIZE] = event;
                ++rt->count;
            }
        }
    }
}
// Keep the editable REPL line on one screen row, including at maximum length.
static void redraw(lua_tabos_runtime_t* rt, const char* prompt, size_t length, size_t cursor, size_t* start)
{
    tabos_tty_size_t size = {.columns = 80U};
    (void) ioctl(0, TABOS_TTY_GET_SIZE, &size);
    size_t prefix = strlen(prompt);
    size_t width  = size.columns > prefix + 1U ? size.columns - prefix - 1U : 1U;
    if (cursor < *start) {
        *start = cursor;
    } else if (cursor - *start >= width) {
        *start = cursor - width + 1U;
    }
    size_t visible = length - *start;
    if (visible > width) {
        visible = width;
    }
    fputc('\r', stdout);
    fputs(prompt, stdout);
    fwrite(rt->line + *start, 1U, visible, stdout);
    fputs("\x1b[K", stdout);
    size_t back = visible - (cursor - *start);
    if (back != 0U) {
        fprintf(stdout, "\x1b[%zuD", back);
    }
}
static void remember(lua_tabos_runtime_t* rt, size_t length)
{
    if (length == 0U || (rt->history_count != 0U && strcmp(rt->history[rt->history_count - 1U], rt->line) == 0)) {
        return;
    }
    if (rt->history_count == LUA_TABOS_HISTORY_SIZE) {
        memmove(rt->history, rt->history + 1U, (LUA_TABOS_HISTORY_SIZE - 1U) * sizeof(rt->history[0]));
        --rt->history_count;
    }
    memcpy(rt->history[rt->history_count++], rt->line, length + 1U);
}
int lua_tabos_readline(lua_tabos_runtime_t* rt, const char* prompt)
{
    size_t length = 0U, cursor = 0U, start = 0U;
    size_t history = rt->history_count;
    bool repl      = prompt[0] != '\0';
    bool too_long  = false;
    rt->line[0]    = '\0';
    fputs(prompt, stdout);
    fflush(stdout);
    for (;;) {
        lua_tabos_console_poll(rt);
        if (rt->interrupted) {
            rt->interrupted = false;
            rt->head = rt->count = 0U;
            rt->overflow         = false;
            fputc('\n', stdout);
            return repl ? 0 : -2;
        }
        if (rt->cancelled) {
            rt->cancelled = false;
            rt->head = rt->count = 0U;
            rt->overflow         = false;
            rt->line[0]          = '\0';
            fputc('\n', stdout);
            if (repl) {
                return -4;
            }
            length = cursor = start = 0U;
            too_long                = false;
        }
        if (rt->overflow) {
            too_long     = true;
            rt->overflow = false;
        }
        if (rt->count == 0U) {
            tabos_wait_item_t item = {.source = rt->source, .events = TABOS_WAIT_READABLE};
            if (tabos_wait(&item, 1U, TABOS_WAIT_TIMEOUT_INFINITE) < 0 ||
                (item.returned_events & (TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP)) != 0U) {
                return -1;
            }
            continue;
        }
        tabos_input_event_t event = rt->events[rt->head];
        rt->head                  = (rt->head + 1U) % LUA_TABOS_QUEUE_SIZE;
        --rt->count;
        if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_D &&
            (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U && (repl || (length == 0U && !too_long))) {
            fputc('\n', stdout);
            return 0;
        }
        if (event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_U &&
            (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
            rt->cancelled = true;
            continue;
        }
        if (event.type == TABOS_INPUT_KEY_DOWN) {
            if (event.key == TABOS_KEY_BACKSPACE) {
                event.type = TABOS_INPUT_TEXT;
                strcpy(event.text, "\b");
            } else if (repl && !too_long) {
                switch (event.key) {
                    case TABOS_KEY_LEFT:
                        if (cursor > 0U) {
                            --cursor;
                        }
                        break;
                    case TABOS_KEY_RIGHT:
                        if (cursor < length) {
                            ++cursor;
                        }
                        break;
                    case TABOS_KEY_HOME: cursor = 0U; break;
                    case TABOS_KEY_END: cursor = length; break;
                    case TABOS_KEY_DELETE:
                        if (cursor < length) {
                            memmove(rt->line + cursor, rt->line + cursor + 1U, length - cursor);
                            --length;
                        }
                        break;
                    case TABOS_KEY_UP:
                    case TABOS_KEY_DOWN:
                        if (history == rt->history_count) {
                            memcpy(rt->draft, rt->line, length + 1U);
                        }
                        if (event.key == TABOS_KEY_UP && history > 0U) {
                            --history;
                        } else if (event.key == TABOS_KEY_DOWN && history < rt->history_count) {
                            ++history;
                        }
                        strcpy(rt->line, history == rt->history_count ? rt->draft : rt->history[history]);
                        length = cursor = strlen(rt->line);
                        start           = 0U;
                        break;
                    default: continue;
                }
                redraw(rt, prompt, length, cursor, &start);
                fflush(stdout);
                continue;
            }
        }
        // Enter/Tab arrive as cooked text; Backspace arrives as a physical key.
        if (event.type != TABOS_INPUT_TEXT) {
            continue;
        }
        for (size_t i = 0U; i < TABOS_INPUT_TEXT_MAX_BYTES && event.text[i] != '\0'; ++i) {
            unsigned char c = (unsigned char) event.text[i];
            if (c == 4U && (repl || (length == 0U && !too_long))) {
                fputc('\n', stdout);
                return 0;
            }
            if (c == 21U) {
                rt->cancelled = true;
                break;
            }
            if (c == '\n' || c == '\r') {
                if (repl) {
                    redraw(rt, prompt, length, cursor, &start);
                }
                fputc('\n', stdout);
                rt->line[length] = '\0';
                if (too_long) {
                    return -3;
                }
                if (repl) {
                    remember(rt, length);
                }
                return 1;
            }
            if (c == 8U || c == 127U) {
                if (cursor > 0U && !too_long) {
                    --cursor;
                    memmove(rt->line + cursor, rt->line + cursor + 1U, length - cursor);
                    --length;
                    if (!repl) {
                        fputc('\b', stdout);
                    }
                }
            } else if (c >= 32U || c == '\t') {
                if (length + 2U >= sizeof(rt->line)) {
                    too_long = true;
                } else if (!too_long) {
                    // Expand tabs to spaces so Backspace always removes one visible cell.
                    if (c == '\t') {
                        c = ' ';
                    }
                    memmove(rt->line + cursor + 1U, rt->line + cursor, length - cursor + 1U);
                    rt->line[cursor++] = (char) c;
                    ++length;
                    if (!repl) {
                        fputc(c, stdout);
                    }
                }
            }
        }
        if (repl) {
            redraw(rt, prompt, length, cursor, &start);
        }
        fflush(stdout);
    }
}
int lua_tabos_console_read(lua_State* L, int first)
{
    int nargs = lua_gettop(L) - 1;
    int count = nargs == 0 ? 1 : nargs;
    // Validate every format before consuming any input.
    for (int i = 0; i < nargs; ++i) {
        const char* format = luaL_checkstring(L, first + i);
        if (strcmp(format, "l") != 0 && strcmp(format, "*l") != 0 && strcmp(format, "L") != 0 &&
            strcmp(format, "*L") != 0) {
            return luaL_error(L, "console reads support only l and L line formats");
        }
    }
    luaL_checkstack(L, count, "too many console formats");
    for (int i = 0; i < count; ++i) {
        bool newline = false;
        if (nargs != 0) {
            const char* format = lua_tostring(L, first + i);
            newline            = strchr(format, 'L') != NULL;
        }
        lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
        int result              = lua_tabos_readline(rt, "");
        if (result < 0) {
            const char* message = "keyboard wait failed";
            if (result == -2) {
                message = "interrupted";
            } else if (result == -3) {
                message = "input overflow; line discarded";
            }
            return luaL_error(L, "%s", message);
        }
        if (result == 0) {
            lua_pushnil(L);
            return i + 1;
        }
        size_t size = strlen(rt->line);
        if (newline) {
            rt->line[size++] = '\n';
        }
        lua_pushlstring(L, rt->line, size);
    }
    return count;
}
