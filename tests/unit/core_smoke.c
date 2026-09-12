#include <tabos/internal/runtime.h>
#include <tabos/internal/input.h>
#include <tabos/internal/pointer.h>
#include <tabos/internal/application.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/hardware_devices.h>

#include <tabos/application.h>
#include <tabos/device.h>
#include <tabos/terminal.h>

#include <tabos/config/console.h>
#include <tabos/config/filesystem.h>
#include <tabos/config/loader.h>

#include "../fakes/platform_test.h"

#include <errno.h>
#include <string.h>

static bool panic_requested;

static bool power_test_entry(tabos_app_context_t* context)
{
    (void) context;
    return true;
}

static void power_test_update(tabos_app_context_t* context)
{
    if (panic_requested) {
        tabos_app_request_exit(context, 1);
    }
}

int main(void)
{
    if (!kernel_runtime_init()) {
        return 1;
    }

    if (!kernel_runtime_init()) {
        return 1;
    }

    if (strlen(kernel_runtime_version()) == 0U) {
        return 1;
    }

    if (kernel_runtime_take_system_action() != PLATFORM_SYSTEM_ACTION_NONE ||
        kernel_runtime_request_system_action(PLATFORM_SYSTEM_ACTION_NONE) ||
        !kernel_runtime_request_system_action(PLATFORM_SYSTEM_ACTION_REBOOT) ||
        kernel_runtime_request_system_action(PLATFORM_SYSTEM_ACTION_POWER_OFF) ||
        kernel_runtime_take_system_action() != PLATFORM_SYSTEM_ACTION_REBOOT ||
        kernel_runtime_take_system_action() != PLATFORM_SYSTEM_ACTION_NONE) {
        return 1;
    }

    if (tabos_terminal_set_scale(0U) || tabos_terminal_set_scale(9U) || !tabos_terminal_set_scale(3U) ||
        tabos_terminal_get_scale() != 3U) {
        return 1;
    }

    bool launch_startup_application = true;
#if TABOS_ENABLE_SHELL_STARTUP
    launch_startup_application = false;
#endif
    if (!kernel_runtime_start(launch_startup_application)) {
        return 1;
    }

    tabos_device_info_t device;
    if (device_registry_count() != 8U || !device_registry_find(TABOS_DEVICE_NAME_DISPLAY, &device) ||
        device.device_class != TABOS_DEVICE_CLASS_DISPLAY || device.state != TABOS_DEVICE_READY ||
        !device_registry_find(TABOS_DEVICE_NAME_KEYBOARD, &device) ||
        device.device_class != TABOS_DEVICE_CLASS_KEYBOARD || !device_registry_find(TABOS_DEVICE_NAME_RTC, &device) ||
        device.device_class != TABOS_DEVICE_CLASS_RTC || device.state != TABOS_DEVICE_READY ||
        device.features != TABOS_DEVICE_FEATURE_RTC_WALL_CLOCK ||
        !device_registry_find(TABOS_DEVICE_NAME_BATTERY, &device) ||
        device.device_class != TABOS_DEVICE_CLASS_BATTERY || device.state != TABOS_DEVICE_READY ||
        device.features != (TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY | TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL) ||
        !device_registry_find(TABOS_DEVICE_NAME_AUDIO, &device) || device.device_class != TABOS_DEVICE_CLASS_AUDIO ||
        device.state != TABOS_DEVICE_READY ||
        device.features != (TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK | TABOS_DEVICE_FEATURE_AUDIO_CAPTURE |
                            TABOS_DEVICE_FEATURE_AUDIO_SPEAKER | TABOS_DEVICE_FEATURE_AUDIO_HEADPHONE |
                            TABOS_DEVICE_FEATURE_AUDIO_MICROPHONE) ||
        !device_registry_find(TABOS_DEVICE_NAME_TOUCH, &device) || device.device_class != TABOS_DEVICE_CLASS_POINTER ||
        device.state != TABOS_DEVICE_READY ||
        device.features != (TABOS_DEVICE_FEATURE_POINTER_TOUCH | TABOS_DEVICE_FEATURE_POINTER_MULTICONTACT) ||
        !device_registry_find(TABOS_DEVICE_NAME_CAMERA, &device) || device.device_class != TABOS_DEVICE_CLASS_CAMERA ||
        device.state != TABOS_DEVICE_READY ||
        device.features != (TABOS_DEVICE_FEATURE_CAMERA_CAPTURE | TABOS_DEVICE_FEATURE_CAMERA_RAW) ||
        !device_registry_find(TABOS_DEVICE_NAME_WIFI, &device) || device.device_class != TABOS_DEVICE_CLASS_NETWORK ||
        device.state != TABOS_DEVICE_OFFLINE) {
        return 1;
    }

    const unsigned int idle_network_status_calls = test_platform_network_status_calls();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_NONE);
    if (test_platform_network_status_calls() != idle_network_status_calls ||
        test_platform_keyboard_update_calls() != 0U || test_platform_pointer_update_calls() != 0U) {
        return 1;
    }
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT);
    if (test_platform_network_status_calls() != idle_network_status_calls ||
        test_platform_keyboard_update_calls() != 1U || test_platform_pointer_update_calls() != 0U) {
        return 1;
    }
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    if (test_platform_network_status_calls() != idle_network_status_calls ||
        test_platform_keyboard_update_calls() != 1U || test_platform_pointer_update_calls() != 1U) {
        return 1;
    }

    if (test_platform_activity_reports() != 0U) {
        return 1;
    }
    test_platform_keyboard_set_status(false, EIO);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_KEYBOARD, &device) || device.state != TABOS_DEVICE_FAULT ||
        device.last_error != EIO) {
        return 1;
    }
