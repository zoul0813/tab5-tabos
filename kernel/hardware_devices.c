#include <tabos/internal/hardware_devices.h>
#include <tabos/internal/audio.h>
#include <tabos/internal/camera.h>

#include <tabos/device.h>
#include <tabos/filesystem.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/network.h>
#include <tabos/internal/pointer.h>
#include <tabos/internal/time.h>
#include <tabos/platform/platform.h>

#include <errno.h>

enum {
    HARDWARE_HEALTH_AUDIT_MS = 60000U,
};

static tabos_device_id_t network_device  = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t keyboard_device = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t rtc_device      = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t battery_device  = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t audio_device    = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t pointer_device  = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t camera_device   = TABOS_DEVICE_ID_INVALID;
static tabos_device_id_t storage_device  = TABOS_DEVICE_ID_INVALID;
static char storage_drive_letter;
static tabos_device_id_t registered_devices[9];
static size_t registered_device_count;
static bool initialized;
static bool health_audit_suspended;
static tabos_timer_t health_audit_timer;

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
    if (found) {
        storage_drive_letter = selected.letter;
    }
    return !found ||
           register_device(TABOS_DEVICE_NAME_STORAGE, selected.name, TABOS_DEVICE_CLASS_STORAGE, TABOS_DEVICE_READY,
                           selected.removable ? TABOS_DEVICE_FEATURE_STORAGE_REMOVABLE : 0U, 0, &storage_device);
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
                         TABOS_DEVICE_FEATURE_KEYBOARD_INPUT, diagnostics.keyboard_error, &keyboard_device)) {
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
    tabos_audio_info_t audio_info;
    const char* audio_driver = NULL;
    int audio_error          = 0;
    if (audio_service_info(&audio_info, &audio_driver, &audio_error)) {
        tabos_device_features_t features = 0U;
        if ((audio_info.features & TABOS_AUDIO_FEATURE_PLAYBACK) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK;
        }
        if ((audio_info.features & TABOS_AUDIO_FEATURE_CAPTURE) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_CAPTURE;
        }
        if ((audio_info.features & TABOS_AUDIO_FEATURE_AEC) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_AEC;
        }
        if ((audio_info.routes & TABOS_AUDIO_ROUTE_SPEAKER) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_SPEAKER;
        }
        if ((audio_info.routes & TABOS_AUDIO_ROUTE_HEADPHONE) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_HEADPHONE;
        }
        if ((audio_info.routes & TABOS_AUDIO_ROUTE_MICROPHONE) != 0U) {
            features |= TABOS_DEVICE_FEATURE_AUDIO_MICROPHONE;
        }
        if (!register_device(TABOS_DEVICE_NAME_AUDIO, audio_driver != NULL ? audio_driver : "audio",
                             TABOS_DEVICE_CLASS_AUDIO, audio_error == 0 ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                             features, audio_error, &audio_device)) {
            hardware_devices_shutdown();
            return false;
        }
    }
    const char* pointer_driver = NULL;
    int pointer_error          = 0;
    if (pointer_service_info(&pointer_driver, &pointer_error)) {
        if (!register_device(TABOS_DEVICE_NAME_TOUCH, pointer_driver != NULL ? pointer_driver : "touch",
                             TABOS_DEVICE_CLASS_POINTER, pointer_error == 0 ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                             TABOS_DEVICE_FEATURE_POINTER_TOUCH | TABOS_DEVICE_FEATURE_POINTER_MULTICONTACT,
                             pointer_error, &pointer_device)) {
            hardware_devices_shutdown();
            return false;
        }
        pointer_service_set_device_id(pointer_device);
    }
    tabos_camera_info_t camera_info;
    const char* camera_driver = NULL;
    bool camera_ready         = false;
    int camera_error          = 0;
    if (camera_service_info(&camera_info, &camera_driver, &camera_ready, &camera_error)) {
        if (!register_device(
                TABOS_DEVICE_NAME_CAMERA, camera_driver != NULL ? camera_driver : "camera", TABOS_DEVICE_CLASS_CAMERA,
                camera_error != 0 ? TABOS_DEVICE_FAULT : (camera_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_OFFLINE),
                TABOS_DEVICE_FEATURE_CAMERA_CAPTURE | TABOS_DEVICE_FEATURE_CAMERA_RAW, camera_error, &camera_device)) {
            hardware_devices_shutdown();
            return false;
        }
        camera_service_set_device_id(camera_device);
    }
    network_status_t network_status;
    if (diagnostics.network_present && network_service_status(&network_status) &&
        !register_device(TABOS_DEVICE_NAME_WIFI, diagnostics.network_name, TABOS_DEVICE_CLASS_NETWORK,
                         network_device_state(&network_status), TABOS_DEVICE_FEATURE_NETWORK_WIFI,
                         network_status.state == NETWORK_STATE_FAILED ? EIO : 0, &network_device)) {
        hardware_devices_shutdown();
        return false;
    }
    initialized            = true;
    health_audit_suspended = false;
    tabos_timer_start(&health_audit_timer, HARDWARE_HEALTH_AUDIT_MS, HARDWARE_HEALTH_AUDIT_MS);
    return true;
}

