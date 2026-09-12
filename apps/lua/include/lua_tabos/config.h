#ifndef LUA_TABOS_CONFIG_H
#define LUA_TABOS_CONFIG_H
#define LUAI_MAXCCALLS          48
#define MAXCCALLS               48
#define lua_getlocaledecpoint() ('.')
#define LUA_TABOS_PATH          "./?.lua;./?/init.lua;T:/lib/lua/5.5/?.lua;T:/lib/lua/5.5/?/init.lua"
#if defined(LUA_USE_LINUX) || defined(LUA_USE_MACOSX) || defined(LUA_USE_POSIX) || defined(LUA_USE_DLOPEN) || \
    defined(LUA_USE_READLINE) || defined(_WIN32)
#error TabOS Lua requires the generic ISO C profile
#endif
#endif
