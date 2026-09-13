#ifndef TABOS_INTERNAL_RUNTIME_H
#define TABOS_INTERNAL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include <tabos/platform/platform.h>
#include <tabos/internal/power.h>

bool kernel_runtime_init(void);
bool kernel_runtime_start(bool launch_startup_application);
void kernel_runtime_update(platform_runtime_events_t events);
uint64_t kernel_runtime_next_deadline(void);
void kernel_runtime_shutdown(void);
/* Internal runtime-owner integration surface; no application/SDK transport. */
bool kernel_runtime_request_suspend(void);
const power_status_t* kernel_runtime_power_status(void);
bool kernel_runtime_request_system_action(platform_system_action_t action);
bool kernel_runtime_system_action_pending(void);
platform_system_action_t kernel_runtime_take_system_action(void);
const char* kernel_runtime_version(void);

#endif