#if TABOS_TEST_RUNTIME_DIAGNOSTICS
    if (test_platform_activity_reports() != 1U) {
        return 1;
    }
#else
    if (test_platform_activity_reports() != 0U) {
        return 1;
    }
#endif
    test_platform_keyboard_set_status(true, 0);
    hardware_devices_suspend_audit();
    if (hardware_devices_next_deadline() != PLATFORM_RUNTIME_DEADLINE_NONE) {
        return 1;
    }
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_KEYBOARD, &device) || device.state != TABOS_DEVICE_FAULT) {
        return 1;
    }
    hardware_devices_resume_audit();
    if (!device_registry_find(TABOS_DEVICE_NAME_KEYBOARD, &device) || device.state != TABOS_DEVICE_READY ||
        device.last_error != 0 || hardware_devices_next_deadline() != test_platform_time_ms() + 60000U) {
        return 1;
    }

    test_platform_rtc_set_status(false, EIO);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_RTC, &device) || device.state != TABOS_DEVICE_FAULT ||
        device.last_error != EIO) {
        return 1;
    }
    test_platform_rtc_set_status(true, 0);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_RTC, &device) || device.state != TABOS_DEVICE_READY ||
        device.last_error != 0) {
        return 1;
    }
    test_platform_battery_set_status(false, EIO);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_BATTERY, &device) || device.state != TABOS_DEVICE_FAULT ||
        device.last_error != EIO) {
        return 1;
    }
    test_platform_battery_set_status(true, 0);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!device_registry_find(TABOS_DEVICE_NAME_BATTERY, &device) || device.state != TABOS_DEVICE_READY ||
        device.last_error != 0) {
        return 1;
    }

    test_platform_network_set_state(PLATFORM_NETWORK_ONLINE, NULL);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_NETWORK);
    if (!device_registry_find(TABOS_DEVICE_NAME_WIFI, &device) || device.state != TABOS_DEVICE_READY ||
        device.last_error != 0) {
        return 1;
    }

    test_platform_audio_error(EIO);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEVICE);
    if (!device_registry_find(TABOS_DEVICE_NAME_AUDIO, &device) || device.state != TABOS_DEVICE_FAULT ||
        device.last_error != EIO) {
        return 1;
    }

    test_platform_camera_error(EIO);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEVICE);
    if (!device_registry_find(TABOS_DEVICE_NAME_CAMERA, &device) || device.state != TABOS_DEVICE_FAULT ||
        device.last_error != EIO) {
        return 1;
    }

#if TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    const tabos_app_descriptor_t* active = tabos_app_active();
    if (active == NULL || strcmp(active->name, "console-test") != 0) {
        return 1;
    }
    const tabos_input_event_t exit_event = {
        .type      = TABOS_INPUT_KEY_DOWN,
        .key       = TABOS_KEY_Q,
        .modifiers = TABOS_MODIFIER_CONTROL,
    };
    if (!input_submit(&exit_event)) {
        return 1;
    }
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT);
    int exit_status = -1;
    if (!tabos_app_is_running() || tabos_process_count() != 1U || tabos_process_system_panicked() ||
        !tabos_app_last_exit_status(&exit_status) || exit_status != 0) {
        return 1;
    }
