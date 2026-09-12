#include <lua_tabos/runtime.h>
#include <tabos/tty.h>
#include <tabos/system.h>
#include <tabos/clock.h>
#include <tabos/runtime_time.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static uint32_t mode = TABOS_TTY_MODE_SCROLL_KEYS;
static uint64_t ticks;
static const char* input;
static size_t input_index;
static bool ctrl_c;
static size_t typeahead;
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
    if (request == TABOS_TTY_GET_MODE) {
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
