#include <lua_tabos/runtime.h>
#include <tabos/clock.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int remove_file(lua_State* L)
{
    const char* path = lua_tabos_checkpath(L, 1);
    return luaL_fileresult(L, remove(path) == 0, path);
}
static int rename_file(lua_State* L)
{
    const char* from = lua_tabos_checkpath(L, 1);
    const char* to   = lua_tabos_checkpath(L, 2);
    return luaL_fileresult(L, rename(from, to) == 0, NULL);
}
static int getenv_absent(lua_State* L)
{
    (void) lua_tabos_checkpath(L, 1);
    lua_pushnil(L);
    return 1;
}
static int exit_process(lua_State* L)
{
    int status = EXIT_SUCCESS;
    if (lua_isboolean(L, 1)) {
        status = lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (!lua_isnoneornil(L, 1)) {
        lua_Integer value = luaL_checkinteger(L, 1);
        luaL_argcheck(L, value >= INT_MIN && value <= INT_MAX, 1, "status out of range");
        status = (int) value;
    }
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (rt->closing) {
        return luaL_error(L, "os.exit is unavailable during state finalization");
    }
    rt->closing = true;
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* main_thread = lua_tothread(L, -1);
    lua_close(main_thread);
    (void) lua_tabos_console_close(rt);
    free(rt);
    exit(status);
    return 0;
}
static int field(lua_State* L, const char* name, int fallback, int low, int high)
{
    lua_getfield(L, 1, name);
    lua_Integer value = lua_isnil(L, -1) ? fallback : luaL_checkinteger(L, -1);
    if (value < low || value > high) {
        return luaL_error(L, "date field '%s' out of range", name);
    }
    lua_pop(L, 1);
    return (int) value;
}
static bool leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static void setfield(lua_State* L, const char* name, int value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}
static void calendar_table(lua_State* L, const struct tm* tm)
{
    setfield(L, "year", tm->tm_year + 1900);
    setfield(L, "month", tm->tm_mon + 1);
    setfield(L, "day", tm->tm_mday);
    setfield(L, "hour", tm->tm_hour);
    setfield(L, "min", tm->tm_min);
    setfield(L, "sec", tm->tm_sec);
    setfield(L, "wday", tm->tm_wday + 1);
    setfield(L, "yday", tm->tm_yday + 1);
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "isdst");
}
static lua_Integer now(lua_State* L)
{
    int64_t value;
    if (tabos_clock_get_epoch(&value) != 0) {
        return -1;
    }
    (void) L;
    return (lua_Integer) value;
}
static int os_time(lua_State* L)
{
    lua_Integer value;
    if (lua_isnoneornil(L, 1)) {
        value = now(L);
        if (value < 0) {
            return luaL_fileresult(L, 0, NULL);
        }
    } else {
        luaL_checktype(L, 1, LUA_TTABLE);
        int year                  = field(L, "year", -1, 1970, 9999);
        int month                 = field(L, "month", -1, 1, 12);
        int day                   = field(L, "day", -1, 1, 31);
        int hour                  = field(L, "hour", 12, 0, 23);
        int minute                = field(L, "min", 0, 0, 59);
        int second                = field(L, "sec", 0, 0, 59);
        static const int months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int maxday                = months[month - 1] + (month == 2 && leap(year));
        luaL_argcheck(L, day <= maxday, 1, "invalid day for month");
        int64_t days = 0;
        for (int y = 1970; y < year; ++y) {
            days += 365 + leap(y);
        }
        for (int m = 1; m < month; ++m) {
            days += months[m - 1] + (m == 2 && leap(year));
        }
        days          += day - 1;
        value          = days * 86400 + hour * 3600 + minute * 60 + second;
        time_t stamp   = (time_t) value;
        struct tm* tm  = gmtime(&stamp);
        if (tm == NULL || (lua_Integer) stamp != value) {
            return luaL_error(L, "date cannot be represented");
        }
        lua_settop(L, 1);
        calendar_table(L, tm);
    }
    lua_pushinteger(L, value);
    return 1;
}
static int os_date(lua_State* L)
{
    const char* format = lua_isnoneornil(L, 1) ? "%a %b %d %H:%M:%S %Y" : lua_tabos_checkpath(L, 1);
    lua_Integer value  = lua_isnoneornil(L, 2) ? now(L) : luaL_checkinteger(L, 2);
    if (value == -1 && lua_isnoneornil(L, 2)) {
        return luaL_fileresult(L, 0, NULL);
    }
    luaL_argcheck(L, value >= 0 && value <= 253402300799LL, 2, "date outside 1970..9999 UTC");
    time_t stamp   = (time_t) value;
    struct tm* ptr = gmtime(&stamp);
    if (ptr == NULL || (lua_Integer) stamp != value) {
        return luaL_error(L, "date cannot be represented");
    }
    struct tm tm = *ptr;
    if (*format == '!') {
        ++format;
    }
    if (strcmp(format, "*t") == 0) {
        lua_newtable(L);
        calendar_table(L, &tm);
        return 1;
    }
    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);
    while (*format != '\0') {
        if (*format != '%') {
            luaL_addchar(&buffer, *format++);
            continue;
        }
        ++format;
        if (*format == '\0' || strchr("aAbBcdHIjmMpSUwWxXyYzZ%", *format) == NULL) {
            return luaL_error(L, "unsupported UTC date format");
        }
        if (*format == 'z' || *format == 'Z') {
            luaL_addstring(&buffer, *format == 'z' ? "+0000" : "UTC");
        } else {
            char spec[3] = {'%', *format, '\0'};
            char output[128];
            size_t size = strftime(output, sizeof(output), spec, &tm);
            if (size == 0U) {
                return luaL_error(L, "UTC date formatting failed");
            }
            luaL_addlstring(&buffer, output, size);
        }
        ++format;
    }
    luaL_pushresult(&buffer);
    return 1;
}
static int difference(lua_State* L)
{
    lua_pushnumber(L, (lua_Number) luaL_checkinteger(L, 1) - (lua_Number) luaL_checkinteger(L, 2));
    return 1;
}
int lua_tabos_open_os(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {   "remove",           remove_file},
        {   "rename",           rename_file},
        {   "getenv",         getenv_absent},
        {     "exit",          exit_process},
        {     "time",               os_time},
        {     "date",               os_date},
        { "difftime",            difference},
        {    "clock", lua_tabos_unsupported},
        {  "execute", lua_tabos_unsupported},
        {  "tmpname", lua_tabos_unsupported},
        {"setlocale", lua_tabos_unsupported},
        {       NULL,                  NULL}
    };
    luaL_newlib(L, functions);
    return 1;
}
