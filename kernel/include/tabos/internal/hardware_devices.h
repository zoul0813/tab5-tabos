#ifndef TABOS_INTERNAL_HARDWARE_DEVICES_H
#define TABOS_INTERNAL_HARDWARE_DEVICES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HARDWARE_DEVICE_HEALTH_RTC,
    HARDWARE_DEVICE_HEALTH_BATTERY,
} hardware_device_health_t;

bool hardware_devices_init(void);
void hardware_devices_update(void);
void hardware_devices_health_changed(hardware_device_health_t health);
uint64_t hardware_devices_next_deadline(void);
void hardware_devices_suspend_audit(void);
void hardware_devices_resume_audit(void);
void hardware_devices_shutdown(void);

#endif
