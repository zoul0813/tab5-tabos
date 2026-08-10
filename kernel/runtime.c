#include <tabos/internal/runtime.h>

#include <tabos/internal/boot_report.h>
#include <tabos/internal/display.h>
#include <tabos/internal/terminal.h>

#include <tabos/terminal.h>

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>
#include <tabos/config/display.h>

#include <inttypes.h>
#include <stdio.h>

static bool runtime_initialized;
static bool runtime_started;
static unsigned int terminal_scale = TABOS_TERMINAL_SCALE;
static tab_terminal_t terminal;
static tab_boot_report_t boot_report;
static char processor_detail[80];
static char memory_detail[80];
static char external_memory_detail[80];
static char flash_detail[48];
static char storage_detail[80];

static void format_size(char *buffer, size_t buffer_size, uint64_t bytes)
{
    static const uint64_t mebibyte = 1024U * 1024U;
    static const uint64_t kibibyte = 1024U;

    if (bytes >= mebibyte) {
        const uint64_t tenths = ((bytes % mebibyte) * 10U) / mebibyte;
        (void)snprintf(buffer, buffer_size, "%" PRIu64 ".%" PRIu64 " MIB",
                       bytes / mebibyte, tenths);
    } else {
        (void)snprintf(buffer, buffer_size, "%" PRIu64 " KIB", bytes / kibibyte);
    }
}

static void format_capacity(char *buffer, size_t buffer_size, uint64_t free_bytes,
                            uint64_t total_bytes, bool free_known)
{
    char total[24];
    format_size(total, sizeof(total), total_bytes);
    if (!free_known) {
        (void)snprintf(buffer, buffer_size, "%s TOTAL; FREE UNKNOWN", total);
        return;
    }

    char free_space[24];
    format_size(free_space, sizeof(free_space), free_bytes);
    (void)snprintf(buffer, buffer_size, "%s FREE / %s TOTAL", free_space, total);
}

static bool render_boot_report(void)
{
    tab_framebuffer_t *framebuffer = tab_display_framebuffer();
    if (framebuffer == NULL || !tab_terminal_init(&terminal, framebuffer, terminal_scale)) {
        return false;
    }

    tab_terminal_clear(&terminal);
    tab_boot_report_write_terminal(&boot_report, &terminal);
    return tab_display_present();
}

bool tabos_runtime_init(void)
{
    if (runtime_initialized) {
        return true;
    }

    runtime_initialized = true;
    return true;
}

bool tabos_runtime_start(void)
{
    if (!runtime_initialized) {
        return false;
    }

    if (runtime_started) {
        return true;
    }

    if (!tab_display_init()) {
        return false;
    }

    tab_platform_diagnostics_t diagnostics;

    tab_boot_report_init(&boot_report, TABOS_SYSTEM_NAME, TABOS_RUNTIME_VERSION);
    (void)tab_boot_report_add(&boot_report, "TARGET", tab_platform_name(), TAB_BOOT_STATUS_OK);
    (void)tab_boot_report_add(&boot_report, "DISPLAY", tab_platform_display_name(),
                              TAB_BOOT_STATUS_OK);
    (void)tab_boot_report_add(&boot_report, "FRAMEBUFFER", "1280X720 RGB565",
                              TAB_BOOT_STATUS_OK);

    if (tab_platform_get_diagnostics(&diagnostics)) {
        if (diagnostics.cpu_frequency_mhz > 0U) {
            (void)snprintf(processor_detail, sizeof(processor_detail), "%s; %u CORES @ %u MHZ",
                           diagnostics.device_name, diagnostics.cpu_cores,
                           diagnostics.cpu_frequency_mhz);
        } else {
            (void)snprintf(processor_detail, sizeof(processor_detail), "%s; %u CORES",
                           diagnostics.device_name, diagnostics.cpu_cores);
        }
        (void)tab_boot_report_add(&boot_report, "PROCESSOR", processor_detail,
                                  TAB_BOOT_STATUS_OK);

        if (diagnostics.memory_total_bytes > 0U) {
            format_capacity(memory_detail, sizeof(memory_detail), diagnostics.memory_free_bytes,
                            diagnostics.memory_total_bytes, diagnostics.memory_free_known);
            (void)tab_boot_report_add(&boot_report, "MEMORY", memory_detail, TAB_BOOT_STATUS_OK);
        }
        if (diagnostics.external_memory_present) {
            format_capacity(external_memory_detail, sizeof(external_memory_detail),
                            diagnostics.external_memory_free_bytes,
                            diagnostics.external_memory_total_bytes, true);
            (void)tab_boot_report_add(&boot_report, "PSRAM", external_memory_detail,
                                      TAB_BOOT_STATUS_OK);
        }
        if (diagnostics.flash_capacity_bytes > 0U) {
            format_size(flash_detail, sizeof(flash_detail), diagnostics.flash_capacity_bytes);
            (void)tab_boot_report_add(&boot_report, "FLASH", flash_detail,
                                      TAB_BOOT_STATUS_INFO);
        }
        if (diagnostics.storage_mounted) {
            format_capacity(storage_detail, sizeof(storage_detail), diagnostics.storage_free_bytes,
                            diagnostics.storage_total_bytes, true);
            (void)tab_boot_report_add(&boot_report, "STORAGE", storage_detail,
                                      TAB_BOOT_STATUS_OK);
        } else {
            (void)tab_boot_report_add(&boot_report, "STORAGE", "FILESYSTEM NOT MOUNTED",
                                      TAB_BOOT_STATUS_WARNING);
        }
    }
    (void)tab_boot_report_add(&boot_report, "KERNEL", "RUNTIME INITIALIZED",
                              TAB_BOOT_STATUS_OK);

    tab_boot_report_write_serial(&boot_report);
    if (!render_boot_report()) {
        tab_display_shutdown();
        return false;
    }

    runtime_started = true;
    return true;
}

void tabos_runtime_shutdown(void)
{
    if (runtime_started) {
        tab_display_shutdown();
        runtime_started = false;
        terminal = (tab_terminal_t){0};
        boot_report = (tab_boot_report_t){0};
    }

    runtime_initialized = false;
}

const char *tabos_runtime_version(void)
{
    return TABOS_RUNTIME_VERSION;
}

bool tabos_terminal_set_scale(unsigned int scale)
{
    if (scale < TABOS_TERMINAL_SCALE_MIN || scale > TABOS_TERMINAL_SCALE_MAX) {
        return false;
    }
    if (scale == terminal_scale) {
        return true;
    }

    const unsigned int previous_scale = terminal_scale;
    terminal_scale = scale;
    if (!runtime_started || render_boot_report()) {
        return true;
    }

    terminal_scale = previous_scale;
    (void)render_boot_report();
    return false;
}

unsigned int tabos_terminal_get_scale(void)
{
    return terminal_scale;
}
