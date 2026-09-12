#include <tabos/console.h>

#include <tabos/config/console.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

#include "platform_test.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        const tabos_console_session_t* session;
        atomic_bool start;
        atomic_bool failed;
} concurrent_write_context_t;

static void* write_during_resize(void* opaque)
{
    concurrent_write_context_t* context = opaque;
    while (!atomic_load_explicit(&context->start, memory_order_acquire)) {}
    for (size_t iteration = 0U; iteration < 512U; ++iteration) {
        if (!tabos_console_write(context->session, "x")) {
            atomic_store_explicit(&context->failed, true, memory_order_release);
            break;
        }
    }
    return NULL;
}

int main(void)
{
    if (!input_init()) {
        return 1;
    }
    if (!display_init()) {
        return 1;
    }

    terminal_t terminal;
    if (!terminal_init(&terminal, display_framebuffer(), 2U)) {
        return 1;
    }
    terminal_clear(&terminal);
    if (!console_init(&terminal)) {
        return 1;
    }

    tabos_console_session_t foreground = {0};
    tabos_console_session_t background = {0};
    if (!tabos_console_acquire(&foreground) || tabos_console_acquire(&background) ||
        !tabos_console_is_foreground(&foreground) || tabos_console_is_foreground(&background)) {
        return 1;
    }
    tabos_tty_size_t size;
    if (!console_get_size(&foreground, &size) || size.rows != 24U || size.columns != 80U ||
        console_get_size(&background, &size) || console_get_size(&foreground, NULL)) {
        return 1;
    }
    if (!terminal.cursor_visible || display_framebuffer()->pixels[0] != 0xffff) {
        return 1;
    }
    if (console_next_deadline() != test_platform_time_ms() + TABOS_CURSOR_BLINK_INTERVAL_MS) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_CURSOR_BLINK_INTERVAL_MS - 1U);
    console_update();
    if (!terminal.cursor_phase_visible) {
        return 1;
    }
    test_platform_advance_time_ms(1U);
    console_update();
    if (terminal.cursor_phase_visible) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_CURSOR_BLINK_INTERVAL_MS);
    console_update();
    if (!terminal.cursor_phase_visible) {
        return 1;
    }

    if (!tabos_console_write(&foreground, "AB\tC") || !tabos_console_write(&foreground, "\b\n")) {
        return 1;
    }
    size_t column = 99U;
    size_t row    = 99U;
    if (!tabos_console_get_cursor(&foreground, &column, &row) || column != 0U || row != 1U) {
        return 1;
    }
    static const char embedded_nul[] = {'A', '\0', 'B'};
    if (!tabos_console_write_bytes(&foreground, embedded_nul, sizeof(embedded_nul)) ||
        !tabos_console_write_bytes(&foreground, embedded_nul, 2U) ||
        !tabos_console_write_bytes(&foreground, embedded_nul + 2U, 1U) ||
        terminal.cells[terminal.columns].character != 'A' || terminal.cells[terminal.columns + 1U].character != 'B' ||
        terminal.cells[terminal.columns + 2U].character != 'A' ||
        terminal.cells[terminal.columns + 3U].character != 'B') {
        return 1;
    }

    const tabos_input_event_t submitted = {
        .type = TABOS_INPUT_KEY_DOWN,
        .key  = TABOS_KEY_A,
    };
    if (!input_submit(&submitted)) {
        return 1;
    }
    tabos_input_event_t received;
    if (tabos_console_poll(&background, &received) || !tabos_console_poll(&foreground, &received) ||
        received.key != TABOS_KEY_A) {
        return 1;
    }

    platform_framebuffer_t* framebuffer = display_framebuffer();
    const size_t framebuffer_bytes = framebuffer->stride_pixels * framebuffer->height * sizeof(*framebuffer->pixels);
    platform_pixel_t* graphics_snapshot = malloc(framebuffer_bytes);
    if (graphics_snapshot == NULL) {
        return 1;
    }
    memcpy(graphics_snapshot, framebuffer->pixels, framebuffer_bytes);
    console_set_graphics_active(true);
    if (!console_graphics_active() || !tabos_console_write(&foreground, "graphics-hidden") ||
        tabos_console_page_up(&foreground) || memcmp(graphics_snapshot, framebuffer->pixels, framebuffer_bytes) != 0 ||
        !input_submit(&submitted) || !tabos_console_poll(&foreground, &received) ||
        console_next_deadline() != UINT64_MAX) {
        return 1;
    }
    test_platform_advance_time_ms(TABOS_CURSOR_BLINK_INTERVAL_MS);
    console_update();
    if (terminal.cursor_visible || !terminal.cursor_phase_visible) {
        return 1;
    }

    for (size_t index = 0U; index < framebuffer->stride_pixels * framebuffer->height; ++index) {
        framebuffer->pixels[index] = 0x1234U;
    }
    const unsigned int presents_before_panic = test_platform_display_present_calls();
    if (!console_write_panic("KERNEL PANIC: fullscreen") || console_graphics_active() || !terminal.rendering_enabled ||
        terminal.cursor_visible || test_platform_display_present_calls() != presents_before_panic + 1U ||
        framebuffer->pixels[0] == 0x1234U) {
        return 1;
    }
    if (terminal.cells[0].character != 'K' || terminal.cells[1].character != 'E' ||
        terminal.cells[2].character != 'R' || terminal.cells[3].character != 'N' ||
        terminal.cells[4].character != 'E' || terminal.cells[5].character != 'L') {
        return 1;
    }
    console_set_graphics_active(true);
    console_set_graphics_active(false);
    if (console_graphics_active() ||
        console_next_deadline() != test_platform_time_ms() + TABOS_CURSOR_BLINK_INTERVAL_MS) {
        return 1;
    }
    free(graphics_snapshot);

    if (!tabos_console_write(&foreground, "\033[31;44;7m\033[?25l\033[")) {
        return 1;
    }
    tabos_console_release(&foreground);
    if (console_next_deadline() != UINT64_MAX || tabos_console_is_foreground(&foreground) ||
        tabos_console_write(&foreground, "stale") || !tabos_console_acquire(&background) ||
        !tabos_console_clear(&background) || !tabos_console_get_cursor(&background, &column, &row) || column != 0U ||
        row != 0U) {
        return 1;
    }

    if (!terminal.cursor_visible || terminal.foreground != 0xffff || terminal.background != 0U || terminal.reverse ||
        terminal.ansi_state != 0U) {
        return 1;
    }
    const size_t generated_lines = terminal.line_capacity + 10U;
    char* history                = malloc((generated_lines * 2U) + 1U);
    if (history == NULL) {
        return 1;
    }
    for (size_t index = 0U; index < generated_lines; ++index) {
        history[index * 2U]        = 'x';
        history[(index * 2U) + 1U] = '\n';
    }
    history[generated_lines * 2U] = '\0';
    const bool history_written    = tabos_console_write(&background, history);
    free(history);
    size_t history_lines = 0U;
    if (!history_written || !tabos_console_get_history_line_count(&background, &history_lines) ||
        history_lines != terminal.line_capacity || terminal.first_line == 0U) {
        return 1;
    }
    if (!tabos_console_get_cursor(&background, &column, &row) || row != terminal.rows - 1U) {
        return 1;
    }
    if (!tabos_console_page_up(&background) || tabos_console_is_at_end(&background) ||
        !tabos_console_page_down(&background) || !tabos_console_scroll_to_start(&background) ||
        terminal.viewport_top != terminal.first_line || !tabos_console_scroll_to_end(&background) ||
        !tabos_console_is_at_end(&background)) {
        return 1;
    }
    if (!tabos_console_page_up(&background) || !tabos_console_write(&background, "live") ||
        !tabos_console_is_at_end(&background)) {
        return 1;
    }

    if (!tabos_console_clear(&background) || !tabos_console_write(&background, "SCALE") ||
        console_resize(display_framebuffer(), 4U) != CONSOLE_RESIZE_OK || terminal.scale != 4U ||
        terminal.column != 5U || terminal.cells[0].character != 'S' || terminal.cells[4].character != 'E') {
        return 1;
    }

    concurrent_write_context_t context = {
        .session = &background,
    };
    pthread_t writer;
    if (pthread_create(&writer, NULL, write_during_resize, &context) != 0) {
        return 1;
    }
    atomic_store_explicit(&context.start, true, memory_order_release);
    for (size_t iteration = 0U; iteration < 32U; ++iteration) {
        const unsigned int scale = iteration % 2U == 0U ? 2U : 4U;
        if (console_resize(display_framebuffer(), scale) != CONSOLE_RESIZE_OK) {
            return 1;
        }
    }
    if (pthread_join(writer, NULL) != 0 || atomic_load_explicit(&context.failed, memory_order_acquire) ||
        terminal.scale != 4U || terminal.cells == NULL || terminal.dirty_cells == NULL) {
        return 1;
    }

    tabos_console_release(&background);
    console_shutdown();
    terminal_shutdown(&terminal);
    display_shutdown();
    input_shutdown();
    return 0;
}
