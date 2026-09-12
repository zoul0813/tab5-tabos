#include <lua_tabos/runtime.h>
#include <errno.h>
#include <string.h>

#define AUDIO_TYPE "tabos.audio.stream"
typedef struct {
        size_t slot;
        uint64_t generation;
} audio_handle_t;

static lua_Integer integer(lua_State* L, int index, lua_Integer low, lua_Integer high)
{
    luaL_checktype(L, index, LUA_TNUMBER);
    lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= low && value <= high, index, "value out of range");
    return value;
}
static int result(lua_State* L, int status)
{
    return luaL_fileresult(L, status == 0, NULL);
}
static int close_slot(lua_tabos_audio_t* slot)
{
    if (!slot->open) {
        return 0;
    }
    if (tabos_audio_close(slot->stream) != 0) {
        return -1;
    }
    slot->open = false;
    return 0;
}
int lua_tabos_audio_close(lua_tabos_runtime_t* rt)
{
    int failure = 0;
    for (size_t i = 0U; i < LUA_TABOS_AUDIO_STREAMS; ++i) {
        if (close_slot(&rt->audio[i]) != 0) {
            failure = errno;
        }
    }
    if (failure != 0) {
        errno = failure;
        return -1;
    }
    return 0;
}
static lua_tabos_audio_t* get_slot(lua_State* L, bool require_open)
{
    audio_handle_t* handle  = luaL_checkudata(L, 1, AUDIO_TYPE);
    lua_tabos_audio_t* slot = &lua_tabos_runtime(L)->audio[handle->slot];
    if (!slot->open || slot->generation != handle->generation) {
        if (require_open) {
            luaL_argerror(L, 1, "audio stream is closed");
        }
        return NULL;
    }
    return slot;
}
static int close_stream(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, false);
    return result(L, slot == NULL ? 0 : close_slot(slot));
}
static int finalize(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, false);
    if (slot != NULL) {
        // No allocation or exception during GC/error cleanup. Runtime teardown retries.
        (void) close_slot(slot);
    }
    return 0;
}
static int open_stream(lua_State* L)
{
    uint32_t rate     = TABOS_AUDIO_DEFAULT_SAMPLE_RATE;
    uint32_t channels = 1U;
    if (!lua_isnoneornil(L, 1)) {
        rate = (uint32_t) integer(L, 1, 8000, 96000);
    }
    static const uint32_t rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000};
    bool supported                = false;
    for (size_t i = 0U; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        supported = supported || rates[i] == rate;
    }
    luaL_argcheck(L, supported, 1, "unsupported sample rate");
    if (!lua_isnoneornil(L, 2)) {
        channels = (uint32_t) integer(L, 2, 1, 2);
    }
    uint32_t route = TABOS_AUDIO_ROUTE_SPEAKER;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TSTRING);
        const char* name = lua_tabos_checkpath(L, 3);
        luaL_argcheck(L, strcmp(name, "speaker") == 0 || strcmp(name, "headphone") == 0, 3, "invalid playback route");
        if (strcmp(name, "headphone") == 0) {
            route = TABOS_AUDIO_ROUTE_HEADPHONE;
        }
    }
    lua_tabos_runtime_t* rt = lua_tabos_runtime(L);
    if (rt->closing) {
        return luaL_error(L, "audio unavailable during state finalization");
    }
    audio_handle_t* handle = lua_newuserdatauv(L, sizeof(*handle), 0);
    *handle                = (audio_handle_t) {0};
    luaL_setmetatable(L, AUDIO_TYPE);
    // Allocation can run user finalizers which open or close streams. Select the
    // slot only after all Lua allocations, then acquire without further allocation.
    size_t index = 0U;
    while (index < LUA_TABOS_AUDIO_STREAMS && rt->audio[index].open) {
        ++index;
    }
    if (index == LUA_TABOS_AUDIO_STREAMS) {
        errno = EMFILE;
        return result(L, -1);
    }
    handle->slot                = index;
    tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK, .channels = channels, .route = route, .sample_rate = rate};
    tabos_audio_stream_t stream = tabos_audio_open(&config);
    if (stream == TABOS_AUDIO_STREAM_INVALID) {
        return result(L, -1);
    }
    lua_tabos_audio_t* slot = &rt->audio[index];
    ++slot->generation;
    slot->stream       = stream;
    slot->channels     = channels;
    slot->open         = true;
    handle->generation = slot->generation;
    return 1;
}
static int write_pcm(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, true);
    luaL_checktype(L, 2, LUA_TSTRING);
    size_t length;
    const char* bytes = lua_tolstring(L, 2, &length);
    size_t frame      = slot->channels * 2U;
    luaL_argcheck(L, length > 0U && length <= TABOS_AUDIO_IO_MAX && length % frame == 0U, 2,
                  "expected 1..16384 bytes of complete signed-16-bit PCM frames");
    size_t offset = 0U;
    if (!lua_isnoneornil(L, 3)) {
        offset = (size_t) integer(L, 3, 0, (lua_Integer) length - 1);
        luaL_argcheck(L, offset % frame == 0U, 3, "offset must align to a PCM frame");
    }
    lua_tabos_console_poll(lua_tabos_runtime(L));
    lua_tabos_hook(L, NULL);
    // The SDK copies accepted bytes into its bounded ring before returning.
    // Never wait, resample, retain the string, or retry after partial progress.
    int count = tabos_audio_write(slot->stream, bytes + offset, (uint32_t) (length - offset));
    if (count < 0) {
        return result(L, -1);
    }
    lua_pushinteger(L, count);
    return 1;
}
static int flush(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, true);
    return result(L, tabos_audio_flush(slot->stream));
}
static int volume(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, true);
    uint32_t value          = (uint32_t) integer(L, 2, 0, TABOS_AUDIO_VOLUME_MAX);
    return result(L, tabos_audio_set_volume(slot->stream, value));
}
static void field(lua_State* L, const char* name, uint32_t value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}
static int status(lua_State* L)
{
    lua_tabos_audio_t* slot = get_slot(L, true);
    tabos_audio_status_t value;
    if (tabos_audio_get_status(slot->stream, &value) != 0) {
        return result(L, -1);
    }
    lua_createtable(L, 0, 4);
    field(L, "buffered_bytes", value.buffered_bytes);
    field(L, "buffer_capacity", value.buffer_capacity);
    field(L, "underruns", value.underruns);
    field(L, "overruns", value.overruns);
    return 1;
}
static int info(lua_State* L)
{
    tabos_audio_info_t value;
    if (tabos_audio_get_info(&value) != 0) {
        return result(L, -1);
    }
    lua_createtable(L, 0, 5);
    field(L, "features", value.features);
    field(L, "routes", value.routes);
    field(L, "sample_rates", value.sample_rates);
    field(L, "default_sample_rate", value.default_sample_rate);
    field(L, "capture_channels", value.capture_channels);
    return 1;
}
void lua_tabos_audio_module(lua_State* L)
{
    static const luaL_Reg methods[] = {
        {     "write",    write_pcm},
        {     "flush",        flush},
        {"set_volume",       volume},
        {    "status",       status},
        {     "close", close_stream},
        {   "__close",     finalize},
        {      "__gc",     finalize},
        {        NULL,         NULL}
    };
    luaL_newmetatable(L, AUDIO_TYPE);
    luaL_setfuncs(L, methods, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushliteral(L, AUDIO_TYPE);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, open_stream);
    lua_setfield(L, -2, "open");
    lua_pushcfunction(L, info);
    lua_setfield(L, -2, "info");
    field(L, "EAGAIN", EAGAIN);
    field(L, "MAX_WRITE_BYTES", TABOS_AUDIO_IO_MAX);
    lua_setfield(L, -2, "audio");
}
