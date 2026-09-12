#ifndef LUA_TABOS_RUNTIME_H
#define LUA_TABOS_RUNTIME_H
#include "lua.h"
#include "lauxlib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tabos/input.h>
#include <tabos/wait.h>
#define LUA_TABOS_MEMORY_LIMIT (3U * 1024U * 1024U)
#define LUA_TABOS_LINE_SIZE    4096U
#define LUA_TABOS_QUEUE_SIZE   128U
typedef struct {
        size_t used, peak, limit, allocations, fail_after;
        uint64_t last_service;
        uint32_t inherited_mode;
        tabos_wait_source_t source;
        tabos_input_event_t events[LUA_TABOS_QUEUE_SIZE];
        size_t head, count;
        bool overflow, interrupted, mode_changed, closing;
        int exit_status;
        char line[LUA_TABOS_LINE_SIZE];
        char chunk[LUA_TABOS_LINE_SIZE];
} lua_tabos_runtime_t;
void* lua_tabos_alloc(void* ud, void* ptr, size_t old_size, size_t new_size);
lua_tabos_runtime_t* lua_tabos_runtime(lua_State* L);
const char* lua_tabos_checkpath(lua_State* L, int index);
int lua_tabos_unsupported(lua_State* L);
void lua_tabos_libraries(lua_State* L);
int lua_tabos_open_os(lua_State* L);
int lua_tabos_open_module(lua_State* L);
bool lua_tabos_console_open(lua_tabos_runtime_t* rt);
int lua_tabos_console_close(lua_tabos_runtime_t* rt);
void lua_tabos_console_poll(lua_tabos_runtime_t* rt);
int lua_tabos_readline(lua_tabos_runtime_t* rt, const char* prompt);
int lua_tabos_console_read(lua_State* L, int first);
void lua_tabos_hook(lua_State* L, lua_Debug* ar);
int lua_tabos_main(int argc, char** argv);
#endif
