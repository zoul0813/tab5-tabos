#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "platform_test.h"
#include <tabos/internal/power_services.h>
#include <tabos/internal/power_config.h>
#include <tabos/internal/application.h>
#include <tabos/internal/audio.h>
#include <tabos/internal/camera.h>
#include <tabos/internal/display.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/input.h>
#include <tabos/internal/pointer.h>
#include <tabos/internal/network.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/hardware_devices.h>
#include <tabos/platform/storage_backend.h>
#include <tabos/filesystem.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char root[] = "/tmp/tabos-power-services-XXXXXX";
static int sync_error;
static unsigned int app_updates;
static unsigned int app_entries;
static unsigned int app_cleanups;
static power_manager_t manager;
static power_services_t services;
static int pointer_owner;
static tabos_pointer_stream_t pointer_stream;
static bool inject_pointer;
static bool expect_input;
static power_callback_fn saved_suspend;
static power_callback_fn saved_boundary_callback;
static platform_system_action_t boundary_action;
static bool action_before_callback;

static void request_boundary_action(void)
{
    assert(kernel_runtime_request_system_action(boundary_action));
    assert(!kernel_runtime_request_system_action(boundary_action));
}

static power_callback_result_t callback_with_system_action(void* context, power_completion_token_t token)
{
    if (action_before_callback) {
        request_boundary_action();
    }
    const power_callback_result_t result = saved_boundary_callback(context, token);
    if (!action_before_callback) {
        request_boundary_action();
    }
    return result;
}

static void inject_activity(void)
{
    if (inject_pointer) {
        tabos_pointer_event_t event = {.type = TABOS_POINTER_DOWN, .contact_id = 0U, .x = 12, .y = 34};
        pointer_service_submit(&event);
        event.type = TABOS_POINTER_UP;
        pointer_service_submit(&event);
    } else {
        tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        assert(input_submit(&event));
        event.type = TABOS_INPUT_TEXT;
        strcpy(event.text, "a");
        assert(input_submit(&event));
        event.type = TABOS_INPUT_KEY_UP;
        assert(input_submit(&event));
    }
    expect_input = true;
}

static power_callback_result_t suspend_with_activity(void* context, power_completion_token_t token)
{
    const power_callback_result_t result = saved_suspend(context, token);
    inject_activity();
    return result;
}

bool platform_battery_status(platform_battery_status_t* status)
{
    *status = (platform_battery_status_t) {0};
    return false;
}
bool platform_battery_set_charging(bool enabled)
{
    (void) enabled;
    return false;
}
bool platform_battery_set_fast_charging(bool enabled)
{
    (void) enabled;
    return false;
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}
bool storage_backend_mount(size_t index, char* letter, char* path, size_t size, bool* removable, const char** name)
{
    assert(index == 0U && strlen(root) < size);
    strcpy(path, root);
    *letter    = 'T';
    *removable = true;
    *name      = "Power coordinator model";
    return true;
}
void storage_backend_unmount(char letter)
{
    assert(letter == 'T');
}
bool storage_backend_info(char letter, uint64_t* total, uint64_t* free_bytes)
{
    assert(letter == 'T');
    *total      = 1024U;
    *free_bytes = 512U;
    return true;
}
int storage_backend_sync(char letter)
{
    assert(letter == 'T');
    /* Deterministic namespace barrier, not a production host durability claim. */
    return sync_error;
}

