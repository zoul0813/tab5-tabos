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
};

void tab_app_system_init(void);
void tab_app_system_update(void);
void tab_app_system_shutdown(void);

bool tab_app_registry_register(const tabos_app_descriptor_t *descriptor);
void tab_app_registry_reset(void);
void tab_app_report_diagnostic_result(tabos_app_context_t *context, int status);

#endif
