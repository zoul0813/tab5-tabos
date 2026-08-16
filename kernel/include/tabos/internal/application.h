#ifndef TABOS_INTERNAL_APPLICATION_H
#define TABOS_INTERNAL_APPLICATION_H

#include <stdbool.h>

#include <tabos/application.h>

struct tabos_app_context {
    const tabos_app_descriptor_t *descriptor;
    tabos_process_id_t process_id;
    tabos_console_session_t console;
    bool console_owned;
    bool exit_requested;
    int exit_status;
    bool child_status_valid;
    int child_status;
    void *application_data;
};

void kernel_application_system_init(void);
void kernel_application_system_update(void);
void kernel_application_system_shutdown(void);

bool application_registry_register(const tabos_app_descriptor_t *descriptor);
void application_registry_reset(void);
void application_report_diagnostic_result(tabos_app_context_t *context, int status);
tabos_app_result_t kernel_process_launch_child(tabos_app_context_t *parent,
                                               const char *name);

#endif