static bool app_entry(tabos_app_context_t* context)
{
    (void) context;
    ++app_entries;
    return true;
}
static void app_update(tabos_app_context_t* context)
{
    (void) context;
    assert(!kernel_runtime_system_action_pending());
    assert(!filesystem_power_is_frozen());
    assert(test_platform_panel_enabled() && test_platform_brightness() == 75U);
    if (expect_input) {
        if (inject_pointer) {
            tabos_pointer_event_t event;
            assert(pointer_service_read(&pointer_owner, pointer_stream, &event) == 0);
            assert(event.type == TABOS_POINTER_DOWN && event.x == 12 && event.y == 34);
            assert(pointer_service_read(&pointer_owner, pointer_stream, &event) == 0);
            assert(event.type == TABOS_POINTER_UP);
            assert(pointer_service_read(&pointer_owner, pointer_stream, &event) == -TABOS_EAGAIN);
        } else {
            tabos_input_event_t event;
            assert(tabos_input_poll(&event) && event.type == TABOS_INPUT_KEY_DOWN);
            assert(tabos_input_poll(&event) && event.type == TABOS_INPUT_TEXT && strcmp(event.text, "a") == 0);
            assert(tabos_input_poll(&event) && event.type == TABOS_INPUT_KEY_UP);
            assert(!tabos_input_poll(&event));
        }
        expect_input = false;
    }
    ++app_updates;
}
static void app_cleanup(tabos_app_context_t* context, int status)
{
    (void) context;
    (void) status;
    ++app_cleanups;
}
static const tabos_app_descriptor_t app = {.abi_version = TABOS_APPLICATION_ABI_VERSION,
                                           .name        = "retained-power-app",
                                           .version     = "1",
                                           .entry       = app_entry,
                                           .update      = app_update,
                                           .cleanup     = app_cleanup};

static void dispatch(void)
{
    power_services_update(&services, PLATFORM_RUNTIME_EVENT_POWER, platform_time_ms());
}

static void begin_storage(void)
{
    assert(power_manager_request_suspend(&manager));
    dispatch();
    assert(manager.status.state == POWER_STATE_SUSPENDING);
    dispatch();
    assert(kernel_application_power_status().state == APPLICATION_POWER_PARKED);
    dispatch();
    assert(filesystem_power_status().state == FILESYSTEM_POWER_SYNCING);
    assert(power_services_next_deadline(&services) == platform_time_ms() + 2000U);
}

static void finish_storage(void)
{
    test_platform_work_finish();
    dispatch();
}

static void assert_active(void)
{
    assert(manager.status.state == POWER_STATE_ACTIVE && !manager.status.resume_failed);
    assert(kernel_application_power_status().state == APPLICATION_POWER_ACTIVE);
    assert(!filesystem_power_is_frozen());
    assert(test_platform_brightness() == 75U && test_platform_panel_enabled());
    const unsigned int before = app_updates;
    kernel_application_system_update();
    assert(app_updates == before + 1U && app_entries == 1U && app_cleanups == 0U);
}

static void reset_manager(void)
{
    power_manager_shutdown(&manager);
    assert(power_manager_init(&manager, power_config_defaults(), platform_time_ms()));
    assert(power_services_register(&services, &manager));
    manager.shutdown_pending = kernel_runtime_system_action_pending;
    assert(power_manager_finalize(&manager));
}

