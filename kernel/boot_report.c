#include <tabos/internal/boot_report.h>

#include <tabos/platform/platform.h>

#include <stdio.h>

static const char* status_name(kernel_boot_status_t status)
{
    switch (status) {
        case KERNEL_BOOT_STATUS_OK: return "OK";
        case KERNEL_BOOT_STATUS_WARNING: return "WARN";
        case KERNEL_BOOT_STATUS_ERROR: return "ERROR";
        case KERNEL_BOOT_STATUS_INFO:
        default: return "INFO";
    }
}

static platform_pixel_t status_color(kernel_boot_status_t status)
{
    switch (status) {
        case KERNEL_BOOT_STATUS_OK: return 0x07e0;
        case KERNEL_BOOT_STATUS_WARNING: return 0xffe0;
        case KERNEL_BOOT_STATUS_ERROR: return 0xf800;
        case KERNEL_BOOT_STATUS_INFO:
        default: return 0x07ff;
    }
}

void kernel_boot_report_init(kernel_boot_report_t* report, const char* system_name, const char* version)
{
    if (report != NULL) {
        *report = (kernel_boot_report_t) {.system_name = system_name, .version = version};
    }
}

bool kernel_boot_report_add(kernel_boot_report_t* report, const char* component, const char* detail,
                            kernel_boot_status_t status)
{
    if (report == NULL || component == NULL || detail == NULL ||
        report->entry_count >= KERNEL_BOOT_REPORT_MAX_ENTRIES) {
        return false;
    }
    report->entries[report->entry_count++] = (kernel_boot_entry_t) {
        .component = component,
        .detail    = detail,
        .status    = status,
    };
    return true;
}

void kernel_boot_report_write_serial(const kernel_boot_report_t* report)
{
    if (report == NULL) {
        return;
    }
    char line[640];
    (void) snprintf(line, sizeof(line), "%s %s boot report", report->system_name, report->version);
    platform_log(line);
    for (size_t index = 0; index < report->entry_count; ++index) {
        const kernel_boot_entry_t* entry = &report->entries[index];
        (void) snprintf(line, sizeof(line), "[%s] %s: %s", status_name(entry->status), entry->component, entry->detail);
        platform_log(line);
    }
}

void kernel_boot_report_write_terminal(const kernel_boot_report_t* report, terminal_t* terminal)
{
    if (report == NULL || terminal == NULL) {
        return;
    }
    terminal_set_colors(terminal, 0xffff, 0x0000);
    terminal_write(terminal, report->system_name);
    terminal_write(terminal, " ");
    terminal_write(terminal, report->version);
    terminal_write_line(terminal, " boot report");
    terminal_write_line(terminal, "");
    for (size_t index = 0; index < report->entry_count; ++index) {
        const kernel_boot_entry_t* entry = &report->entries[index];
        terminal_set_colors(terminal, status_color(entry->status), 0x0000);
        terminal_write(terminal, "[");
        terminal_write(terminal, status_name(entry->status));
        terminal_write(terminal, "] ");
        terminal_set_colors(terminal, 0xffff, 0x0000);
        terminal_write(terminal, entry->component);
        terminal_write(terminal, ": ");
        terminal_write_line(terminal, entry->detail);
    }
}
