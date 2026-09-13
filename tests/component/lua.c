#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <lua_tabos/runtime.h>
#include <tabos/tty.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void test_lua_input(const char* text);
void test_lua_reader(lua_tabos_runtime_t* rt);
void test_lua_interrupt(void);
void test_lua_typeahead(size_t count);
uint32_t test_lua_mode(void);
void test_lua_key(tabos_key_t key, bool down);
void test_lua_graphics_failure(int error);
void test_lua_mode_failure(unsigned long request);
void test_lua_audio_failure(int error);
size_t test_lua_audio_open_count(void);
void test_lua_audio_bytes(const void* bytes, size_t count);
void test_lua_pointer_failure(int error);
bool test_lua_pointer_opened(void);
void test_lua_pointer_event(tabos_pointer_event_t event);
unsigned int test_lua_graphics_presents(void);
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
    const tabos_input_event_t edits[] = {
        {    .type = TABOS_INPUT_TEXT,              .text = "acX"},
        {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_BACKSPACE},
        {.type = TABOS_INPUT_KEY_DOWN,      .key = TABOS_KEY_LEFT},
        {    .type = TABOS_INPUT_TEXT,                .text = "b"},
        {.type = TABOS_INPUT_KEY_DOWN,      .key = TABOS_KEY_HOME},
        {.type = TABOS_INPUT_KEY_DOWN,    .key = TABOS_KEY_DELETE},
        {.type = TABOS_INPUT_KEY_DOWN,     .key = TABOS_KEY_RIGHT},
        {.type = TABOS_INPUT_KEY_DOWN,    .key = TABOS_KEY_DELETE},
        {.type = TABOS_INPUT_KEY_DOWN,       .key = TABOS_KEY_END},
        {    .type = TABOS_INPUT_TEXT,              .text = "d\n"},
    };
    memcpy(rt->events, edits, sizeof(edits));
    rt->head  = 0U;
    rt->count = sizeof(edits) / sizeof(edits[0]);
    assert(lua_tabos_readline(rt, "> ") == 1 && strcmp(rt->line, "bd") == 0);
    rt->events[0] = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_UP};
    rt->head      = 0U;
    rt->count     = 1U;
    test_lua_input("\n");
    assert(lua_tabos_readline(rt, "> ") == 1 && strcmp(rt->line, "bd") == 0);
    rt->events[0] = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_ESCAPE};
    rt->head      = 0U;
    rt->count     = 1U;
    test_lua_input("kept\n");
    assert(lua_tabos_readline(rt, ">> ") == 1 && strcmp(rt->line, "kept") == 0);
    test_lua_input("\3");
    assert(lua_tabos_readline(rt, "") == -2);
    test_lua_input("\4");
    assert(lua_tabos_readline(rt, "") == 0);
    test_lua_input("discard\25");
    assert(lua_tabos_readline(rt, ">> ") == -4);
    test_lua_input("discard\4");
    assert(lua_tabos_readline(rt, "> ") == 0);
    test_lua_input("discard\3");
    assert(lua_tabos_readline(rt, "> ") == 0);
    rt->overflow = true;
    rt->events[0] =
        (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_U, .modifiers = TABOS_MODIFIER_CONTROL};
    rt->head  = 0U;
    rt->count = 1U;
    assert(lua_tabos_readline(rt, "> ") == -4 && !rt->overflow && rt->count == 0U);
    char long_line[LUA_TABOS_LINE_SIZE + 10U];
    memset(long_line, 'a', sizeof(long_line));
    long_line[sizeof(long_line) - 2U] = '\n';
    long_line[sizeof(long_line) - 1U] = 0;
    test_lua_input(long_line);
    assert(lua_tabos_readline(rt, "") == -3);
    long_line[LUA_TABOS_LINE_SIZE - 2U] = '\n';
    long_line[LUA_TABOS_LINE_SIZE - 1U] = '\0';
    test_lua_input(long_line);
    assert(lua_tabos_readline(rt, "> ") == 1 && strlen(rt->line) == LUA_TABOS_LINE_SIZE - 2U);
    for (size_t i = 0U; i < LUA_TABOS_HISTORY_SIZE + 2U; ++i) {
        char entry[32];
        snprintf(entry, sizeof(entry), "%zu\n", i);
        test_lua_input(entry);
        assert(lua_tabos_readline(rt, "> ") == 1);
    }
    assert(rt->history_count == LUA_TABOS_HISTORY_SIZE && strcmp(rt->history[0], "2") == 0);
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
    execute(L, "t=require('tabos'); assert(t.rgb(255,0,0)==63488 and t.rgb(0,255,0)==2016 "
               "and t.rgb(0,0,255)==31); "
               "assert(not pcall(t.rgb,256,0,0)); assert(not pcall(t.rgb,'1',0,0)); "
               "assert(not pcall(t.graphics.open,0,10)); assert(not pcall(t.graphics.open,640,480)); "
               "assert(not pcall(t.graphics.open,0/0,10)); assert(not pcall(t.graphics.open,10.5,10)); "
               "assert(not t.graphics.open(2000,1)); "
               "s=assert(t.graphics.open(8,8)); local w,h=s:size(); assert(w==8 and h==8); "
               "local a,b,c=t.graphics.open(8,8); assert(a==nil and type(b)=='string' and type(c)=='number'); "
               "assert(s:clear(0)); assert(s:fill_rect(-1,-1,3,3,63488)); "
               "assert(s:pixel(7,7,31)); assert(s:line(0,3,7,3,2016)); "
               "assert(s:rect(4,4,3,3,65535)); "
               "assert(s:fill_rect(32767,0,1,8,123)); assert(s:fill_rect(-32768,0,1,8,123)); "
               "assert(s:rect(32767,32767,32767,32767,123)); "
               "assert(s:blit(0,5,2,1,string.char(31,0,0,248))); "
               "assert(s:set_letterbox_color(31)); assert(s:present()); "
               "assert(not pcall(s.fill_rect,s,0,0,-1,4,0)); "
               "assert(not pcall(s.line,s,math.maxinteger,0,0,0,0)); "
               "assert(not pcall(s.pixel,s,0,0,65536)); "
               "assert(not pcall(s.blit,s,0,0,2,1,'short')); "
               "assert(not pcall(s.blit,s,0,0,32767,32767,'')); "
               "assert(not pcall(io.read)); assert(not pcall(s.is_down,s,'bad')); assert(s:poll()==nil)");
    assert(rt->graphics.open && test_lua_mode() == TABOS_TTY_MODE_RAW_INPUT);
    assert(test_lua_graphics_presents() == 1U);
    const tabos_color_t* pixels = rt->graphics.pixels;
    for (size_t y = 0U; y < 8U; ++y) {
        for (size_t x = 0U; x < 8U; ++x) {
            tabos_color_t expected = 0U;
            if (x < 2U && y < 2U) {
                expected = 63488U;
            }
            if (y == 3U) {
                expected = 2016U;
            }
            if (x >= 4U && x <= 6U && y >= 4U && y <= 6U && (x != 5U || y != 5U)) {
                expected = 65535U;
            }
            if ((x == 7U && y == 7U) || (x == 0U && y == 5U)) {
                expected = 31U;
            }
            if (x == 1U && y == 5U) {
                expected = 63488U;
            }
            assert(pixels[y * 8U + x] == expected);
        }
    }
    test_lua_key(TABOS_KEY_LEFT, true);
    execute(L, "assert(s:is_down('left')); local e=s:poll(); "
               "assert(e.type=='key_down' and e.key=='left' and not e['repeat']); assert(s:poll()==nil)");
    test_lua_key(TABOS_KEY_LEFT, false);
    execute(L, "assert(not s:is_down('left')); assert(s:poll().type=='key_up')");
    // Hooks must buffer short taps including release, and held state survives overflow.
    test_lua_key(TABOS_KEY_A, true);
    lua_tabos_console_poll(rt);
    test_lua_key(TABOS_KEY_A, false);
    lua_tabos_console_poll(rt);
    execute(L, "assert(s:poll().type=='key_down'); assert(s:poll().type=='key_up')");
    for (size_t i = 0U; i < LUA_TABOS_QUEUE_SIZE + 1U; ++i) {
        test_lua_key(TABOS_KEY_A, i != LUA_TABOS_QUEUE_SIZE);
        lua_tabos_console_poll(rt);
    }
    execute(L, "assert(not s:is_down('a')); assert(s:poll().type=='overflow'); "
               "for i=1,128 do assert(s:poll().type=='key_down') end; assert(s:poll()==nil)");
    test_lua_graphics_failure(EIO);
    execute(L, "local a,b,c=s:present(); assert(a==nil and type(b)=='string' and type(c)=='number'); "
               "assert(not s:close())");
    assert(rt->graphics.open); // Keep borrowed memory alive when close fails.
    test_lua_graphics_failure(0);
    execute(L, "assert(s:close()); assert(s:close()); assert(not pcall(s.pixel,s,0,0,0)); "
               "old=s; s=assert(t.graphics.open(8,8)); assert(old:close()); "
               "old=nil; collectgarbage(); assert(s:clear(0)); assert(s:close()); "
               "s=nil; collectgarbage(); "
               "assert(not pcall(function() local g <close> = assert(t.graphics.open(8,8)); error('test') end)); "
               "do local g=assert(t.graphics.open(8,8)) end; collectgarbage()");
    assert(!rt->graphics.open && test_lua_mode() == 0U);
    test_lua_graphics_failure(ENOMEM);
    execute(L, "local a,b,c=t.graphics.open(8,8); assert(a==nil and type(b)=='string' and type(c)=='number')");
    test_lua_graphics_failure(0);
    execute(L, "s=assert(t.graphics.open(8,8))");
    test_lua_interrupt();
    execute(L, "local ok,e=pcall(s.present,s); assert(not ok and e:find('interrupted')); assert(s:close())");
    execute(L, "s=assert(t.graphics.open(8,8)); local data=string.rep('x',128); "
               "assert(s:blit(0,0,8,8,data))");
    rt->fail_after = rt->allocations;
    assert(luaL_loadstring(L, "return s:blit(0,0,8,8,string.rep('x',128))") != LUA_OK ||
           lua_pcall(L, 0, 0, 0) != LUA_OK);
    rt->fail_after = 0U;
    lua_settop(L, 0);
    assert(lua_tabos_graphics_close(rt) == 0);
    execute(L, "s=nil; collectgarbage(); s=assert(t.graphics.open(8,8))");
    assert(lua_tabos_graphics_close(rt) == 0);
    assert(!rt->graphics.open && test_lua_mode() == 0U);
    test_lua_mode_failure(TABOS_TTY_GET_MODE);
    execute(L, "assert(not t.graphics.open(8,8))");
    assert(!rt->graphics.open);
    test_lua_mode_failure(TABOS_TTY_SET_MODE);
    execute(L, "assert(not t.graphics.open(8,8))");
    assert(!rt->graphics.open && test_lua_mode() == 0U);
    execute(L, "s=assert(t.graphics.open(8,8))");
    test_lua_mode_failure(TABOS_TTY_SET_MODE);
    execute(L, "assert(not s:close()); assert(not t.graphics.open(8,8)); assert(s:close())");
    assert(!rt->graphics_mode_changed && test_lua_mode() == 0U);
    // Exercise allocation failure after resource acquisition, including blit scratch
    // allocation, then recover in the same state without leaked external canvas memory.
    for (size_t offset = 0U; offset < 20U; ++offset) {
        assert(luaL_loadstring(L, "local g <close> = assert(t.graphics.open(8,8)); "
                                  "local bytes=string.rep('z',128); assert(g:blit(0,0,8,8,bytes)); "
                                  "assert(g:present())") == LUA_OK);
        rt->fail_after = rt->allocations + offset;
        (void) lua_pcall(L, 0, 0, 0);
        rt->fail_after = 0U;
        lua_settop(L, 0);
        assert(lua_tabos_graphics_close(rt) == 0);
        lua_gc(L, LUA_GCCOLLECT);
        assert(!rt->graphics.open && test_lua_mode() == 0U);
    }
    execute(L, "s=assert(t.graphics.open(320,200)); assert(not pcall(s.pointer_poll,s)); "
               "assert(s:pointer_close()); assert(s:pointer_open()); assert(not s:pointer_open()); "
               "assert(s:pointer_poll()==nil)");
    // 320x200 uses scale 3 with (160,60) letterboxing on a 1280x720 display.
    tabos_pointer_event_t touch = {.type       = TABOS_POINTER_DOWN,
                                   .device_id  = 42U,
                                   .contact_id = UINT32_MAX,
                                   .x          = 190,
                                   .y          = 120,
                                   .buttons    = 1U,
                                   .pressure   = 32768U,
                                   .flags      = TABOS_POINTER_EVENT_HAS_PRESSURE};
    test_lua_pointer_event(touch);
    execute(L, "local e=assert(s:pointer_poll()); assert(e.type=='down' and e.x==10 and e.y==20); "
               "assert(e.inside and e.display_x==190 and e.display_y==120); "
               "assert(e.contact_id==4294967295 and e.device_id==42 and e.buttons==1 and e.pressure==32768)");
    touch.type  = TABOS_POINTER_MOVE;
    touch.x     = 159;
    touch.y     = 59;
    touch.flags = 0U;
    test_lua_pointer_event(touch);
    execute(L, "local e=s:pointer_poll(); assert(e.type=='move' and e.x==-1 and e.y==-1); "
               "assert(not e.inside and e.pressure==nil)");
    touch.type = TABOS_POINTER_UP;
    touch.x    = 1120;
    touch.y    = 660;
    test_lua_pointer_event(touch);
    execute(L, "local e=s:pointer_poll(); assert(e.type=='up' and e.x==320 and e.y==200 and not e.inside)");
    touch.type = TABOS_POINTER_CANCEL;
    test_lua_pointer_event(touch);
    execute(L, "assert(s:pointer_poll().type=='cancel')");
    test_lua_key(TABOS_KEY_LEFT, true);
    execute(L, "assert(s:pointer_poll()==nil); assert(s:poll().key=='left'); assert(s:is_down('left'))");
    test_lua_pointer_failure(EIO);
    execute(L, "local e,m,c=s:pointer_poll(); assert(e==nil and type(m)=='string' and c~=nil); "
               "assert(not s:pointer_close()); assert(not s:close())");
    assert(rt->graphics.open && test_lua_pointer_opened());
    test_lua_pointer_failure(0);
    execute(L, "assert(s:pointer_close()); assert(s:pointer_close()); assert(s:pointer_open()); "
               "assert(s:close()); assert(not pcall(s.pointer_open,s)); "
               "assert(not pcall(s.pointer_poll,s)); assert(not pcall(s.pointer_close,s)); "
               "old=s; s=assert(t.graphics.open(320,200)); assert(s:pointer_open()); assert(old:close())");
    assert(test_lua_pointer_opened());
    execute(L, "assert(s:close()); s=assert(t.graphics.open(320,200))");
    test_lua_pointer_failure(ENODEV);
    execute(L, "assert(not s:pointer_open())");
    assert(!test_lua_pointer_opened());
    test_lua_pointer_failure(0);
    execute(L, "assert(s:close()); assert(not pcall(function() local s <close> = "
               "assert(t.graphics.open(320,200)); assert(s:pointer_open()); error('touch') end)); "
               "do local s=assert(t.graphics.open(320,200)); assert(s:pointer_open()) end; collectgarbage()");
    assert(!test_lua_pointer_opened());
    // Failed event-table construction must preserve the pending release for retry.
    execute(L, "s=assert(t.graphics.open(320,200)); assert(s:pointer_open())");
    for (size_t offset = 0U; offset < 15U; ++offset) {
        test_lua_pointer_event(touch);
        assert(luaL_loadstring(L, "return s:pointer_poll()") == LUA_OK);
        rt->fail_after = rt->allocations + offset;
        int status     = lua_pcall(L, 0, 1, 0);
        rt->fail_after = 0U;
        lua_settop(L, 0);
        if (status != LUA_OK) {
            execute(L, "assert(s:pointer_poll().type=='cancel')");
        }
    }
    test_lua_interrupt();
    assert(luaL_loadstring(L, "s:pointer_poll()") == LUA_OK);
    assert(lua_pcall(L, 0, 0, 0) != LUA_OK);
    lua_settop(L, 0);
    rt->interrupted = false;
    assert(lua_tabos_graphics_close(rt) == 0 && !test_lua_pointer_opened());
    execute(L, "a=t.audio; assert(a.info().default_sample_rate==44100); "
               "assert(a.MAX_WRITE_BYTES==16384); assert(not pcall(a.open,12345)); "
               "assert(not pcall(a.open,'44100')); assert(not pcall(a.open,0/0)); "
               "assert(not pcall(a.open,44100,3)); assert(not pcall(a.open,44100,1,'microphone')); "
               "sound=assert(a.open()); assert(sound:set_volume(500)); "
               "assert(not pcall(sound.set_volume,sound,1001)); "
               "assert(not pcall(sound.write,sound,'')); assert(not pcall(sound.write,sound,'x')); "
               "assert(not pcall(sound.write,sound,string.rep('x',16386))); "
               "assert(not pcall(sound.write,sound,'abcd',1)); "
               "assert(not pcall(sound.write,sound,'abcd',4)); "
               "assert(sound:write(string.char(0,128,255,127))==4)");
    const unsigned char audio_bytes[] = {0U, 128U, 255U, 127U};
    test_lua_audio_bytes(audio_bytes, sizeof(audio_bytes));
    execute(L, "assert(sound:flush()); local pcm=string.rep('x',16384); "
               "assert(sound:write(pcm)==16384); assert(sound:write(pcm,2)==16382); "
               "assert(sound:write('abcd')==2); local n,e,c=sound:write('ab'); "
               "assert(n==nil and type(e)=='string' and c==a.EAGAIN); "
               "assert(sound:status().buffered_bytes==32768); assert(sound:flush()); "
               "assert(sound:status().buffered_bytes==0); assert(not a.open(48000)); "
               "assert(sound:close()); assert(sound:close()); assert(not pcall(sound.status,sound)); "
               "old=sound; sound=assert(a.open(48000,2,'headphone')); assert(old:close()); "
               "old=nil; collectgarbage(); assert(sound:write('abcd')==4); "
               "assert(not pcall(sound.write,sound,'ab')); assert(sound:close()); "
               "do local s <close> = assert(a.open()) end; "
               "assert(not pcall(function() local s <close> = assert(a.open()); error('expected') end)); "
               "do local s=assert(a.open()) end; collectgarbage()");
    assert(test_lua_audio_open_count() == 0U);
    test_lua_audio_failure(EIO);
    execute(L, "assert(not a.open()); assert(not a.info())");
    test_lua_audio_failure(0);
    execute(L, "sound=assert(a.open())");
    test_lua_audio_failure(EIO);
    execute(L, "assert(not sound:write('ab')); assert(not sound:flush()); "
               "assert(not sound:status()); assert(not sound:set_volume(500)); assert(not sound:close())");
    test_lua_audio_failure(0);
    execute(L, "assert(sound:close()); streams={}; for i=1,8 do streams[i]=assert(a.open()) end; "
               "assert(not a.open()); for i=1,8 do assert(streams[i]:close()) end");
    for (size_t offset = 0U; offset < 16U; ++offset) {
        assert(luaL_loadstring(L, "local s <close> = assert(a.open()); "
                                  "assert(s:write(string.rep('x',128))); s:status()") == LUA_OK);
        rt->fail_after = rt->allocations + offset;
        (void) lua_pcall(L, 0, 0, 0);
        rt->fail_after = 0U;
        lua_settop(L, 0);
        assert(lua_tabos_audio_close(rt) == 0);
        lua_gc(L, LUA_GCCOLLECT);
        assert(test_lua_audio_open_count() == 0U);
    }
    execute(L, "sound=assert(a.open())");
    test_lua_interrupt();
    execute(L, "local ok,e=pcall(sound.write,sound,'ab'); assert(not ok and e:find('interrupted')); "
               "assert(sound:close())");
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
