#include <lua_tabos/runtime.h>
#include <tabos/system.h>
#include <tabos/runtime_time.h>
#include <errno.h>
#include <string.h>
static size_t bounded_length(const char* value, size_t capacity)
{
    size_t length = 0U;
    while (length < capacity && value[length] != '\0') {
        ++length;
    }
    return length;
}
static int info(lua_State* L)
{
    tabos_system_info_t value;
    if (tabos_system_info(&value) != 0) {
        return luaL_fileresult(L, 0, NULL);
    }
    if (value.memory_total_bytes > LUA_MAXINTEGER || value.external_memory_total_bytes > LUA_MAXINTEGER) {
        errno = EOVERFLOW;
        return luaL_fileresult(L, 0, NULL);
    }
    lua_newtable(L);
#define STRING_FIELD(name)                                                          \
    lua_pushlstring(L, value.name, bounded_length(value.name, sizeof(value.name))); \
    lua_setfield(L, -2, #name)
#define NUMBER_FIELD(name)                        \
    lua_pushinteger(L, (lua_Integer) value.name); \
    lua_setfield(L, -2, #name)
    STRING_FIELD(target);
    STRING_FIELD(device);
    STRING_FIELD(display);
    NUMBER_FIELD(display_width);
    NUMBER_FIELD(display_height);
    NUMBER_FIELD(cpu_cores);
    NUMBER_FIELD(cpu_frequency_mhz);
    NUMBER_FIELD(memory_total_bytes);
    NUMBER_FIELD(external_memory_total_bytes);
#undef STRING_FIELD
#undef NUMBER_FIELD
    return 1;
}
static int monotonic(lua_State* L)
{
    uint64_t value = tabos_monotonic_ms();
    if (value > LUA_MAXINTEGER) {
        errno = EOVERFLOW;
        return luaL_fileresult(L, 0, NULL);
    }
    lua_pushinteger(L, (lua_Integer) value);
    return 1;
}
static int sleep_ms(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TNUMBER);
    lua_Integer duration = luaL_checkinteger(L, 1);
    luaL_argcheck(L, duration >= 0 && (lua_Unsigned) duration <= UINT32_MAX, 1, "duration out of range");
    // Slice the SDK's yield-based sleep so cooperative cancellation remains usable.
    while (duration > 0) {
        uint32_t slice = duration > 20 ? 20U : (uint32_t) duration;
        if (tabos_sleep_ms(slice) != 0) {
            return luaL_fileresult(L, 0, NULL);
        }
        duration -= slice;
        lua_tabos_console_poll(lua_tabos_runtime(L));
        lua_tabos_hook(L, NULL);
    }
    lua_pushboolean(L, 1);
    return 1;
}
int lua_tabos_open_module(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {        "info",      info},
        {"monotonic_ms", monotonic},
        {    "sleep_ms",  sleep_ms},
        {          NULL,      NULL}
    };
    luaL_newlib(L, functions);
    lua_tabos_graphics_module(L);
    lua_tabos_audio_module(L);
    return 1;
}