static void audit_unreported_health(void)
{
    if (keyboard_device != TABOS_DEVICE_ID_INVALID) {
        int keyboard_error        = 0;
        const bool keyboard_ready = platform_keyboard_health(&keyboard_error);
        (void) device_registry_set_state(keyboard_device, keyboard_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         keyboard_ready ? 0 : (keyboard_error != 0 ? keyboard_error : EIO));
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
    if (storage_device != TABOS_DEVICE_ID_INVALID) {
        bool mounted       = false;
        const size_t count = tabos_fs_drive_count();
        for (size_t index = 0U; index < count; ++index) {
            tabos_drive_info_t drive;
            if (tabos_fs_drive_info(index, &drive) && drive.letter == storage_drive_letter && drive.mounted) {
                mounted = true;
                break;
            }
        }
        (void) device_registry_set_state(storage_device, mounted ? TABOS_DEVICE_READY : TABOS_DEVICE_OFFLINE, 0);
    }
}

void hardware_devices_health_changed(hardware_device_health_t health)
{
    if (!initialized) {
        return;
    }
    if (health == HARDWARE_DEVICE_HEALTH_RTC && rtc_device != TABOS_DEVICE_ID_INVALID) {
        int rtc_error        = 0;
        const bool rtc_ready = platform_wall_clock_status(&rtc_error);
        (void) device_registry_set_state(rtc_device, rtc_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         rtc_ready ? 0 : (rtc_error != 0 ? rtc_error : EIO));
    } else if (health == HARDWARE_DEVICE_HEALTH_BATTERY && battery_device != TABOS_DEVICE_ID_INVALID) {
        int battery_error        = 0;
        const bool battery_ready = platform_battery_health(&battery_error);
        (void) device_registry_set_state(battery_device, battery_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         battery_ready ? 0 : (battery_error != 0 ? battery_error : EIO));
    }
}

void hardware_devices_update(void)
{
    if (!initialized) {
        return;
    }
    if (!health_audit_suspended && tabos_timer_poll(&health_audit_timer)) {
        audit_unreported_health();
    }
    if (audio_device != TABOS_DEVICE_ID_INVALID) {
        int audio_error        = 0;
        const bool audio_ready = audio_service_info(NULL, NULL, &audio_error) && audio_error == 0;
        (void) device_registry_set_state(audio_device, audio_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT,
                                         audio_ready ? 0 : (audio_error != 0 ? audio_error : EIO));
    }
    if (pointer_device != TABOS_DEVICE_ID_INVALID) {
        int pointer_error        = 0;
        const bool pointer_ready = pointer_service_info(NULL, &pointer_error);
        const tabos_device_state_t state =
            pointer_ready ? (pointer_error == 0 ? TABOS_DEVICE_READY : TABOS_DEVICE_FAULT) : TABOS_DEVICE_OFFLINE;
        (void) device_registry_set_state(pointer_device, state,
                                         state == TABOS_DEVICE_FAULT ? (pointer_error != 0 ? pointer_error : EIO) : 0);
    }
    if (camera_device != TABOS_DEVICE_ID_INVALID) {
        bool camera_ready         = false;
        int camera_error          = 0;
        const bool camera_present = camera_service_info(NULL, NULL, &camera_ready, &camera_error);
        const tabos_device_state_t state =
            !camera_present ?
                TABOS_DEVICE_OFFLINE :
                (camera_error != 0 ? TABOS_DEVICE_FAULT : (camera_ready ? TABOS_DEVICE_READY : TABOS_DEVICE_OFFLINE));
        (void) device_registry_set_state(camera_device, state, state == TABOS_DEVICE_FAULT ? camera_error : 0);
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

uint64_t hardware_devices_next_deadline(void)
{
    return initialized && !health_audit_suspended ? time_timer_deadline(&health_audit_timer) : TIME_DEADLINE_NONE;
}

void hardware_devices_suspend_audit(void)
{
    if (initialized) {
        health_audit_suspended = true;
    }
}

void hardware_devices_resume_audit(void)
{
    if (!initialized || !health_audit_suspended) {
        return;
    }
    health_audit_suspended = false;
    if (tabos_timer_poll(&health_audit_timer)) {
        audit_unreported_health();
    }
}

void hardware_devices_shutdown(void)
{
    while (registered_device_count > 0U) {
        --registered_device_count;
        (void) device_registry_remove(registered_devices[registered_device_count]);
        registered_devices[registered_device_count] = TABOS_DEVICE_ID_INVALID;
    }
    initialized            = false;
    health_audit_suspended = false;
    tabos_timer_cancel(&health_audit_timer);
    network_device       = TABOS_DEVICE_ID_INVALID;
    keyboard_device      = TABOS_DEVICE_ID_INVALID;
    rtc_device           = TABOS_DEVICE_ID_INVALID;
    battery_device       = TABOS_DEVICE_ID_INVALID;
    audio_device         = TABOS_DEVICE_ID_INVALID;
    pointer_device       = TABOS_DEVICE_ID_INVALID;
    camera_device        = TABOS_DEVICE_ID_INVALID;
    storage_device       = TABOS_DEVICE_ID_INVALID;
    storage_drive_letter = '\0';
}
