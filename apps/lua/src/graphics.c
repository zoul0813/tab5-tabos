#include <lua_tabos/runtime.h>
#include <errno.h>
#include <tabos/tty.h>
#include <sys/ioctl.h>
#include <string.h>

#define SCREEN_TYPE   "tabos.screen"
#define CANVAS_PIXELS (256U * 1024U)

// The runtime owns the context; userdata carries a generation, never a pixel pointer.
// An old finalizer cannot close a subsequently opened screen.
typedef struct {
        uint64_t generation;
} screen_t;

static lua_Integer integer(lua_State* L, int index, lua_Integer low, lua_Integer high)
{
    luaL_checktype(L, index, LUA_TNUMBER);
    lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= low && value <= high, index, "value out of range");
    return value;
}
static int32_t coordinate(lua_State* L, int index)
{
    // Bound both SDK arithmetic and work per C call (including offscreen lines).
    return (int32_t) integer(L, index, -32768, 32767);
}
static uint32_t extent(lua_State* L, int index)
{
    return (uint32_t) integer(L, index, 0, 32767);
}
static tabos_color_t color(lua_State* L, int index)
{
    return (tabos_color_t) integer(L, index, 0, UINT16_MAX);
}
static int result(lua_State* L, int status)
{
    return luaL_fileresult(L, status == 0, NULL);
}
static tabos_graphics_t* screen(lua_State* L)
{
    screen_t* handle        = luaL_checkudata(L, 1, SCREEN_TYPE);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    luaL_argcheck(L, rt->graphics.open && handle->generation == rt->graphics_generation, 1, "screen is closed");
    return &rt->graphics;
}
static void reset_input(lua_tabos_runtime_t* rt)
{
    rt->head = rt->count = 0U;
    rt->overflow = rt->cancelled = false;
    memset(rt->keys, 0, sizeof(rt->keys));
}
int lua_tabos_graphics_close(lua_tabos_runtime_t* rt)
{
    if (rt->pointer_open) {
        if (tabos_pointer_close(rt->pointer) != 0) {
            return -1;
        }
        rt->pointer_open    = false;
        rt->pointer_pending = false;
    }
    if (rt->graphics.open) {
        if (tabos_graphics_close(&rt->graphics) != 0) {
            return -1;
        }
        reset_input(rt);
    }
    if (rt->graphics_mode_changed) {
        if (ioctl(0, TABOS_TTY_SET_MODE, rt->graphics_mode) != 0) {
            return -1;
        }
        rt->graphics_mode_changed = false;
    }
    return 0;
}

