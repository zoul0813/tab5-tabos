#ifndef TABOS_INTERNAL_POWER_SERVICES_H
#define TABOS_INTERNAL_POWER_SERVICES_H

#include <tabos/internal/power.h>

enum {
    POWER_SERVICE_COUNT = 9
};
typedef struct power_services power_services_t;
typedef struct {
        power_services_t* owner;
        unsigned int kind;
        bool entered;
} power_service_t;
struct power_services {
        power_manager_t* manager;
        power_service_t services[POWER_SERVICE_COUNT];
        power_completion_token_t pending_token;
        unsigned int pending_kind;
        bool pending;
        bool panic_reported;
};

/* Runtime-owner only. Registers retained services before manager finalization.
 * Platform prepare/entry/restore remain the manager's final dependency. No SDK
 * trigger or hardware sleep capability is enabled by registration. */
bool power_services_register(power_services_t* services, power_manager_t* manager);
void power_services_update(power_services_t* services, platform_runtime_events_t events, uint64_t now_ms);
uint64_t power_services_next_deadline(const power_services_t* services);
bool power_services_transitioning(const power_services_t* services);
/* Teardown may wait for borrowed storage work; never unpark application code. */
void power_services_shutdown(power_services_t* services);

#endif
