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
    // Bound each pass, but keep checking Ctrl-C even when typeahead is full.
    for (size_t i = 0U; i < 64U && tabos_input_poll(&event); ++i) {
        if ((event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_C &&
             (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U) ||
            (event.type == TABOS_INPUT_TEXT && event.text[0] == 3)) {
            rt->interrupted = true;
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
int lua_tabos_readline(lua_tabos_runtime_t* rt, const char* prompt)
{
    size_t length = 0U;
    bool too_long = false;
    fputs(prompt, stdout);
    fflush(stdout);
    for (;;) {
        lua_tabos_console_poll(rt);
        if (rt->interrupted) {
            rt->interrupted = false;
            rt->head = rt->count = 0U;
            rt->overflow         = false;
            fputs("^C\n", stdout);
            return -2;
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
            (event.modifiers & TABOS_MODIFIER_CONTROL) != 0U && length == 0U && !too_long) {
            fputc('\n', stdout);
            return 0;
        }
        // Cooked text contains Enter/Backspace/Tab; never echo physical duplicates.
        if (event.type != TABOS_INPUT_TEXT) {
            continue;
        }
        for (size_t i = 0U; i < TABOS_INPUT_TEXT_MAX_BYTES && event.text[i] != '\0'; ++i) {
            unsigned char c = (unsigned char) event.text[i];
            if (c == 4U && length == 0U && !too_long) {
                fputc('\n', stdout);
                return 0;
            }
            if (c == '\n' || c == '\r') {
                fputc('\n', stdout);
                rt->line[length] = '\0';
                if (too_long) {
                    return -3;
                }
                return 1;
            }
            if (c == 8U || c == 127U) {
                if (length > 0U && !too_long) {
                    --length;
                    fputc('\b', stdout);
                }
            } else if (c >= 32U || c == '\t') {
                if (length + 2U >= sizeof(rt->line)) {
                    too_long = true;
                } else if (!too_long) {
                    // Expand tabs to spaces so Backspace always removes one visible cell.
                    if (c == '\t') {
                        c = ' ';
                    }
                    rt->line[length++] = (char) c;
                    fputc(c, stdout);
                }
            }
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
