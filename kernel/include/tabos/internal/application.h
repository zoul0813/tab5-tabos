#ifndef TABOS_INTERNAL_APPLICATION_H
#define TABOS_INTERNAL_APPLICATION_H

#include <stdbool.h>

#include <tabos/application.h>

struct tabos_app_context {
        const tabos_app_descriptor_t* descriptor;
        tabos_process_id_t process_id;
        tabos_console_session_t console;
        bool console_owned;
        bool exit_requested;
        int exit_status;
        tabos_process_termination_t termination_cause;
        bool child_status_valid;
        int child_status;
        void* application_data;
};

void kernel_application_system_init(void);
void kernel_application_system_update(void);
bool kernel_application_system_runnable(void);
uint64_t kernel_application_system_next_deadline(void);
void kernel_application_system_shutdown(void);

typedef enum {
    APPLICATION_POWER_ACTIVE,
    APPLICATION_POWER_PARKING,
    APPLICATION_POWER_PARKED,
    APPLICATION_POWER_TIMEOUT,
    APPLICATION_POWER_LIFECYCLE
} application_power_state_t;

typedef struct {
        application_power_state_t state;
        tabos_process_id_t blocker;
        uint64_t deadline_ms;
} application_power_status_t;

/* Runtime-dispatcher only. This freezes application admission, not services,
 * and does not enable platform sleep. Status is copied, never a process pointer. */
bool kernel_application_power_begin(uint64_t now_ms);
void kernel_application_power_update(uint64_t now_ms);
void kernel_application_power_end(void);
application_power_status_t kernel_application_power_status(void);

bool application_registry_register(const tabos_app_descriptor_t* descriptor);
void application_registry_reset(void);
void application_report_diagnostic_result(tabos_app_context_t* context, int status);
tabos_app_result_t kernel_process_launch_child(tabos_app_context_t* parent, const char* name);
void kernel_process_fail(tabos_app_context_t* context, tabos_process_termination_t cause, int exit_status);
bool kernel_process_force_terminate(tabos_process_id_t process_id, int exit_status);

#endif
