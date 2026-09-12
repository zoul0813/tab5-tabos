#include <lua_tabos/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <tabos/runtime_time.h>

void* lua_tabos_alloc(void* ud, void* ptr, size_t old_size, size_t new_size)
{
    lua_tabos_runtime_t* rt = ud;
    if (ptr == NULL) {
        old_size = 0U; // Lua passes a type tag for new objects.
    }
    if (new_size == 0U) {
        free(ptr);
        rt->used -= old_size;
        return NULL;
    }
    if (new_size > old_size &&
        (new_size - old_size > rt->limit - rt->used || (rt->fail_after != 0U && rt->allocations >= rt->fail_after))) {
        return NULL;
    }
    void* next = realloc(ptr, new_size);
    if (next == NULL) {
        // Lua requires shrinking to succeed; retaining a larger allocation is valid.
        if (ptr != NULL && new_size <= old_size) {
            rt->used -= old_size - new_size;
            return ptr;
        }
        return NULL;
    }
    ++rt->allocations;
    rt->used = rt->used - old_size + new_size;
    if (rt->used > rt->peak) {
        rt->peak = rt->used;
    }
    return next;
}
lua_tabos_runtime_t* lua_tabos_runtime(lua_State* L)
{
    void* ud = NULL;
    (void) lua_getallocf(L, &ud);
    return ud;
}
const char* lua_tabos_checkpath(lua_State* L, int index)
{
    size_t size;
    const char* value = luaL_checklstring(L, index, &size);
    luaL_argcheck(L, memchr(value, 0, size) == NULL, index, "embedded NUL in path or name");
    return value;
}
int lua_tabos_unsupported(lua_State* L)
{
    return luaL_error(L, "operation unsupported in the TabOS Lua profile");
}
void lua_tabos_hook(lua_State* L, lua_Debug* ar)
{
    (void) ar;
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    uint64_t now            = tabos_monotonic_ms();
    if (now - rt->last_service >= 10U) {
        rt->last_service = now;
        lua_tabos_console_poll(rt);
        (void) sched_yield();
    }
    if (rt->interrupted) {
        rt->interrupted = false;
        luaL_error(L, "interrupted");
    }
}
