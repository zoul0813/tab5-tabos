#include <tabos/internal/runtime.h>

#include <tabos/internal/application.h>
#include <tabos/internal/boot_report.h>
#include <tabos/internal/diagnostic_apps.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/input.h>
#include <tabos/internal/terminal.h>

#include <tabos/terminal.h>
#include <tabos/filesystem.h>

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>
#include <tabos/config/loader.h>
#include <tabos/config/display.h>
#include <tabos/config/console.h>

#include <inttypes.h>
#include <stdio.h>

static bool runtime_initialized;
static bool runtime_started;
static unsigned int terminal_scale = TABOS_TERMINAL_SCALE;
static terminal_t terminal;
static kernel_boot_report_t boot_report;
static char processor_detail[80];
static char memory_detail[80];
static char external_memory_detail[80];
static char flash_detail[48];
static char storage_detail[512];

static void format_size(char* buffer, size_t buffer_size, uint64_t bytes)
{
    static const uint64_t mebibyte = 1024U * 1024U;
    static const uint64_t kibibyte = 1024U;

    if (bytes >= mebibyte) {
        const uint64_t tenths = ((bytes % mebibyte) * 10U) / mebibyte;
        (void) snprintf(buffer, buffer_size, "%" PRIu64 ".%" PRIu64 " MiB", bytes / mebibyte, tenths);
    } else {
        (void) snprintf(buffer, buffer_size, "%" PRIu64 " KiB", bytes / kibibyte);
    }
}

static void format_capacity(char* buffer, size_t buffer_size, uint64_t free_bytes, uint64_t total_bytes,
                            bool free_known)
{
    char total[24];
    format_size(total, sizeof(total), total_bytes);
    if (!free_known) {
        (void) snprintf(buffer, buffer_size, "%s total; free unknown", total);
        return;
    }

    char free_space[24];
    format_size(free_space, sizeof(free_space), free_bytes);
    (void) snprintf(buffer, buffer_size, "%s free / %s total", free_space, total);
}

static bool add_storage_report(void)
{
    const size_t drive_count = tabos_fs_drive_count();
    if (drive_count == 0U) {
        return kernel_boot_report_add(&boot_report, "Storage", "\n  No drives available", KERNEL_BOOT_STATUS_WARNING);
    }

    size_t used       = 0U;
    size_t listed     = 0U;
    storage_detail[0] = '\0';
    for (size_t index = 0U; index < drive_count; ++index) {
        tabos_drive_info_t drive;
        if (!tabos_fs_drive_info(index, &drive)) {
            continue;
        }
        char capacity[64];
        format_capacity(capacity, sizeof(capacity), drive.free_bytes, drive.total_bytes, true);
        const int written = snprintf(storage_detail + used, sizeof(storage_detail) - used, "\n  %c:/ %s %s",
                                     drive.letter, drive.name != NULL ? drive.name : "Filesystem", capacity);
        if (written < 0 || (size_t) written >= sizeof(storage_detail) - used) {
            break;
        }
        used += (size_t) written;
        listed++;
    }
    if (listed == 0U) {
        return kernel_boot_report_add(&boot_report, "Storage", "\n  No drives available", KERNEL_BOOT_STATUS_WARNING);
    }
    return kernel_boot_report_add(&boot_report, "Storage", storage_detail, KERNEL_BOOT_STATUS_OK);
}

static bool render_boot_report(void)
{
    platform_framebuffer_t* framebuffer = display_framebuffer();
    if (framebuffer == NULL || !terminal_init(&terminal, framebuffer, terminal_scale)) {
        return false;
    }

    terminal_clear(&terminal);
    kernel_boot_report_write_terminal(&boot_report, &terminal);
    if (!display_present()) {
        terminal_shutdown(&terminal);
        return false;
    }
    return true;
}

bool kernel_runtime_init(void)
{
    if (runtime_initialized) {
        return true;
    }

    runtime_initialized = true;
    input_init();
    return true;
}

