#ifndef TABOS_INTERNAL_BOOT_REPORT_H
#define TABOS_INTERNAL_BOOT_REPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/internal/terminal.h>

typedef enum {
    TAB_BOOT_STATUS_INFO,
    TAB_BOOT_STATUS_OK,
    TAB_BOOT_STATUS_WARNING,
    TAB_BOOT_STATUS_ERROR,
} tab_boot_status_t;

typedef struct {
    const char *component;
    const char *detail;
    tab_boot_status_t status;
} tab_boot_entry_t;

enum { TAB_BOOT_REPORT_MAX_ENTRIES = 12 };

typedef struct {
    const char *system_name;
    const char *version;
    tab_boot_entry_t entries[TAB_BOOT_REPORT_MAX_ENTRIES];
    size_t entry_count;
} tab_boot_report_t;

void tab_boot_report_init(tab_boot_report_t *report, const char *system_name, const char *version);
bool tab_boot_report_add(tab_boot_report_t *report, const char *component, const char *detail,
                         tab_boot_status_t status);
void tab_boot_report_write_serial(const tab_boot_report_t *report);
void tab_boot_report_write_terminal(const tab_boot_report_t *report, tab_terminal_t *terminal);

#endif