static int close_screen(lua_State* L)
{
    screen_t* handle        = luaL_checkudata(L, 1, SCREEN_TYPE);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (handle->generation != rt->graphics_generation) {
        return result(L, 0);
    }
    return result(L, lua_tabos_graphics_close(rt));
}
static int finalize_screen(lua_State* L)
{
    screen_t* handle        = luaL_checkudata(L, 1, SCREEN_TYPE);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (handle->generation == rt->graphics_generation) {
        // Never allocate or throw from cleanup, including OOM unwinding.
        (void) lua_tabos_graphics_close(rt);
    }
    return 0;
}
static int open_screen(lua_State* L)
{
    uint32_t width  = (uint32_t) integer(L, 1, 1, 32767);
    uint32_t height = (uint32_t) integer(L, 2, 1, 32767);
    luaL_argcheck(L, (uint64_t) width * height <= CANVAS_PIXELS, 2, "canvas exceeds 512 KiB");
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (rt->closing) {
        return luaL_error(L, "graphics unavailable during state finalization");
    }
    if (rt->graphics.open || rt->graphics_mode_changed) {
        errno = EBUSY;
        return result(L, -1);
    }
    if (ioctl(0, TABOS_TTY_GET_MODE, &rt->graphics_mode) != 0) {
        return result(L, -1);
    }
    screen_t* handle   = lua_newuserdatauv(L, sizeof(*handle), 0);
    handle->generation = 0U;
    luaL_setmetatable(L, SCREEN_TYPE);
    // Allocate every Lua object before acquiring graphics. Opening cannot longjmp.
    rt->graphics = (tabos_graphics_t) {.width = width, .height = height};
    if (tabos_graphics_open(&rt->graphics) != 0) {
        return result(L, -1);
    }
    ++rt->graphics_generation;
    handle->generation = rt->graphics_generation;
    if (ioctl(0, TABOS_TTY_SET_MODE, rt->graphics_mode | (uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        int saved_errno = errno;
        (void) lua_tabos_graphics_close(rt);
        errno = saved_errno;
        return result(L, -1);
    }
    rt->graphics_mode_changed = true;
    reset_input(rt);
    return 1;
}
static int rgb(lua_State* L)
{
    unsigned int red   = (unsigned int) integer(L, 1, 0, 255);
    unsigned int green = (unsigned int) integer(L, 2, 0, 255);
    unsigned int blue  = (unsigned int) integer(L, 3, 0, 255);
    lua_pushinteger(L, TABOS_RGB565(red, green, blue));
    return 1;
}
static int size(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    lua_pushinteger(L, graphics->width);
    lua_pushinteger(L, graphics->height);
    return 2;
}
static int clear(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    tabos_color_t value        = color(L, 2);
    return result(L, tabos_graphics_clear(graphics, value));
}
static int letterbox(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    tabos_color_t value        = color(L, 2);
    return result(L, tabos_graphics_set_letterbox_color(graphics, value));
}
static int pixel(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    int32_t x = coordinate(L, 2), y = coordinate(L, 3);
    tabos_color_t value = color(L, 4);
    return result(L, tabos_graphics_pixel(graphics, x, y, value));
}
static int line(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    int32_t x = coordinate(L, 2), y = coordinate(L, 3);
    int32_t x1 = coordinate(L, 4), y1 = coordinate(L, 5);
    tabos_color_t value = color(L, 6);
    return result(L, tabos_graphics_line(graphics, x, y, x1, y1, value));
}
static int rectangle(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    int32_t x = coordinate(L, 2), y = coordinate(L, 3);
    uint32_t width = extent(L, 4), height = extent(L, 5);
    tabos_color_t value = color(L, 6);
    if (lua_toboolean(L, lua_upvalueindex(1))) {
        return result(L, tabos_graphics_fill_rect(graphics, x, y, width, height, value));
    }
    return result(L, tabos_graphics_rect(graphics, x, y, width, height, value));
}
static int blit(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    int32_t x = coordinate(L, 2), y = coordinate(L, 3);
    uint32_t width  = (uint32_t) integer(L, 4, 1, 32767);
    uint32_t height = (uint32_t) integer(L, 5, 1, 32767);
    luaL_argcheck(L, (uint64_t) width * height <= CANVAS_PIXELS, 5, "bitmap exceeds 512 KiB");
    luaL_checktype(L, 6, LUA_TSTRING);
    size_t length;
    const unsigned char* bytes = (const unsigned char*) lua_tolstring(L, 6, &length);
    size_t count               = (size_t) width * height;
    luaL_argcheck(L, length == count * 2U, 6, "expected width * height * 2 RGB565 bytes");
    // Decode into aligned Lua-owned storage. Logical blits copy synchronously;
    // neither this buffer nor the source string is borrowed after the call.
    tabos_color_t* pixels = lua_newuserdatauv(L, length, 0);
    for (size_t i = 0U; i < count; ++i) {
        pixels[i] = (tabos_color_t) (bytes[i * 2U] | ((uint16_t) bytes[i * 2U + 1U] << 8U));
    }
    return result(L, tabos_graphics_blit(graphics, x, y, width, height, pixels));
}
static int present(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    lua_tabos_console_poll(lua_tabos_runtime(L));
    lua_tabos_hook(L, NULL);
    int status      = tabos_graphics_present(graphics);
    int saved_errno = errno;
    lua_tabos_console_poll(lua_tabos_runtime(L));
    lua_tabos_hook(L, NULL);
    errno = saved_errno;
    return result(L, status);
}

static const struct {
        const char* name;
        tabos_key_t key;
} key_names[] = {
    {     "left",      TABOS_KEY_LEFT},
    {    "right",     TABOS_KEY_RIGHT},
    {       "up",        TABOS_KEY_UP},
    {     "down",      TABOS_KEY_DOWN},
    {    "space",     TABOS_KEY_SPACE},
    {    "enter",     TABOS_KEY_ENTER},
    {   "escape",    TABOS_KEY_ESCAPE},
    {      "tab",       TABOS_KEY_TAB},
    {"backspace", TABOS_KEY_BACKSPACE},
    {   "delete",    TABOS_KEY_DELETE},
    {     "home",      TABOS_KEY_HOME},
    {      "end",       TABOS_KEY_END},
    {  "page_up",   TABOS_KEY_PAGE_UP},
    {"page_down", TABOS_KEY_PAGE_DOWN},
    {     "ctrl",      TABOS_KEY_CTRL},
    {    "shift",     TABOS_KEY_SHIFT},
    {      "alt",       TABOS_KEY_ALT},
    {      "gui",       TABOS_KEY_GUI},
    {      "sym",       TABOS_KEY_SYM},
};
static void push_key(lua_State* L, tabos_key_t key)
{
    char letter = 0;
    if (key >= TABOS_KEY_A && key <= TABOS_KEY_Z) {
        letter = (char) ('a' + key - TABOS_KEY_A);
    } else if (key >= TABOS_KEY_1 && key <= TABOS_KEY_9) {
        letter = (char) ('1' + key - TABOS_KEY_1);
    } else if (key == TABOS_KEY_0) {
        letter = '0';
    }
    if (letter != 0) {
        lua_pushlstring(L, &letter, 1U);
        return;
    }
    for (size_t i = 0U; i < sizeof(key_names) / sizeof(key_names[0]); ++i) {
        if (key_names[i].key == key) {
            lua_pushstring(L, key_names[i].name);
            return;
        }
    }
    lua_pushstring(L, "unknown");
}
static int is_down(lua_State* L)
{
    (void) screen(L);
    luaL_checktype(L, 2, LUA_TSTRING);
    const char* name = lua_tabos_checkpath(L, 2);
    tabos_key_t key  = TABOS_KEY_UNKNOWN;
    if (strlen(name) == 1U && name[0] >= 'a' && name[0] <= 'z') {
        key = (tabos_key_t) (TABOS_KEY_A + name[0] - 'a');
    } else if (strlen(name) == 1U && name[0] >= '1' && name[0] <= '9') {
        key = (tabos_key_t) (TABOS_KEY_1 + name[0] - '1');
    } else if (strcmp(name, "0") == 0) {
        key = TABOS_KEY_0;
    } else {
        for (size_t i = 0U; i < sizeof(key_names) / sizeof(key_names[0]); ++i) {
            if (strcmp(name, key_names[i].name) == 0) {
                key = key_names[i].key;
                break;
            }
        }
    }
    luaL_argcheck(L, key != TABOS_KEY_UNKNOWN, 2, "unknown key name");
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    lua_tabos_console_poll(rt);
    lua_tabos_hook(L, NULL);
    lua_pushboolean(L, rt->keys[key]);
    return 1;
}
static int poll(lua_State* L)
{
    (void) screen(L);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    lua_tabos_console_poll(rt);
    lua_tabos_hook(L, NULL);
    if (!rt->overflow && rt->count == 0U) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 5);
    if (rt->overflow) {
        lua_pushliteral(L, "overflow");
        lua_setfield(L, -2, "type");
        rt->overflow = false;
        return 1;
    }
    tabos_input_event_t* event = &rt->events[rt->head];
    lua_pushstring(L, event->type == TABOS_INPUT_KEY_DOWN ? "key_down" : "key_up");
    lua_setfield(L, -2, "type");
    push_key(L, event->key);
    lua_setfield(L, -2, "key");
    lua_pushinteger(L, event->key);
    lua_setfield(L, -2, "code");
    lua_pushinteger(L, event->modifiers);
    lua_setfield(L, -2, "modifiers");
    lua_pushboolean(L, event->repeat);
    lua_setfield(L, -2, "repeat");
    rt->head = (rt->head + 1U) % LUA_TABOS_QUEUE_SIZE;
    --rt->count;
    return 1;
}

static int pointer_open(lua_State* L)
{
    (void) screen(L);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (rt->closing) {
        return luaL_error(L, "pointer unavailable during state finalization");
    }
    if (rt->pointer_open) {
        errno = EBUSY;
        return result(L, -1);
    }
    tabos_device_info_t device;
    if (tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &device) != 0) {
        return result(L, -1);
    }
    rt->pointer = tabos_pointer_open(device.id);
    if (rt->pointer == TABOS_POINTER_STREAM_INVALID) {
        return result(L, -1);
    }
    // Record ownership before any Lua allocation can fail.
    rt->pointer_open    = true;
    rt->pointer_pending = false;
    return result(L, 0);
}

