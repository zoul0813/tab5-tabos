#include <lua_tabos/runtime.h>
#include <tabos/tty.h>
#include <tabos/internal/elf_api.h>
#include <tabos/system.h>
#include <tabos/clock.h>
#include <tabos/runtime_time.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static uint32_t mode = TABOS_TTY_MODE_SCROLL_KEYS;
static unsigned long mode_failure;
void test_lua_mode_failure(unsigned long request)
{
    mode_failure = request;
}
static uint64_t ticks;
static const char* input;
static size_t input_index;
static bool ctrl_c;
static size_t typeahead;
static tabos_input_event_t pending_event;
static bool has_event;
void test_lua_key(tabos_key_t key, bool down)
{
    pending_event = (tabos_input_event_t) {.type = down ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP, .key = key};
    has_event     = true;
}
void test_lua_input(const char* text)
{
    input       = text;
    input_index = 0U;
}
void test_lua_typeahead(size_t count)
{
    typeahead = count;
}
void test_lua_interrupt(void)
{
    ctrl_c = true;
}
uint32_t test_lua_mode(void)
{
    return mode;
}
int ioctl(int fd, unsigned long request, ...)
{
    (void) fd;
    va_list args;
    va_start(args, request);
    int result = 0;
    if (mode_failure == request) {
        mode_failure = 0U;
        errno        = EIO;
        result       = -1;
    } else if (request == TABOS_TTY_GET_MODE) {
        *va_arg(args, uint32_t*) = mode;
    } else if (request == TABOS_TTY_SET_MODE) {
        mode = va_arg(args, uint32_t);
    } else {
        errno  = ENOTTY;
        result = -1;
    }
    va_end(args);
    return result;
}
bool tabos_input_poll(tabos_input_event_t* event)
{
    if (ctrl_c) {
        ctrl_c = false;
        *event = (tabos_input_event_t) {
            .type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_C, .modifiers = TABOS_MODIFIER_CONTROL};
        return true;
    }
    if (has_event) {
        has_event = false;
        *event    = pending_event;
        return true;
    }
    if (typeahead != 0U) {
        --typeahead;
        *event = (tabos_input_event_t) {.type = TABOS_INPUT_TEXT, .text = "x"};
        return true;
    }
    return false;
}
tabos_wait_source_t tabos_input_wait_source(void)
{
    return 1;
}
// Readline waits drive deterministic input; running-script hooks only see explicit Ctrl-C.
static lua_tabos_runtime_t* reader;
void test_lua_reader(lua_tabos_runtime_t* rt)
{
    reader = rt;
}
int tabos_wait(tabos_wait_item_t* items, uint32_t count, uint32_t timeout)
{
    assert(count == 1U && timeout == TABOS_WAIT_TIMEOUT_INFINITE);
    if (reader == NULL || input == NULL || input[input_index] == '\0') {
        errno = EIO;
        return -1;
    }
    char c = input[input_index++];
    if (c == 3) {
        reader->interrupted = true;
    } else {
        reader->events[0] = (tabos_input_event_t) {
            .type = TABOS_INPUT_TEXT, .text = {c, 0}
        };
        reader->head  = 0U;
        reader->count = 1U;
    }
    items[0].returned_events = TABOS_WAIT_READABLE;
    return 1;
}
uint64_t tabos_monotonic_ms(void)
{
    return ++ticks;
}
int tabos_sleep_ms(uint32_t duration)
{
    ticks += duration;
    return 0;
}
int tabos_clock_get_epoch(int64_t* seconds)
{
    *seconds = 1709164800;
    return 0;
}
int tabos_system_info(tabos_system_info_t* info)
{
    memset(info, 0, sizeof(*info));
    strcpy(info->target, "test");
    info->cpu_cores = 2;
    return 0;
}