static void shutdown_boundaries(void)
{
    const platform_system_action_t actions[] = {PLATFORM_SYSTEM_ACTION_REBOOT, PLATFORM_SYSTEM_ACTION_POWER_OFF};
    for (size_t action = 0U; action < 2U; ++action) {
        boundary_action = actions[action];
        /* Both sides of each callback, plus platform preparation and suspended state. */
        for (unsigned int operation = 0U; operation < 2U; ++operation) {
            for (unsigned int before = 0U; before < 2U; ++before) {
                for (size_t index = 0U; index <= POWER_SERVICE_COUNT; ++index) {
                    app_entries = 0U;
                    app_cleanups = 0U;
                    assert(kernel_runtime_init() && kernel_runtime_start(false));
                    assert(application_registry_register(&app));
                    assert(tabos_app_launch(app.name) == TABOS_APP_RESULT_OK);
                    reset_manager();
                    action_before_callback = before != 0U;
                    if (index < POWER_SERVICE_COUNT) {
                        power_callback_fn* callback = operation == 0U ? &manager.participants[index].suspend :
                                                                       &manager.participants[index].resume;
                        saved_boundary_callback = *callback;
                        *callback = callback_with_system_action;
                    } else if (operation == 0U) {
                        test_platform_power_prepare_hook(request_boundary_action);
                    }
                    const unsigned int updates = app_updates;
                    const unsigned int sleeps = test_platform_sleep_calls();
                    assert(power_manager_request_suspend(&manager));
                    for (unsigned int pass = 0U; pass < 8U && !kernel_runtime_system_action_pending(); ++pass) {
                        if (filesystem_power_status().state == FILESYSTEM_POWER_SYNCING) {
                            test_platform_work_finish();
                        }
                        dispatch();
                        if (operation != 0U && index == POWER_SERVICE_COUNT &&
                            manager.status.state == POWER_STATE_SUSPENDED) {
                            request_boundary_action();
                        }
                    }
                    assert(kernel_runtime_system_action_pending());
                    assert(!kernel_runtime_request_suspend());
                    if (operation == 0U) {
                        assert(test_platform_sleep_calls() == sleeps);
                    }
                    /* A queued action must bypass ordinary dispatch and parking
                     * timeout recovery, even after its deadline has expired. */
                    test_platform_advance_time_ms(3000U);
                    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_APPLICATION | PLATFORM_RUNTIME_EVENT_DEADLINE);
                    dispatch();
                    assert(kernel_runtime_next_deadline() == PLATFORM_RUNTIME_DEADLINE_NONE);
                    assert(app_updates == updates && app_entries == 1U && app_cleanups == 0U);
                    power_services_shutdown(&services);
                    assert(manager.status.state == POWER_STATE_SHUTTING_DOWN);
                    assert(!filesystem_power_is_frozen());
                    /* Only a request after the final application-resume callback
                     * may find execution already reopened. No app update occurs. */
                    if (!(operation != 0U && index == POWER_SERVICE_COUNT - 1U && before == 0U)) {
                        assert(kernel_application_power_status().state != APPLICATION_POWER_ACTIVE);
                    }
                    assert(kernel_runtime_take_system_action() == boundary_action);
                    assert(kernel_runtime_take_system_action() == PLATFORM_SYSTEM_ACTION_NONE);
                    power_manager_shutdown(&manager);
                    kernel_runtime_shutdown();
                    assert(app_cleanups == 1U && app_updates == updates);
                }
            }
        }
    }
}

static void runtime_shutdown_requests(void)
{
    const platform_system_action_t actions[] = {PLATFORM_SYSTEM_ACTION_REBOOT, PLATFORM_SYSTEM_ACTION_POWER_OFF};
    for (size_t action = 0U; action < 2U; ++action) {
        for (unsigned int during_sync = 0U; during_sync < 2U; ++during_sync) {
            app_entries = 0U;
            app_cleanups = 0U;
            assert(kernel_runtime_init() && kernel_runtime_start(false));
            assert(application_registry_register(&app));
            assert(tabos_app_launch(app.name) == TABOS_APP_RESULT_OK);
            boundary_action = actions[action];
            if (during_sync == 0U) {
                test_platform_power_prepare_hook(request_boundary_action);
            }
            const unsigned int updates = app_updates;
            const unsigned int sleeps = test_platform_sleep_calls();
            assert(kernel_runtime_request_suspend());
            kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
            kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
            kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
            assert(filesystem_power_status().state == FILESYSTEM_POWER_SYNCING);
            if (during_sync != 0U) {
                request_boundary_action();
            } else {
                test_platform_work_finish();
            }
            kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER | PLATFORM_RUNTIME_EVENT_APPLICATION);
            assert(kernel_runtime_system_action_pending());
            assert(test_platform_sleep_calls() == sleeps && app_updates == updates);
            assert(kernel_application_power_status().state == APPLICATION_POWER_PARKED);
            assert(kernel_runtime_take_system_action() == boundary_action);
            kernel_runtime_shutdown();
            assert(app_cleanups == 1U && app_updates == updates);
        }
    }
}

