#ifndef TABOS_INTERNAL_BOOT_REPORT_H
#define TABOS_INTERNAL_BOOT_REPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/internal/terminal.h>

typedef enum {
    KERNEL_BOOT_STATUS_INFO,
    KERNEL_BOOT_STATUS_OK,
    KERNEL_BOOT_STATUS_WARNING,
    KERNEL_BOOT_STATUS_ERROR,
} kernel_boot_status_t;

typedef struct {
        const char* component;
        const char* detail;
        kernel_boot_status_t status;
} kernel_boot_entry_t;

enum {
    KERNEL_BOOT_REPORT_MAX_ENTRIES = 12
};

typedef struct {
        const char* system_name;
        const char* version;
        kernel_boot_entry_t entries[KERNEL_BOOT_REPORT_MAX_ENTRIES];
        size_t entry_count;
} kernel_boot_report_t;

void kernel_boot_report_init(kernel_boot_report_t* report, const char* system_name, const char* version);
bool kernel_boot_report_add(kernel_boot_report_t* report, const char* component, const char* detail,
                            kernel_boot_status_t status);
void kernel_boot_report_write_serial(const kernel_boot_report_t* report);
void kernel_boot_report_write_terminal(const kernel_boot_report_t* report, terminal_t* terminal);

#endif