static int pointer_close(lua_State* L)
{
    (void) screen(L);
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (rt->pointer_open) {
        if (tabos_pointer_close(rt->pointer) != 0) {
            return result(L, -1);
        }
        rt->pointer_open    = false;
        rt->pointer_pending = false;
    }
    return result(L, 0);
}

static lua_Integer canvas_coordinate(int32_t value, uint32_t offset, uint32_t scale)
{
    int64_t relative = (int64_t) value - offset;
    // Floor division keeps coordinates just outside a letterbox edge negative.
    if (relative < 0) {
        return -((-relative + scale - 1U) / scale);
    }
    return relative / scale;
}

static int pointer_poll(lua_State* L)
{
    tabos_graphics_t* graphics = screen(L);
    lua_tabos_runtime_t* rt    = lua_tabos_runtime(L);
    luaL_argcheck(L, rt->pointer_open, 1, "pointer is closed");
    lua_tabos_console_poll(rt);
    lua_tabos_hook(L, NULL);
    if (!rt->pointer_pending && tabos_pointer_read(rt->pointer, &rt->pointer_event) != 0) {
        if (errno == EAGAIN) {
            lua_pushnil(L);
            return 1;
        }
        return result(L, -1);
    }
    // Retain the event across allocation failures until its whole table is built.
    rt->pointer_pending         = true;
    tabos_pointer_event_t event = rt->pointer_event;
    // Snapshot geometry before allocation can run a finalizer that closes the screen.
    lua_Integer x = canvas_coordinate(event.x, graphics->output_x, graphics->scale);
    lua_Integer y = canvas_coordinate(event.y, graphics->output_y, graphics->scale);
    bool inside   = x >= 0 && y >= 0 && x < graphics->width && y < graphics->height;
    lua_createtable(L, 0, 10);
    static const char* const types[] = {"down", "move", "up", "cancel"};
    lua_pushstring(L, types[event.type]);
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, y);
    lua_setfield(L, -2, "y");
    lua_pushboolean(L, inside);
    lua_setfield(L, -2, "inside");
    lua_pushinteger(L, event.x);
    lua_setfield(L, -2, "display_x");
    lua_pushinteger(L, event.y);
    lua_setfield(L, -2, "display_y");
    lua_pushinteger(L, event.device_id);
    lua_setfield(L, -2, "device_id");
    lua_pushinteger(L, event.contact_id);
    lua_setfield(L, -2, "contact_id");
    lua_pushinteger(L, event.buttons);
    lua_setfield(L, -2, "buttons");
    if ((event.flags & TABOS_POINTER_EVENT_HAS_PRESSURE) != 0U) {
        lua_pushinteger(L, event.pressure);
        lua_setfield(L, -2, "pressure");
    }
    rt->pointer_pending = false;
    return 1;
}
void lua_tabos_graphics_module(lua_State* L)
{
    static const luaL_Reg methods[] = {
        {              "close",    close_screen},
        {               "size",            size},
        {              "clear",           clear},
        {              "pixel",           pixel},
        {               "line",            line},
        {               "blit",            blit},
        {            "present",         present},
        {"set_letterbox_color",       letterbox},
        {               "poll",            poll},
        {            "is_down",         is_down},
        {       "pointer_open",    pointer_open},
        {       "pointer_poll",    pointer_poll},
        {      "pointer_close",   pointer_close},
        {               "__gc", finalize_screen},
        {            "__close", finalize_screen},
        {                 NULL,            NULL}
    };
    luaL_newmetatable(L, SCREEN_TYPE);
    luaL_setfuncs(L, methods, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushliteral(L, "tabos.screen");
    lua_setfield(L, -2, "__metatable");
    lua_pushboolean(L, 0);
    lua_pushcclosure(L, rectangle, 1);
    lua_setfield(L, -2, "rect");
    lua_pushboolean(L, 1);
    lua_pushcclosure(L, rectangle, 1);
    lua_setfield(L, -2, "fill_rect");
    lua_pop(L, 1);
    lua_pushcfunction(L, rgb);
    lua_setfield(L, -2, "rgb");
    lua_newtable(L);
    lua_pushcfunction(L, open_screen);
    lua_setfield(L, -2, "open");
    lua_setfield(L, -2, "graphics");
}