static void shutdown_restore_failures(void)
{
    const platform_system_action_t actions[] = {PLATFORM_SYSTEM_ACTION_REBOOT, PLATFORM_SYSTEM_ACTION_POWER_OFF};
    for (size_t action = 0U; action < 2U; ++action) {
        for (unsigned int fault = 0U; fault < 3U; ++fault) {
            app_entries = 0U;
            app_cleanups = 0U;
            assert(kernel_runtime_init() && kernel_runtime_start(false));
            assert(application_registry_register(&app));
            assert(tabos_app_launch(app.name) == TABOS_APP_RESULT_OK);
            reset_manager();
            begin_storage();
            finish_storage();
            if (fault == 0U) {
                test_platform_power_fail_once(3U);
            } else if (fault == 1U) {
                test_platform_fail_panel_once();
            } else {
                test_platform_network_power_errors(0, -TABOS_EIO);
            }
            boundary_action = actions[action];
            request_boundary_action();
            const unsigned int updates = app_updates;
            power_services_shutdown(&services);
            assert(manager.status.resume_failed && services.panic_reported);
            assert(strstr(test_platform_last_log(), "KERNEL PANIC: power restore failed") != NULL);
            assert(kernel_application_power_status().state == APPLICATION_POWER_PARKED);
            assert(app_updates == updates);
            assert(kernel_runtime_take_system_action() == boundary_action);
            power_manager_shutdown(&manager);
            kernel_runtime_shutdown();
            assert(app_cleanups == 1U && app_updates == updates);
            test_platform_network_power_errors(0, 0);
        }
    }
}

static void activity_boundaries(void)
{
    pointer_service_set_device_id(43U);
    pointer_service_set_foreground_owner(&pointer_owner);
    pointer_stream = pointer_service_open(&pointer_owner, 43U);
    assert(pointer_stream > 0);
    for (unsigned int source = 0U; source < 2U; ++source) {
        inject_pointer = source != 0U;
        for (size_t index = 0U; index <= POWER_SERVICE_COUNT; ++index) {
            reset_manager();
            power_policy_t policy    = manager.status.policy;
            policy.automatic_suspend = true;
            assert(power_manager_set_policy(&manager, policy, platform_time_ms()));
            if (index == POWER_SERVICE_COUNT) {
                test_platform_power_prepare_hook(inject_activity);
            } else {
                saved_suspend                       = manager.participants[index].suspend;
                manager.participants[index].suspend = suspend_with_activity;
            }
            const unsigned int sleeps  = test_platform_sleep_calls();
            const unsigned int updates = app_updates;
            assert(power_manager_request_suspend(&manager));
            dispatch();
            for (unsigned int pass = 0U; pass < 8U && manager.status.state == POWER_STATE_SUSPENDING; ++pass) {
                assert(app_updates == updates);
                if (filesystem_power_status().state == FILESYSTEM_POWER_SYNCING) {
                    test_platform_work_finish();
                }
                dispatch();
            }
            assert(expect_input);
            assert_active(); /* App sees retained input only after display/storage restoration. */
            assert(!expect_input && manager.status.failure.code == POWER_FAILURE_NONE);
            assert(manager.status.policy.automatic_suspend);
            assert(test_platform_sleep_calls() == sleeps);
            bool held;
            (void) input_take_power_activity(&held);
            assert(!held);
            (void) pointer_service_take_power_activity(&held);
            assert(!held);
        }
    }
    assert(pointer_service_close(&pointer_owner, pointer_stream) == 0);
    reset_manager();
}

