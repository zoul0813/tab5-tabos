#ifndef TABOS_INTERNAL_HARDWARE_DEVICES_H
#define TABOS_INTERNAL_HARDWARE_DEVICES_H

#include <stdbool.h>
#include <stdint.h>

bool hardware_devices_init(void);
void hardware_devices_update(void);
uint64_t hardware_devices_next_deadline(void);
void hardware_devices_shutdown(void);

#endif
