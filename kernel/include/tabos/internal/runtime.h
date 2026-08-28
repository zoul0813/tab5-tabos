#ifndef TABOS_INTERNAL_RUNTIME_H
#define TABOS_INTERNAL_RUNTIME_H

#include <stdbool.h>

#include <tabos/platform/platform.h>

bool kernel_runtime_init(void);
bool kernel_runtime_start(void);
void kernel_runtime_update(void);
void kernel_runtime_shutdown(void);
bool kernel_runtime_request_system_action(platform_system_action_t action);
platform_system_action_t kernel_runtime_take_system_action(void);
const char* kernel_runtime_version(void);

#endif