#elif TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    int exit_status = 0;
    if (tabos_app_count() != 1U || !tabos_app_is_running() || tabos_process_count() != 1U ||
        tabos_process_system_panicked() || !tabos_app_last_exit_status(&exit_status) || exit_status != 1) {
        return 1;
    }
#elif TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    int exit_status = 0;
    if (tabos_app_count() != 1U || tabos_app_is_running() || tabos_process_count() != 1U ||
        !tabos_process_system_panicked() || !tabos_app_last_exit_status(&exit_status) || exit_status != 4) {
        return 1;
    }
#else
    if (tabos_app_count() != 0U || tabos_app_is_running()) {
        return 1;
    }
#endif

    if (!tabos_terminal_set_scale(4U) || tabos_terminal_get_scale() != 4U) {
        return 1;
    }
    kernel_runtime_shutdown();
    /* Isolate power checks from optional diagnostic startup applications. */
    if (!kernel_runtime_init() || !kernel_runtime_start(false)) {
        return 1;
    }
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 20U) {
        return 1;
    }
    test_platform_advance_time_ms(120000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 0U) {
        return 1;
    }
    /* The normal health audit still detects faults while the screen is off. */
    test_platform_rtc_set_status(false, EIO);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 0U || !device_registry_find(TABOS_DEVICE_NAME_RTC, &device) ||
        device.state != TABOS_DEVICE_FAULT) {
        return 1;
    }
    test_platform_rtc_set_status(true, 0);
    /* Physical touch activity restores output even without an open pointer stream. */
    tabos_pointer_event_t touch = {.type = TABOS_POINTER_DOWN, .contact_id = 0U, .x = 10, .y = 20};
    pointer_service_submit(&touch);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    if (test_platform_brightness() != 75U) {
        return 1;
    }
    test_platform_advance_time_ms(180000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 75U) {
        return 1;
    }
    touch.type = TABOS_POINTER_MOVE;
    touch.x    = 11;
    pointer_service_submit(&touch);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    touch.type = TABOS_POINTER_UP;
    pointer_service_submit(&touch);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 20U) {
        return 1;
    }
    test_platform_advance_time_ms(120000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 0U) {
        return 1;
    }
    const tabos_input_event_t wake_key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
    if (!test_platform_panel_enabled()) {
        return 1;
    }
    test_platform_advance_time_ms(119999U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (!test_platform_panel_enabled()) {
        return 1;
    }
    test_platform_advance_time_ms(1U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_panel_enabled() || test_platform_brightness() != 0U) {
        return 1;
    }
    /* Final stage ignores pointer activity even on host/other touch controllers. */
    touch.type = TABOS_POINTER_DOWN;
    pointer_service_submit(&touch);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    touch.type = TABOS_POINTER_UP;
    pointer_service_submit(&touch);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    if (test_platform_panel_enabled() || test_platform_brightness() != 0U) {
        return 1;
    }
    if (!input_submit(&wake_key)) {
        return 1;
    }
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT);
    if (test_platform_brightness() != 75U || !test_platform_panel_enabled()) {
        return 1;
    }
    kernel_runtime_shutdown();
    /* A root-process panic while off must become visible and stay visible. */
    if (!kernel_runtime_init() || !kernel_runtime_start(false)) {
        return 1;
    }
    const tabos_app_descriptor_t app = {.abi_version  = TABOS_APPLICATION_ABI_VERSION,
                                        .name         = "power-panic",
                                        .version      = "1",
                                        .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
                                        .entry        = power_test_entry,
                                        .update       = power_test_update};
    if (!application_registry_register(&app) || tabos_app_launch(app.name) != TABOS_APP_RESULT_OK) {
        return 1;
    }
    test_platform_advance_time_ms(300000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 0U || test_platform_panel_enabled()) {
        return 1;
    }
    panic_requested = true;
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_APPLICATION);
    if (!tabos_process_system_panicked() || test_platform_brightness() != 75U || !test_platform_panel_enabled()) {
        return 1;
    }
    test_platform_advance_time_ms(180000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_DEADLINE);
    if (test_platform_brightness() != 75U) {
        return 1;
    }
    kernel_runtime_shutdown();
    return 0;
}
