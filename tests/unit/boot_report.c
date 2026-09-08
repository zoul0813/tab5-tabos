#include <tabos/internal/boot_report.h>

#include "platform_test.h"

#include <stdlib.h>
#include <string.h>

static bool terminal_contains(const terminal_t* terminal, const char* text)
{
    const size_t length = strlen(text);
    size_t matched      = 0U;
    const size_t cells  = terminal->line_capacity * terminal->columns;
    for (size_t index = 0U; index < cells; ++index) {
        if (terminal->cells[index].character == text[matched]) {
            if (++matched == length) {
                return true;
            }
        } else {
            matched = terminal->cells[index].character == text[0] ? 1U : 0U;
        }
    }
    return length == 0U;
}

int main(void)
{
    kernel_boot_report_t report;
    char component[32] = "Display";
    char detail[32]    = "distinct-display-driver";
    kernel_boot_report_init(&report, "TabOS", "test");
    if (!kernel_boot_report_add(&report, component, detail, KERNEL_BOOT_STATUS_OK)) {
        return 1;
    }

    (void) strcpy(component, "Audio");
    (void) strcpy(detail, "distinct-audio-driver");
    test_platform_clear_log();
    kernel_boot_report_write_serial(&report);
    if (strcmp(test_platform_last_log(), "[OK] Display: distinct-display-driver") != 0 ||
        !kernel_boot_report_add(&report, component, detail, KERNEL_BOOT_STATUS_OK)) {
        return 1;
    }

    (void) strcpy(component, "Reused");
    (void) strcpy(detail, "overwritten-driver");
    platform_pixel_t* pixels = calloc(640U * 240U, sizeof(*pixels));
    if (pixels == NULL) {
        return 1;
    }
    platform_framebuffer_t framebuffer = {
        .pixels        = pixels,
        .width         = 640U,
        .height        = 240U,
        .stride_pixels = 640U,
    };
    terminal_t terminal;
    const bool initialized = terminal_init(&terminal, &framebuffer, 1U);
    if (initialized) {
        kernel_boot_report_write_terminal(&report, &terminal);
    }
    const bool valid = initialized && terminal_contains(&terminal, "Display: distinct-display-driver") &&
                       terminal_contains(&terminal, "Audio: distinct-audio-driver");
    if (initialized) {
        terminal_shutdown(&terminal);
    }
    free(pixels);
    return valid ? 0 : 1;
}
