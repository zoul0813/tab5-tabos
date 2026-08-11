#include <tabos/internal/boot_report.h>

#include <tabos/platform/platform.h>

#include <stdio.h>

static const char *status_name(tab_boot_status_t status)
{
    switch (status) {
    case TAB_BOOT_STATUS_OK:
        return "OK";
    case TAB_BOOT_STATUS_WARNING:
        return "WARN";
    case TAB_BOOT_STATUS_ERROR:
        return "ERROR";
    case TAB_BOOT_STATUS_INFO:
    default:
        return "INFO";
    }
}

static tab_pixel_t status_color(tab_boot_status_t status)
{
    switch (status) {
    case TAB_BOOT_STATUS_OK:
        return 0x07e0;
    case TAB_BOOT_STATUS_WARNING:
        return 0xffe0;
    case TAB_BOOT_STATUS_ERROR:
        return 0xf800;
    case TAB_BOOT_STATUS_INFO:
    default:
        return 0x07ff;
    }
}

void tab_boot_report_init(tab_boot_report_t *report, const char *system_name, const char *version)
{
    if (report != NULL) {
        *report = (tab_boot_report_t){.system_name = system_name, .version = version};
    }
}

bool tab_boot_report_add(tab_boot_report_t *report, const char *component, const char *detail,
                         tab_boot_status_t status)
{
    if (report == NULL || component == NULL || detail == NULL ||
        report->entry_count >= TAB_BOOT_REPORT_MAX_ENTRIES) {
        return false;
    }
    report->entries[report->entry_count++] = (tab_boot_entry_t){
        .component = component,
        .detail = detail,
        .status = status,
    };
    return true;
}

void tab_boot_report_write_serial(const tab_boot_report_t *report)
{
    if (report == NULL) {
        return;
    }
    char line[160];
    (void)snprintf(line, sizeof(line), "%s %s boot report", report->system_name, report->version);
    tab_platform_log(line);
    for (size_t index = 0; index < report->entry_count; ++index) {
        const tab_boot_entry_t *entry = &report->entries[index];
        (void)snprintf(line, sizeof(line), "[%s] %s: %s", status_name(entry->status),
                       entry->component, entry->detail);
        tab_platform_log(line);
    }
}

void tab_boot_report_write_terminal(const tab_boot_report_t *report, tab_terminal_t *terminal)
{
    if (report == NULL || terminal == NULL) {
        return;
    }
    tab_terminal_set_colors(terminal, 0xffff, 0x0000);
    tab_terminal_write(terminal, report->system_name);
    tab_terminal_write(terminal, " ");
    tab_terminal_write(terminal, report->version);
    tab_terminal_write_line(terminal, " boot report");
    tab_terminal_write_line(terminal, "");
    for (size_t index = 0; index < report->entry_count; ++index) {
        const tab_boot_entry_t *entry = &report->entries[index];
        tab_terminal_set_colors(terminal, status_color(entry->status), 0x0000);
        tab_terminal_write(terminal, "[");
        tab_terminal_write(terminal, status_name(entry->status));
        tab_terminal_write(terminal, "] ");
        tab_terminal_set_colors(terminal, 0xffff, 0x0000);
        tab_terminal_write(terminal, entry->component);
        tab_terminal_write(terminal, ": ");
        tab_terminal_write_line(terminal, entry->detail);
    }
}
