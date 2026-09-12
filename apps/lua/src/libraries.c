#include <lua_tabos/runtime.h>
#include "lualib.h"
void lua_tabos_libraries(lua_State* L)
{
    static const luaL_Reg libraries[] = {
        {      LUA_GNAME,          luaopen_base},
        {LUA_LOADLIBNAME,       luaopen_package},
        {  LUA_COLIBNAME,     luaopen_coroutine},
        { LUA_TABLIBNAME,         luaopen_table},
        { LUA_STRLIBNAME,        luaopen_string},
        {LUA_MATHLIBNAME,          luaopen_math},
        {LUA_UTF8LIBNAME,          luaopen_utf8},
        {  LUA_IOLIBNAME,            luaopen_io},
        {  LUA_OSLIBNAME,     lua_tabos_open_os},
        {        "tabos", lua_tabos_open_module},
        {           NULL,                  NULL}
    };
    for (const luaL_Reg* entry = libraries; entry->name != NULL; ++entry) {
        luaL_requiref(L, entry->name, entry->func, 1);
        lua_pop(L, 1);
    }
}