int main(void)
{
    assert(mkdtemp(root) != NULL);
    test_platform_set_time_ms(10U);
    test_platform_work_enable(true);
    assert(kernel_runtime_init() && kernel_runtime_start(false));
    assert(application_registry_register(&app));
    assert(tabos_app_launch(app.name) == TABOS_APP_RESULT_OK);
    /* Exercise the actual dispatcher, not only the participant adapter. */
    assert(kernel_runtime_request_suspend());
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(filesystem_power_status().state == FILESYSTEM_POWER_SYNCING);
    test_platform_work_finish();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(kernel_runtime_power_status()->state == POWER_STATE_SUSPENDED);
    assert(kernel_runtime_next_deadline() == PLATFORM_RUNTIME_DEADLINE_NONE);
    const unsigned int updates = app_updates;
    const unsigned int reads   = test_platform_network_status_calls();
    test_platform_advance_time_ms(60000U);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_NETWORK | PLATFORM_RUNTIME_EVENT_DEADLINE);
    assert(app_updates == updates && test_platform_network_status_calls() == reads);
    assert(kernel_runtime_next_deadline() == PLATFORM_RUNTIME_DEADLINE_NONE);
    inject_activity();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT);
    assert(kernel_runtime_power_status()->state == POWER_STATE_ACTIVE && !expect_input);
    assert(app_updates == updates + 1U && test_platform_network_status_calls() > reads);
    assert(kernel_runtime_next_deadline() > platform_time_ms());
    /* Same-pass activity takes priority over a queued explicit request. */
    const unsigned int prior_sleeps = test_platform_sleep_calls();
    assert(kernel_runtime_request_suspend());
    inject_activity();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_INPUT | PLATFORM_RUNTIME_EVENT_POWER);
    assert(kernel_runtime_power_status()->state == POWER_STATE_ACTIVE && !expect_input);
    assert(test_platform_sleep_calls() == prior_sleeps);

    pointer_service_set_device_id(43U);
    pointer_service_set_foreground_owner(&pointer_owner);
    pointer_stream = pointer_service_open(&pointer_owner, 43U);
    assert(pointer_stream > 0);
    assert(kernel_runtime_request_suspend());
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(filesystem_power_status().state == FILESYSTEM_POWER_SYNCING);
    test_platform_work_finish();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(kernel_runtime_power_status()->state == POWER_STATE_SUSPENDED);
    inject_pointer = true;
    inject_activity();
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POINTER);
    assert(kernel_runtime_power_status()->state == POWER_STATE_ACTIVE && !expect_input);
    assert(pointer_service_close(&pointer_owner, pointer_stream) == 0);
    inject_pointer = false;
    assert(kernel_runtime_request_suspend());
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(filesystem_power_status().state == FILESYSTEM_POWER_SYNCING);
    const unsigned int before_shutdown = app_updates;
    kernel_runtime_shutdown();
    assert(app_cleanups == 1U && app_updates == before_shutdown);
    app_cleanups = 0U;
    app_entries  = 0U;
    assert(kernel_runtime_init() && kernel_runtime_start(false));
    assert(application_registry_register(&app));
    assert(tabos_app_launch(app.name) == TABOS_APP_RESULT_OK);
    reset_manager();
    activity_boundaries();
    const tabos_fd_t file = tabos_fs_open("T:/retained", TABOS_O_CREAT | TABOS_O_RDWR, 0600U);
    assert(file > 0 && tabos_fs_write(file, "abc", 3U) == 3);
    assert(tabos_fs_seek(file, 1, TABOS_SEEK_SET) == 1);
    const tabos_dir_t directory = tabos_fs_opendir("T:/");
    tabos_dirent_t entry;
    assert(directory > 0 && tabos_fs_readdir(directory, &entry) == 1);
    tabos_stat_t identity;
    assert(tabos_fs_fstat(file, &identity) == 0);
    platform_pixel_t* pixels = display_framebuffer()->pixels;
    pixels[0]                = 0x1234U;
    const char* down[]       = {"applications:down", "health:down",   "audio:down",   "camera:down", "network:down",
                                "storage:down",      "keyboard:down", "pointer:down", "display:down"};
    for (unsigned int cycle = 0U; cycle < 1000U; ++cycle) {
        begin_storage();
        assert(tabos_fs_close(file) == -1 && *tabos_errno_location() == TABOS_EBUSY);
        const unsigned int before = app_updates;
        kernel_application_system_update();
        assert(app_updates == before);
        finish_storage();
        assert(manager.status.state == POWER_STATE_SUSPENDED);
        assert(manager.status.brightness_valid && manager.status.effective_brightness == 0U);
        assert(manager.status.panel_valid && !manager.status.panel_enabled);
        assert(!test_platform_panel_enabled() && test_platform_brightness() == 0U);
        assert(power_services_next_deadline(&services) == PLATFORM_RUNTIME_DEADLINE_NONE);
        assert(hardware_devices_next_deadline() == PLATFORM_RUNTIME_DEADLINE_NONE);
        assert(!network_service_disconnect());
        assert(!display_present());
        for (size_t index = 0U; index < POWER_SERVICE_COUNT; ++index) {
            assert(strcmp(power_manager_trace_entry(&manager, index), down[index]) == 0);
        }
        dispatch();
        assert_active();
        assert(manager.trace_count == POWER_SERVICE_COUNT * 2U);
        assert(strcmp(power_manager_trace_entry(&manager, 9U), "display:up") == 0);
        assert(strcmp(power_manager_trace_entry(&manager, 17U), "applications:up") == 0);
        assert(display_framebuffer()->pixels == pixels && pixels[0] == 0x1234U);
        tabos_stat_t after;
        assert(tabos_fs_fstat(file, &after) == 0 && after.file_id == identity.file_id);
        assert(tabos_fs_seek(file, 0, TABOS_SEEK_CUR) == 1);
        assert(tabos_fs_readdir(directory, &entry) == 0);
    }

    /* Partial display failure must restore that failing node, not skip it. */
    begin_storage();
    test_platform_fail_panel_once();
    finish_storage();
    assert_active();
    assert(manager.status.failure.code == POWER_FAILURE_CALLBACK);
    assert(strcmp(manager.status.failure.participant, "display") == 0);
    assert(strcmp(power_manager_trace_entry(&manager, 9U), "display:up") == 0);
    assert(display_present());

    for (unsigned int stage = 1U; stage <= 2U; ++stage) {
        begin_storage();
        test_platform_power_fail_once(stage);
        finish_storage();
        assert_active();
        assert(manager.status.failure.code ==
               (stage == 1U ? POWER_FAILURE_PLATFORM_PREPARE : POWER_FAILURE_PLATFORM_SLEEP));
    }
    sync_error = TABOS_ENOTSUP;
    begin_storage();
    finish_storage();
    assert_active();
    assert(strcmp(manager.status.failure.participant, "storage") == 0);
    sync_error = 0;

    /* Cancellation waits for borrowed storage work; cannot reopen it early. */
    begin_storage();
    const unsigned int sleeps = test_platform_sleep_calls();
    power_manager_request_activity(&manager, platform_time_ms());
    dispatch();
    assert(filesystem_power_is_frozen() && manager.status.state == POWER_STATE_SUSPENDING);
    test_platform_advance_time_ms(2000U);
    dispatch();
    assert(filesystem_power_status().state == FILESYSTEM_POWER_ABORTING);
    assert(power_services_next_deadline(&services) == PLATFORM_RUNTIME_DEADLINE_NONE);
    finish_storage();
    assert_active();
    assert(test_platform_sleep_calls() == sleeps);

    /* Native unsupported transport rolls back the already-parked services. */
    test_platform_network_power_errors(-TABOS_ENOTSUP, 0);
    assert(power_manager_request_suspend(&manager));
    dispatch();
    dispatch();
    assert_active();
    assert(strcmp(manager.status.failure.participant, "network") == 0);
    test_platform_network_power_errors(0, 0);

    /* Resume failure stops before unpark, retaining ownership for explicit teardown. */
    begin_storage();
    finish_storage();
    test_platform_network_power_errors(0, -TABOS_EIO);
    dispatch();
    assert(manager.status.resume_failed && manager.status.state == POWER_STATE_RESUMING);
    assert(kernel_application_power_status().state == APPLICATION_POWER_PARKED);
    assert(strstr(test_platform_last_log(), "KERNEL PANIC: power restore failed") != NULL);
    assert(!network_service_disconnect());
    test_platform_network_power_errors(0, 0);
    assert(network_service_power_resume() == 0);
    camera_service_power_resume();
    audio_service_power_resume();
    hardware_devices_resume_audit();
    kernel_application_power_end();
    reset_manager();

    /* System action during sync restores services but never schedules app code. */
    begin_storage();
    power_manager_begin_shutdown(&manager, platform_time_ms());
    assert(manager.status.state == POWER_STATE_SUSPENDING);
    finish_storage();
    assert(manager.status.state == POWER_STATE_SHUTTING_DOWN);
    assert(kernel_application_power_status().state == APPLICATION_POWER_PARKED);
    assert(!filesystem_power_is_frozen());
    assert(tabos_fs_closedir(directory) == 0 && tabos_fs_close(file) == 0);
    assert(tabos_fs_unlink("T:/retained") == 0);
    power_manager_shutdown(&manager);
    kernel_runtime_shutdown();
    assert(app_cleanups == 1U && app_entries == 1U);
    shutdown_boundaries();
    runtime_shutdown_requests();
    shutdown_restore_failures();
    assert(rmdir(root) == 0);
    return 0;
}
