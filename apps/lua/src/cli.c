#include <lua_tabos/runtime.h>
#include "lualib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        int argc, script;
        char** argv;
        bool interactive, version, actions;
} options_t;
static void report(lua_State* L)
{
    (void) lua_tabos_audio_close(lua_tabos_runtime(L));
    (void) lua_tabos_graphics_close(lua_tabos_runtime(L));
    const char* message = lua_tostring(L, -1);
    fprintf(stderr, "lua: %s\n", message == NULL ? "error object is not a string" : message);
    lua_pop(L, 1);
}
static int traceback(lua_State* L)
{
    const char* message = lua_tostring(L, 1);
    if (message == NULL) {
        message = "error object is not a string";
    }
    luaL_traceback(L, L, message, 1);
    return 1;
}
static int call(lua_State* L, int nargs, int results)
{
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    int status = lua_pcall(L, nargs, results, base);
    lua_remove(L, base);
    return status;
}
static int parse(options_t* o)
{
    for (int i = 1; i < o->argc; ++i) {
        const char* arg = o->argv[i];
        if (strcmp(arg, "--") == 0) {
            o->script = i + 1 < o->argc ? i + 1 : 0;
            return 0;
        }
        if (*arg != '-') {
            o->script = i;
            return 0;
        }
        if (strcmp(arg, "-i") == 0) {
            o->interactive = o->version = true;
        } else if (strcmp(arg, "-v") == 0) {
            o->version = true;
        } else if (arg[1] == 'e' || arg[1] == 'l') {
            o->actions = true;
            if (arg[2] == '\0' && (++i >= o->argc || o->argv[i][0] == '-')) {
                fputs("lua: -e/-l requires an argument\n", stderr);
                return -1;
            }
        } else {
            fprintf(stderr, "lua: unsupported option '%s'; use --help\n", arg);
            return -1;
        }
    }
    return 0;
}
static int repl_step(lua_State* L)
{
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (lua_tabos_audio_close(rt) != 0) {
        return luaL_error(L, "audio close failed");
    }
    if (lua_tabos_graphics_close(rt) != 0) {
        return luaL_error(L, "graphics close failed");
    }
    int result = lua_tabos_readline(rt, "> ");
    if (result == 0 || result == -1) {
        if (result == -1) {
            rt->exit_status = 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }
    if (result < 0) {
        if (result == -3) {
            return luaL_error(L, "input overflow; line discarded");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    size_t length = strlen(rt->line);
    memcpy(rt->chunk, rt->line, length + 1U);
    lua_pushfstring(L, "return %s", rt->chunk[0] == '=' ? rt->chunk + 1 : rt->chunk);
    int status = luaL_loadbufferx(L, lua_tostring(L, -1), lua_rawlen(L, -1), "=stdin", "t");
    lua_remove(L, -2);
    if (status != LUA_OK) {
        lua_pop(L, 1);
        for (;;) {
            status = luaL_loadbufferx(L, rt->chunk, length, "=stdin", "t");
            if (status != LUA_ERRSYNTAX) {
                break;
            }
            const char* message = lua_tostring(L, -1);
            size_t size         = message == NULL ? 0U : strlen(message);
            if (size < 5U || strcmp(message + size - 5U, "<eof>") != 0) {
                break;
            }
            lua_pop(L, 1);
            result = lua_tabos_readline(rt, ">> ");
            if (result == 0 || result == -1) {
                rt->exit_status = result == -1 ? 1 : 0;
                lua_pushboolean(L, 0);
                return 1;
            }
            if (result == -4) {
                lua_pushboolean(L, 1);
                return 1;
            }
            if (result != 1) {
                return luaL_error(L, "incomplete input discarded");
            }
            size_t extra = strlen(rt->line);
            if (length + extra + 2U > sizeof(rt->chunk)) {
                return luaL_error(L, "input overflow; chunk discarded");
            }
            rt->chunk[length++] = '\n';
            memcpy(rt->chunk + length, rt->line, extra + 1U);
            length += extra;
        }
    }
    if (status != LUA_OK || call(L, 0, LUA_MULTRET) != LUA_OK) {
        return lua_error(L);
    }
    int count = lua_gettop(L);
    if (count != 0) {
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        lua_call(L, count, 0);
    }
    lua_pushboolean(L, 1);
    return 1;
}
static int run(lua_State* L)
{
    options_t* o = lua_touserdata(L, 1);
    lua_settop(L, 0);
    lua_tabos_libraries(L);
    lua_sethook(L, lua_tabos_hook, LUA_MASKCOUNT, 1000);
    // New Lua threads inherit the hook and allocator; debug library is absent.
    int base = o->script;
    lua_createtable(L, o->argc, 0);
    for (int i = 0; i < o->argc; ++i) {
        lua_pushstring(L, o->argv[i]);
        lua_rawseti(L, -2, i - base);
    }
    lua_setglobal(L, "arg");
    int end = o->script != 0 ? o->script : o->argc;
    for (int i = 1; i < end; ++i) {
        const char* arg = o->argv[i];
        if (arg[0] != '-' || (arg[1] != 'e' && arg[1] != 'l')) {
            continue;
        }
        const char* value = arg[2] != '\0' ? arg + 2 : o->argv[++i];
        if (arg[1] == 'e') {
            if (luaL_loadbufferx(L, value, strlen(value), "=(command line)", "t") != LUA_OK ||
                call(L, 0, 0) != LUA_OK) {
                return lua_error(L);
            }
        } else {
            const char* equal  = strchr(value, '=');
            const char* module = equal == NULL ? value : equal + 1;
            lua_getglobal(L, "require");
            lua_pushstring(L, module);
            lua_call(L, 1, 1);
            if (equal != NULL) {
                lua_pushlstring(L, value, (size_t) (equal - value));
            } else {
                lua_pushstring(L, value);
            }
            lua_pushglobaltable(L);
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -4);
            lua_settable(L, -3);
            lua_pop(L, 3);
        }
    }
    if (o->script != 0) {
        const char* filename = o->argv[o->script];
        if (strcmp(filename, "-") == 0) {
            return luaL_error(L, "stdin scripts are unsupported");
        }
        if (luaL_loadfilex(L, filename, "t") != LUA_OK) {
            return lua_error(L);
        }
        int nargs = o->argc - o->script - 1;
        luaL_checkstack(L, nargs, "too many script arguments");
        for (int i = o->script + 1; i < o->argc; ++i) {
            lua_pushstring(L, o->argv[i]);
        }
        if (call(L, nargs, 0) != LUA_OK) {
            return lua_error(L);
        }
    }
    if (o->interactive || (o->script == 0 && !o->actions && !o->version)) {
        if (!o->version) {
            puts(LUA_COPYRIGHT);
        }
        lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
        rt->interactive_session = true;
        for (;;) {
            lua_settop(L, 0);
            lua_pushcfunction(L, repl_step);
            int result = lua_pcall(L, 0, 1, 0);
            if (rt->exit_requested) {
                break;
            }
            if (result != LUA_OK) {
                report(L);
                lua_gc(L, LUA_GCCOLLECT);
                continue;
            }
            if (!lua_toboolean(L, -1)) {
                break;
            }
        }
    }
    if (lua_tabos_runtime(L)->exit_status != 0) {
        return luaL_error(L, "keyboard wait failed");
    }
    return 0;
}
int lua_tabos_main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: lua [-v] [-i] [-e code] [-l [global=]module] [--] [script [args]]\n"
             "No arguments: REPL. Ctrl-C/Ctrl-D exit; Ctrl-U cancels input.\n"
             "Source only; no stdin scripts, pipes, native modules, or host environment.\n"
             "64-bit integers/doubles; 3 MiB Lua heap; 4094-byte console lines/chunks.\n"
             "See docs/lua.md for the TabOS library profile.");
        return 0;
    }
    options_t options = {.argc = argc, .argv = argv};
    if (parse(&options) != 0) {
        return 1;
    }
    if (options.version) {
        puts(LUA_COPYRIGHT);
    }
    lua_tabos_runtime_t* rt = calloc(1U, sizeof(*rt));
    if (rt == NULL) {
        fputs("lua: not enough memory\n", stderr);
        return 1;
    }
    rt->limit    = LUA_TABOS_MEMORY_LIMIT;
    int status   = 1;
    lua_State* L = lua_newstate(lua_tabos_alloc, rt, 0U);
    if (L == NULL) {
        fputs("lua: not enough memory\n", stderr);
        goto cleanup;
    }
    if (!lua_tabos_console_open(rt)) {
        fputs("lua: console initialization failed\n", stderr);
        goto cleanup;
    }
    lua_pushcfunction(L, run);
    lua_pushlightuserdata(L, &options);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        report(L);
    } else {
        status = 0;
    }
cleanup:
    if (L != NULL) {
        rt->closing = true;
        (void) lua_tabos_audio_close(rt);
        (void) lua_tabos_graphics_close(rt);
        lua_close(L);
        if (lua_tabos_audio_close(rt) != 0) {
            status = 1;
        }
        if (lua_tabos_graphics_close(rt) != 0) {
            status = 1;
        }
    }
    if (lua_tabos_console_close(rt) != 0) {
        status = 1;
    }
    free(rt);
    return status;
}