// Real portable SDK drawing with a deterministic display transport.
static bool graphics_opened;
static int graphics_failure;
static unsigned int graphics_presents;
void test_lua_graphics_failure(int error)
{
    graphics_failure = error;
}
unsigned int test_lua_graphics_presents(void)
{
    return graphics_presents;
}
static int fake_graphics_open(uint32_t* width, uint32_t* height)
{
    if (graphics_failure != 0) {
        return -graphics_failure;
    }
    assert(!graphics_opened);
    graphics_opened = true;
    *width          = 1280U;
    *height         = 720U;
    return 0;
}
static int fake_graphics_close(void)
{
    if (graphics_failure != 0) {
        return -graphics_failure;
    }
    assert(graphics_opened);
    graphics_opened = false;
    return 0;
}
static int fake_graphics_present(void)
{
    assert(graphics_opened);
    ++graphics_presents;
    return -graphics_failure;
}
static int fake_graphics_blit(const tabos_graphics_blit_options_t* options)
{
    assert(graphics_opened && options->pixels != NULL);
    assert(options->destination.width <= 1280U && options->destination.height <= 720U);
    return 0;
}
static int fake_graphics_clear(uint32_t value)
{
    (void) value;
    assert(graphics_opened);
    return 0;
}
static struct {
        bool open;
        uint32_t rate, channels, buffered;
        unsigned char pcm[32768];
} audio_streams[8];
static int audio_failure;
void test_lua_audio_failure(int error)
{
    audio_failure = error;
}
size_t test_lua_audio_open_count(void)
{
    size_t count = 0U;
    for (size_t i = 0U; i < 8U; ++i) {
        count += audio_streams[i].open;
    }
    return count;
}
static int fake_audio_info(tabos_audio_info_t* value)
{
    *value = (tabos_audio_info_t) {.features            = TABOS_AUDIO_FEATURE_PLAYBACK,
                                   .routes              = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE,
                                   .sample_rates        = TABOS_AUDIO_RATES_ALL,
                                   .default_sample_rate = 44100U};
    return -audio_failure;
}
static int fake_audio_open(const tabos_audio_config_t* config)
{
    if (audio_failure != 0) {
        return -audio_failure;
    }
    assert(config->direction == TABOS_AUDIO_PLAYBACK);
    for (size_t i = 0U; i < 8U; ++i) {
        if (audio_streams[i].open && audio_streams[i].rate != config->sample_rate) {
            return -EBUSY;
        }
    }
    for (size_t i = 0U; i < 8U; ++i) {
        if (!audio_streams[i].open) {
            audio_streams[i].open     = true;
            audio_streams[i].buffered = 0U;
            audio_streams[i].rate     = config->sample_rate;
            audio_streams[i].channels = config->channels;
            return (int) i;
        }
    }
    return -EMFILE;
}
static int fake_audio_close(int stream)
{
    assert(stream >= 0 && stream < 8 && audio_streams[stream].open);
    if (audio_failure != 0) {
        return -audio_failure;
    }
    audio_streams[stream].open = false;
    return 0;
}
static int fake_audio_flush(int stream)
{
    assert(stream >= 0 && stream < 8 && audio_streams[stream].open);
    if (audio_failure != 0) {
        return -audio_failure;
    }
    audio_streams[stream].buffered = 0U;
    return 0;
}
static int fake_audio_write(int stream, const void* pcm, uint32_t bytes)
{
    assert(stream >= 0 && stream < 8 && audio_streams[stream].open);
    assert(bytes <= TABOS_AUDIO_IO_MAX && bytes % (2U * audio_streams[stream].channels) == 0U);
    if (audio_failure != 0) {
        return -audio_failure;
    }
    uint32_t available = 32768U - audio_streams[stream].buffered;
    if (available == 0U) {
        return -EAGAIN;
    }
    uint32_t count = bytes < available ? bytes : available;
    memcpy(audio_streams[stream].pcm + audio_streams[stream].buffered, pcm, count);
    audio_streams[stream].buffered += count;
    return (int) count;
}
static int fake_audio_status(int stream, tabos_audio_status_t* value)
{
    assert(stream >= 0 && stream < 8 && audio_streams[stream].open);
    *value = (tabos_audio_status_t) {.buffered_bytes = audio_streams[stream].buffered, .buffer_capacity = 32768U};
    return -audio_failure;
}
static int fake_audio_volume(int stream, uint32_t volume)
{
    assert(stream >= 0 && stream < 8 && audio_streams[stream].open && volume <= 1000U);
    return -audio_failure;
}
void test_lua_audio_bytes(const void* bytes, size_t count)
{
    assert(audio_streams[0].buffered == count);
    assert(memcmp(audio_streams[0].pcm, bytes, count) == 0);
}
static const tabos_elf_api_t graphics_api = {
    .audio_info       = fake_audio_info,
    .audio_open       = fake_audio_open,
    .audio_close      = fake_audio_close,
    .audio_flush      = fake_audio_flush,
    .audio_write      = fake_audio_write,
    .audio_status     = fake_audio_status,
    .audio_set_volume = fake_audio_volume,

    .graphics_open    = fake_graphics_open,
    .graphics_close   = fake_graphics_close,
    .graphics_present = fake_graphics_present,
    .graphics_blit_ex = fake_graphics_blit,
    .graphics_clear   = fake_graphics_clear,
};
const tabos_elf_api_t* tabos_runtime_api = &graphics_api;