bool kernel_runtime_start(void)
{
    if (!runtime_initialized) {
        return false;
    }

    if (runtime_started) {
        return true;
    }

    if (!filesystem_init()) {
        return false;
    }

    if (!display_init()) {
        return false;
    }

    platform_diagnostics_t diagnostics;

    kernel_boot_report_init(&boot_report, TABOS_SYSTEM_NAME, TABOS_RUNTIME_VERSION);
    (void) kernel_boot_report_add(&boot_report, "Target", platform_name(), KERNEL_BOOT_STATUS_OK);
    (void) kernel_boot_report_add(&boot_report, "Display", platform_display_name(), KERNEL_BOOT_STATUS_OK);
    (void) kernel_boot_report_add(&boot_report, "Framebuffer", "1280x720 RGB565", KERNEL_BOOT_STATUS_OK);

    if (platform_get_diagnostics(&diagnostics)) {
        if (diagnostics.cpu_frequency_mhz > 0U) {
            (void) snprintf(processor_detail, sizeof(processor_detail), "%s; %u cores @ %u MHz",
                            diagnostics.device_name, diagnostics.cpu_cores, diagnostics.cpu_frequency_mhz);
        } else {
            (void) snprintf(processor_detail, sizeof(processor_detail), "%s; %u cores", diagnostics.device_name,
                            diagnostics.cpu_cores);
        }
        (void) kernel_boot_report_add(&boot_report, "Processor", processor_detail, KERNEL_BOOT_STATUS_OK);

        if (diagnostics.memory_total_bytes > 0U) {
            format_capacity(memory_detail, sizeof(memory_detail), diagnostics.memory_free_bytes,
                            diagnostics.memory_total_bytes, diagnostics.memory_free_known);
            (void) kernel_boot_report_add(&boot_report, "Memory", memory_detail, KERNEL_BOOT_STATUS_OK);
        }
        if (diagnostics.external_memory_present) {
            format_capacity(external_memory_detail, sizeof(external_memory_detail),
                            diagnostics.external_memory_free_bytes, diagnostics.external_memory_total_bytes, true);
            (void) kernel_boot_report_add(&boot_report, "PSRAM", external_memory_detail, KERNEL_BOOT_STATUS_OK);
        }
        if (diagnostics.flash_capacity_bytes > 0U) {
            format_size(flash_detail, sizeof(flash_detail), diagnostics.flash_capacity_bytes);
            (void) kernel_boot_report_add(&boot_report, "Flash", flash_detail, KERNEL_BOOT_STATUS_INFO);
        }
        if (diagnostics.keyboard_name != NULL) {
            (void) kernel_boot_report_add(&boot_report, "Keyboard", diagnostics.keyboard_name,
                                          diagnostics.keyboard_present ? KERNEL_BOOT_STATUS_OK :
                                                                         KERNEL_BOOT_STATUS_WARNING);
        }
    }
    (void) add_storage_report();
    (void) kernel_boot_report_add(&boot_report, "Kernel", "Runtime initialized", KERNEL_BOOT_STATUS_OK);

    kernel_boot_report_write_serial(&boot_report);
    if (!render_boot_report()) {
        display_shutdown();
        return false;
    }

    if (!console_init(&terminal)) {
        terminal_shutdown(&terminal);
        display_shutdown();
        return false;
    }
    kernel_application_system_init();
    if (!diagnostic_apps_register()) {
        kernel_application_system_shutdown();
        console_shutdown();
        terminal_shutdown(&terminal);
        display_shutdown();
        return false;
    }
    runtime_started = true;
#if TABOS_ENABLE_SHELL_STARTUP
    const tabos_app_result_t startup_result = tabos_app_launch_path(TABOS_SHELL_PATH);
#else
    const char* startup_app                 = diagnostic_startup_app();
    const tabos_app_result_t startup_result = startup_app != NULL ? tabos_app_launch(startup_app) : TABOS_APP_RESULT_OK;
#endif
    if (startup_result != TABOS_APP_RESULT_OK) {
        kernel_application_system_shutdown();
        console_shutdown();
        terminal_shutdown(&terminal);
        runtime_started = false;
        display_shutdown();
        return false;
    }
    return true;
}

void kernel_runtime_update(void)
{
    if (!runtime_started) {
        return;
    }
    input_update();
    console_update();
    kernel_application_system_update();
}

void kernel_runtime_shutdown(void)
{
    if (runtime_started) {
        kernel_application_system_shutdown();
        console_shutdown();
        terminal_shutdown(&terminal);
        display_shutdown();
        runtime_started = false;
        boot_report     = (kernel_boot_report_t) {0};
    }

    filesystem_shutdown();
    runtime_initialized = false;
    input_shutdown();
}

const char* kernel_runtime_version(void)
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

    if (!runtime_started) {
        terminal_scale = scale;
        return true;
    }

    if (!terminal_resize(&terminal, display_framebuffer(), scale)) {
        return false;
    }
    terminal_scale = scale;
    console_rebind(&terminal);
    return display_present();
}

unsigned int tabos_terminal_get_scale(void)
{
    return terminal_scale;
}
