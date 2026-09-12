#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <lua_tabos/runtime.h>
#include <tabos/tty.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void test_lua_input(const char* text);
void test_lua_reader(lua_tabos_runtime_t* rt);
void test_lua_interrupt(void);
void test_lua_typeahead(size_t count);
uint32_t test_lua_mode(void);
static int initialize(lua_State* L)
{
    lua_tabos_libraries(L);
    return 0;
}
static void execute(lua_State* L, const char* script)
{
    int result = luaL_loadstring(L, script);
    if (result == LUA_OK) {
        result = lua_pcall(L, 0, 0, 0);
    }
    if (result != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
    }
    assert(result == LUA_OK);
}
int main(void)
{
    lua_tabos_runtime_t* rt = calloc(1U, sizeof(*rt));
    assert(rt != NULL);
    rt->limit = LUA_TABOS_MEMORY_LIMIT;
    // Allocator contract: type-tag osize, failed growth, shrink and free.
    void* p = lua_tabos_alloc(rt, NULL, 99U, 100U);
    assert(p != NULL && rt->used == 100U);
    assert(lua_tabos_alloc(rt, p, 100U, rt->limit + 1U) == NULL && rt->used == 100U);
    p = lua_tabos_alloc(rt, p, 100U, 20U);
    assert(p != NULL && rt->used == 20U);
    assert(lua_tabos_alloc(rt, p, 20U, 0U) == NULL && rt->used == 0U);
    // Fail library registration at many different allocation sites, then close safely.
    for (size_t failure = 1U; failure < 350U; ++failure) {
        rt->allocations = 0U;
        rt->fail_after  = failure;
        lua_State* L    = lua_newstate(lua_tabos_alloc, rt, 0U);
        if (L != NULL) {
            lua_pushcfunction(L, initialize);
            (void) lua_pcall(L, 0, 0, 0);
            lua_close(L);
        }
        assert(rt->used == 0U);
    }
    rt->fail_after = 0U;
    lua_State* L   = lua_newstate(lua_tabos_alloc, rt, 0U);
    assert(L != NULL);
    lua_pushcfunction(L, initialize);
    assert(lua_pcall(L, 0, 0, 0) == LUA_OK);
    lua_sethook(L, lua_tabos_hook, LUA_MASKCOUNT, 1000);
    assert(lua_tabos_console_open(rt));
    assert(test_lua_mode() == 0U);
    test_lua_reader(rt);
    test_lua_typeahead(1U);
    lua_tabos_console_poll(rt);
    test_lua_input("\n");
    assert(lua_tabos_readline(rt, "") == 1 && strcmp(rt->line, "x") == 0);
    test_lua_typeahead(LUA_TABOS_QUEUE_SIZE + 16U);
    lua_tabos_console_poll(rt);
    lua_tabos_console_poll(rt);
    lua_tabos_console_poll(rt);
    assert(rt->count == LUA_TABOS_QUEUE_SIZE && rt->overflow);
    test_lua_interrupt();
    lua_tabos_console_poll(rt);
    assert(rt->interrupted); // Ctrl-C survives full typeahead.
    assert(lua_tabos_readline(rt, "") == -2 && rt->count == 0U);
    rt->overflow = true;
    test_lua_input("\n");
    assert(lua_tabos_readline(rt, "") == -3);
    test_lua_input("ab\bC\n");
    assert(lua_tabos_readline(rt, "") == 1 && strcmp(rt->line, "aC") == 0);
    test_lua_input("\3");
    assert(lua_tabos_readline(rt, "") == -2);
    test_lua_input("\4");
    assert(lua_tabos_readline(rt, "") == 0);
    char long_line[LUA_TABOS_LINE_SIZE + 10U];
    memset(long_line, 'a', sizeof(long_line));
    long_line[sizeof(long_line) - 2U] = '\n';
    long_line[sizeof(long_line) - 1U] = 0;
    test_lua_input(long_line);
    assert(lua_tabos_readline(rt, "") == -3);
    test_lua_input("one\ntwo\nthree\n");
    execute(L, "assert(io.read() == 'one'); assert(io.stdin:read('L') == 'two\\n'); "
               "assert(io.lines()() == 'three'); assert(not pcall(io.read, 'a'))");
    execute(L, "assert(math.maxinteger == 9223372036854775807); assert(math.mininteger == -9223372036854775807-1); "
               "assert(math.maxinteger+1 == math.mininteger); assert(math.abs(math.sin(math.pi/2)-1)<1e-12); "
               "assert(string.dump == nil and debug == nil and #package.searchers == 2); "
               "assert(load('return 2')()==2); assert(not load(string.char(27)..'Lua','x','b')); "
               "assert(not load(function() return string.char(27)..'Lua' end)); "
               "assert(not pcall(io.open, 'a'..string.char(0)..'b')); "
               "assert(not pcall(loadfile, 'a'..string.char(0)..'b')); assert(not loadfile()); "
               "assert(not pcall(os.clock)); assert(not pcall(os.execute)); assert(not pcall(io.tmpfile)); "
               "assert(os.getenv('HOME') == nil and package.cpath == ''); "
               "assert(os.date('!%Y-%m-%d',0)=='1970-01-01'); "
               "local t={year=2024,month=2,day=29,hour=0}; assert(os.time(t)==1709164800 and t.yday==60); "
               "assert(not pcall(os.time,{year=2023,month=2,day=29})); assert(not pcall(os.date,'%Q')); "
               "assert(os.date('%z %Z',0)=='+0000 UTC'); assert(not pcall(os.date,'*t',-2)); "
               "assert(require('tabos').info().cpu_cores==2); assert(require('tabos').sleep_ms(0)); "
               "assert(not pcall(require('tabos').sleep_ms, -1)); assert(not pcall(require('tabos').sleep_ms, 0/0)); "
               "local ok=pcall(function() local function f() return 1+f() end; f() end); assert(not ok); "
               "local ok=pcall(string.rep,'x',4000000); assert(not ok); collectgarbage(); assert(1+2==3)");
    execute(L, "local f,e=load('return '..string.rep('(',100)..'1'..string.rep(')',100)); "
               "assert(f==nil and e:find('C stack overflow')); "
               "local ok,e=pcall(string.match,string.rep('a',100),string.rep('a?',100)); "
               "assert(not ok and e:find('pattern too complex')); "
               "local function counter() local n=0; return function() n=n+1; return n end end; "
               "local c=counter(); assert(c()==1 and c()==2); "
               "local t=setmetatable({}, {__index=function(_,k) return k end}); assert(t.hello=='hello')");
    char temp[] = "/tmp/tabos-lua-native-XXXXXX";
    assert(mkdtemp(temp) != NULL);
    char cwd[4096];
    assert(getcwd(cwd, sizeof(cwd)) != NULL && chdir(temp) == 0);
    execute(
        L, "local f=assert(io.open('bytes','wb')); assert(f:write('a'..string.char(0,255)..'z')); "
           "assert(f:flush()); assert(f:close()); f=assert(io.open('bytes','rb')); "
           "assert(f:read('a')=='a'..string.char(0,255)..'z'); assert(f:seek('set',1)==1); "
           "assert(f:read(1)==string.char(0)); assert(f:close()); assert(os.rename('bytes','renamed')); "
           "assert(os.remove('renamed')); local a,b,c=io.open('absent'); assert(a==nil and type(b)=='string' and "
           "type(c)=='number'); "
           "f=assert(io.open('mod.lua','w')); f:write('return {value=42}'); f:close(); "
           "assert(require('mod').value==42 and require('mod')==require('mod')); "
           "assert(os.remove('mod.lua')); package.path='a'..string.char(0)..'b'; assert(not pcall(require,'missing'))");
    assert(chdir(cwd) == 0 && rmdir(temp) == 0);
    test_lua_interrupt();
    assert(luaL_loadstring(L, "while true do end") == LUA_OK);
    assert(lua_pcall(L, 0, 0, 0) != LUA_OK);
    lua_settop(L, 0);
    test_lua_interrupt();
    execute(L, "local co=coroutine.create(function() while true do end end); local ok,e=coroutine.resume(co); "
               "assert(not ok and e:find('interrupted'))");
    lua_close(L);
    assert(rt->used == 0U);
    assert(lua_tabos_console_close(rt) == 0 && test_lua_mode() == TABOS_TTY_MODE_SCROLL_KEYS);
    free(rt);
    puts("Lua profile, allocation failure, console, files, dates, modules and coroutine interruption passed");
    return 0;
}
