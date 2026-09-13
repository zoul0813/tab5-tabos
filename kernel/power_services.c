#include <tabos/internal/power_services.h>
#include <tabos/internal/application.h>
#include <tabos/internal/audio.h>
#include <tabos/internal/camera.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/hardware_devices.h>
#include <tabos/internal/input.h>
#include <tabos/internal/network.h>
#include <tabos/internal/pointer.h>

#include <stdio.h>

/* Dependency order: reverse for suspend, forward for resume. Shared buses and
 * controller wake setup remain below platform prepare, which Tab5 rejects. */
enum {
    SERVICE_DISPLAY,
    SERVICE_POINTER,
    SERVICE_KEYBOARD,
    SERVICE_STORAGE,
    SERVICE_NETWORK,
    SERVICE_CAMERA,
    SERVICE_AUDIO,
    SERVICE_HEALTH,
    SERVICE_APPLICATIONS
};
static const char* const names[POWER_SERVICE_COUNT] = {"display", "pointer", "keyboard", "storage",     "network",
                                                       "camera",  "audio",   "health",   "applications"};

static power_callback_result_t result_of(int result)
{
    return result == 0 ? POWER_CALLBACK_SUCCESS : POWER_CALLBACK_FAILURE;
}

static bool activity_pending(void)
{
    return input_power_activity_pending() || pointer_service_power_activity_pending();
}

static power_callback_result_t pending(power_service_t* service, power_completion_token_t token)
{
    service->owner->pending       = true;
    service->owner->pending_kind  = service->kind;
    service->owner->pending_token = token;
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
    return POWER_CALLBACK_PENDING;
}

static bool blocked(void* context, char* reason, size_t capacity)
{
    const power_service_t* service = context;
    if (service->kind == SERVICE_APPLICATIONS &&
        (tabos_process_system_panicked() || console_graphics_active() ||
         kernel_application_power_status().state == APPLICATION_POWER_PARKING ||
         kernel_application_power_status().state == APPLICATION_POWER_PARKED)) {
        (void) snprintf(reason, capacity, "application ownership");
        return true;
    }
    if (service->kind == SERVICE_STORAGE && filesystem_power_is_frozen()) {
        (void) snprintf(reason, capacity, "storage ownership");
        return true;
    }
    return false;
}

static power_callback_result_t suspend_service(void* context, power_completion_token_t token)
{
    power_service_t* service = context;
    const uint64_t now       = platform_time_ms();
    if (service->kind == SERVICE_APPLICATIONS) {
        service->entered = kernel_application_power_begin(now);
        return service->entered ? pending(service, token) : POWER_CALLBACK_FAILURE;
    }
    if (service->kind == SERVICE_STORAGE) {
        service->entered = filesystem_power_begin(now);
        return service->entered ? pending(service, token) : POWER_CALLBACK_FAILURE;
    }
    service->entered = true;
    switch (service->kind) {
        case SERVICE_HEALTH: hardware_devices_suspend_audit(); return POWER_CALLBACK_SUCCESS;
        case SERVICE_AUDIO: return result_of(audio_service_power_suspend());
        case SERVICE_CAMERA: return result_of(camera_service_power_suspend());
        case SERVICE_NETWORK: return result_of(network_service_power_suspend());
        case SERVICE_KEYBOARD: return result_of(input_power_suspend());
        case SERVICE_POINTER: return result_of(pointer_service_power_suspend());
        case SERVICE_DISPLAY: {
            power_status_t* status   = &service->owner->manager->status;
            const int result         = display_power_suspend();
            status->brightness_valid = false;
            status->panel_valid      = false;
            if (result == 0) {
                status->desired_brightness   = 0U;
                status->effective_brightness = 0U;
                status->panel_enabled        = false;
                status->brightness_valid     = true;
                status->panel_valid          = true;
            }
            return result_of(result);
        }
        default: return POWER_CALLBACK_FAILURE;
    }
}

static power_callback_result_t resume_service(void* context, power_completion_token_t token)
{
    power_service_t* service = context;
    if (!service->entered) {
        return POWER_CALLBACK_SUCCESS;
    }
    int result = 0;
    switch (service->kind) {
        case SERVICE_DISPLAY:
            result = display_power_resume();
            /* Backend restores its saved pre-suspend state. Reconcile policy
             * brightness before applications are unparked, not before display. */
            service->owner->manager->status.brightness_valid = false;
            service->owner->manager->status.panel_valid      = false;
            break;
        case SERVICE_POINTER: pointer_service_power_resume(); break;
        case SERVICE_KEYBOARD: input_power_resume(); break;
        case SERVICE_STORAGE:
            filesystem_power_abort();
            if (filesystem_power_is_frozen()) {
                return pending(service, token);
            }
            break;
        case SERVICE_NETWORK: result = network_service_power_resume(); break;
        case SERVICE_CAMERA: camera_service_power_resume(); break;
        case SERVICE_AUDIO: audio_service_power_resume(); break;
        case SERVICE_HEALTH: hardware_devices_resume_audit(); break;
        case SERVICE_APPLICATIONS:
            if (!service->owner->manager->shutdown_requested) {
                if (!power_manager_restore_active_display(service->owner->manager)) {
                    return POWER_CALLBACK_FAILURE;
                }
                kernel_application_power_end();
            }
            break;
        default: return POWER_CALLBACK_FAILURE;
    }
    if (result == 0) {
        service->entered = false;
    }
    return result_of(result);
}

