#include <tabos/internal/hardware_devices.h>

#include <tabos/device.h>
#include <tabos/filesystem.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/network.h>
#include <tabos/platform/platform.h>

#include <errno.h>

static tabos_device_id_t network_device = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t rtc_device     = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t battery_device = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t registered_devices[6];
static size_t registered_device_count;
static bool initialized;

static bool register_device(const char* name, const char* driver, tabos_device_class_t device_class,
                            tabos_device_state_t state, tabos_device_features_t features, int32_t last_error,
                            tabos_device_id_t* id)
{
    const device_registry_registration_t registration = {
        .name         = name,
        .driver       = driver,
        .device_class = device_class,
        .state        = state,
        .features     = features,
        .last_error   = last_error,
    };
    const tabos_device_id_t registered = device_registry_register(&registration);
    if (registered == TABOS_DEVICE_ID_INVALID) {
        return false;
    }
    if (id != NULL) {
        *id = registered;
    }
    registered_devices[registered_device_count++] = registered;
    return true;
}

static bool register_storage(void)
{
    tabos_drive_info_t selected;
    bool found         = false;
    const size_t count = tabos_fs_drive_count();
    for (size_t index = 0U; index < count; ++index) {
        tabos_drive_info_t drive;
        if (!tabos_fs_drive_info(index, &drive) || !drive.mounted) {
            continue;
        }
        if (!found || drive.removable) {
            selected = drive;
            found    = true;
        }
        if (drive.removable) {
            break;
        }
    }
    return !found ||
           register_device(TABOS_DEVICE_NAME_STORAGE, selected.name, TABOS_DEVICE_CLASS_STORAGE, TABOS_DEVICE_READY,
                           selected.removable ? TABOS_DEVICE_FEATURE_STORAGE_REMOVABLE : 0U, 0, NULL);
}

static tabos_device_state_t network_device_state(const network_status_t* status)
{
    if (status->state == NETWORK_STATE_ONLINE) {
        return TABOS_DEVICE_READY;
    }
    if (status->state == NETWORK_STATE_FAILED) {
        return TABOS_DEVICE_FAULT;
    }
    return TABOS_DEVICE_OFFLINE;
}

bool hardware_devices_init(void)
{
    if (initialized) {
        return true;
    }
    platform_diagnostics_t diagnostics;
    if (!platform_get_diagnostics(&diagnostics)) {
        return false;
    }
    if (!register_device(TABOS_DEVICE_NAME_DISPLAY, platform_display_name(), TABOS_DEVICE_CLASS_DISPLAY,
                         TABOS_DEVICE_READY, TABOS_DEVICE_FEATURE_DISPLAY_FRAMEBUFFER, 0, NULL)) {
        hardware_devices_shutdown();
        return false;
    }
    if (diagnostics.keyboard_detected &&
        !register_device(TABOS_DEVICE_NAME_KEYBOARD, diagnostics.keyboard_driver, TABOS_DEVICE_CLASS_KEYBOARD,
                         diagnostics.keyboard_present ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                         TABOS_DEVICE_FEATURE_KEYBOARD_INPUT, diagnostics.keyboard_error, NULL)) {
        hardware_devices_shutdown();
        return false;
    }
    if (!register_storage()) {
        hardware_devices_shutdown();
        return false;
    }
    if (diagnostics.rtc_detected &&
        !register_device(TABOS_DEVICE_NAME_RTC, diagnostics.rtc_name, TABOS_DEVICE_CLASS_RTC,
                         diagnostics.rtc_present && diagnostics.rtc_error == 0 ? TABOS_DEVICE_READY :
                                                                                 TABOS_DEVICE_FAULT,
                         TABOS_DEVICE_FEATURE_RTC_WALL_CLOCK, diagnostics.rtc_error, &rtc_device)) {
        hardware_devices_shutdown();
        return false;
    }
    if (diagnostics.battery_detected &&
        !register_device(TABOS_DEVICE_NAME_BATTERY, diagnostics.battery_name, TABOS_DEVICE_CLASS_BATTERY,
                         diagnostics.battery_present && diagnostics.battery_error == 0 ? TABOS_DEVICE_READY :
                                                                                         TABOS_DEVICE_FAULT,
                         TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY | TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL,
                         diagnostics.battery_error, &battery_device)) {
        hardware_devices_shutdown();
        return false;
    }
    network_status_t network_status;
    if (diagnostics.network_present && network_service_status(&network_status) &&
        !register_device(TABOS_DEVICE_NAME_WIFI, diagnostics.network_name, TABOS_DEVICE_CLASS_NETWORK,
                         network_device_state(&network_status), TABOS_DEVICE_FEATURE_NETWORK_WIFI,
                         network_status.state == NETWORK_STATE_FAILED ? EIO : 0, &network_device)) {
        hardware_devices_shutdown();
        return false;
    }
    initialized = true;
    return true;
}

void hardware_devices_update(void)
{
    if (!initialized) {
        return;
    }
    if (rtc_device != TABOS_DEVICE_ID_INVALID) {
        int rtc_error        = 0;
        const bool rtc_ready = platform_wall_clock_status(&rtc_error);
        (void) device_registry_set_state(rtc_device, rtc_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         rtc_ready ? 0 : (rtc_error != 0 ? rtc_error : EIO));
    }
    if (battery_device != TABOS_DEVICE_ID_INVALID) {
        int battery_error        = 0;
        const bool battery_ready = platform_battery_health(&battery_error);
        (void) device_registry_set_state(battery_device, battery_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         battery_ready ? 0 : (battery_error != 0 ? battery_error : EIO));
    }
    if (network_device != TABOS_DEVICE_ID_INVALID) {
        network_status_t status;
        if (!network_service_status(&status)) {
            (void) device_registry_set_state(network_device, TABOS_DEVICE_FAULT, EIO);
            return;
        }
        const tabos_device_state_t state = network_device_state(&status);
        (void) device_registry_set_state(network_device, state, state == TABOS_DEVICE_FAULT ? EIO : 0);
    }
}

void hardware_devices_shutdown(void)
{
    while (registered_device_count > 0U) {
        --registered_device_count;
        (void) device_registry_remove(registered_devices[registered_device_count]);
        registered_devices[registered_device_count] = TABOS_DEVICE_ID_INVALID;
    }
    initialized    = false;
    network_device = TABOS_DEVICE_ID_INVALID;
    rtc_device     = TABOS_DEVICE_ID_INVALID;
    battery_device = TABOS_DEVICE_ID_INVALID;
}
