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
    assert(!filesystem_power_is_frozen());
    assert(test_platform_panel_enabled() && test_platform_brightness() == 75U);
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
    assert(power_manager_finalize(&manager));
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
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_POWER);
    assert(kernel_runtime_power_status()->state == POWER_STATE_ACTIVE);
    assert(app_updates == updates + 1U && test_platform_network_status_calls() > reads);
    assert(kernel_runtime_next_deadline() > platform_time_ms());
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
    assert(rmdir(root) == 0);
    return 0;
}