bool power_services_register(power_services_t* services, power_manager_t* manager)
{
    if (services == NULL || manager == NULL) {
        return false;
    }
    *services                 = (power_services_t) {.manager = manager};
    manager->activity_pending = activity_pending;
    for (unsigned int index = 0U; index < POWER_SERVICE_COUNT; ++index) {
        services->services[index]                           = (power_service_t) {.owner = services, .kind = index};
        const char* dependencies[]                          = {index == 0U ? NULL : names[index - 1U]};
        const power_participant_registration_t registration = {.name             = names[index],
                                                               .dependencies     = dependencies,
                                                               .dependency_count = index == 0U ? 0U : 1U,
                                                               .blocked          = blocked,
                                                               .suspend          = suspend_service,
                                                               .resume           = resume_service,
                                                               .context          = &services->services[index]};
        if (!power_manager_register(manager, &registration)) {
            return false;
        }
    }
    return true;
}

bool power_services_transitioning(const power_services_t* services)
{
    const power_state_t state = services->manager->status.state;
    return state == POWER_STATE_SUSPENDING || state == POWER_STATE_SUSPENDED || state == POWER_STATE_RESUMING;
}

void power_services_update(power_services_t* services, platform_runtime_events_t events, uint64_t now_ms)
{
    if (services->pending) {
        power_callback_result_t result = POWER_CALLBACK_PENDING;
        if (services->pending_kind == SERVICE_APPLICATIONS) {
            kernel_application_power_update(now_ms);
            const application_power_state_t state = kernel_application_power_status().state;
            if (state != APPLICATION_POWER_PARKING) {
                result = state == APPLICATION_POWER_PARKED ? POWER_CALLBACK_SUCCESS : POWER_CALLBACK_FAILURE;
            }
        } else if (services->pending_kind == SERVICE_STORAGE) {
            filesystem_power_update(now_ms);
            const filesystem_power_state_t state = filesystem_power_status().state;
            if (services->pending_token.operation == POWER_OPERATION_RESUME) {
                if (!filesystem_power_is_frozen()) {
                    filesystem_power_abort();
                    services->services[SERVICE_STORAGE].entered = false;
                    result                                      = POWER_CALLBACK_SUCCESS;
                }
            } else if (state == FILESYSTEM_POWER_READY || state == FILESYSTEM_POWER_FAILED) {
                result = state == FILESYSTEM_POWER_READY ? POWER_CALLBACK_SUCCESS : POWER_CALLBACK_FAILURE;
            }
        }
        if (result != POWER_CALLBACK_PENDING) {
            services->pending = false;
            power_manager_complete(services->manager, services->pending_token, result);
        }
    }
    power_manager_update(services->manager, events, now_ms);
    if (services->manager->status.resume_failed && !services->panic_reported) {
        services->panic_reported = true;
        char message[128];
        (void) snprintf(message, sizeof(message), "KERNEL PANIC: power restore failed: %s; applications parked",
                        services->manager->status.failure.participant);
        platform_log(message);
        /* Only use framebuffer diagnostics if that dependency was restored. */
        if (!services->services[SERVICE_DISPLAY].entered &&
            services->manager->status.failure.code != POWER_FAILURE_PLATFORM_RESTORE) {
            (void) console_write_panic(message);
        }
    }
}

uint64_t power_services_next_deadline(const power_services_t* services)
{
    if (!services->pending || services->manager->status.resume_failed) {
        return PLATFORM_RUNTIME_DEADLINE_NONE;
    }
    if (services->pending_kind == SERVICE_APPLICATIONS) {
        if (kernel_application_system_runnable()) {
            return platform_time_ms();
        }
        return kernel_application_system_next_deadline();
    }
    return filesystem_power_next_deadline();
}

void power_services_shutdown(power_services_t* services)
{
    power_manager_begin_shutdown(services->manager, platform_time_ms());
    if (services->pending) {
        /* The platform run loop may already have dropped its notification target.
         * Join storage through its worker, never through runtime event waits. */
        if (services->pending_kind == SERVICE_STORAGE) {
            filesystem_power_finish_for_shutdown();
            services->services[SERVICE_STORAGE].entered = false;
        }
        services->pending = false;
        power_manager_complete(services->manager, services->pending_token,
                               services->pending_token.operation == POWER_OPERATION_RESUME ? POWER_CALLBACK_SUCCESS :
                                                                                             POWER_CALLBACK_FAILURE);
    }
    power_services_update(services, PLATFORM_RUNTIME_EVENT_POWER, platform_time_ms());
}
