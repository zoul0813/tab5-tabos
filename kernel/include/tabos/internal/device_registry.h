#ifndef TABOS_INTERNAL_DEVICE_REGISTRY_H
#define TABOS_INTERNAL_DEVICE_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#include <tabos/device.h>

enum {
    DEVICE_REGISTRY_CAPACITY = 32,
};

typedef struct {
        const char* name;
        const char* driver;
        tabos_device_class_t device_class;
        tabos_device_state_t state;
        tabos_device_features_t features;
        int32_t last_error;
} device_registry_registration_t;

bool device_registry_init(void);
void device_registry_shutdown(void);
tabos_device_id_t device_registry_register(const device_registry_registration_t* registration);
bool device_registry_remove(tabos_device_id_t id);
bool device_registry_set_state(tabos_device_id_t id, tabos_device_state_t state, int32_t last_error);
size_t device_registry_count(void);
bool device_registry_at(size_t index, tabos_device_info_t* info);
bool device_registry_get(tabos_device_id_t id, tabos_device_info_t* info);
bool device_registry_find(const char* name, tabos_device_info_t* info);

#endif
