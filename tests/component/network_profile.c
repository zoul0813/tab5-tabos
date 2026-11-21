#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/filesystem.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/network.h>
#include <tabos/internal/network_config.h>
#include <tabos/platform/storage_backend.h>

#include "platform_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char storage_root[] = "/tmp/tabos-network-profile-XXXXXX";
static int failures;

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t capacity, bool* removable, const char** name)
{
    if (index != 0U || strlen(storage_root) >= capacity) {
        return false;
    }
    strcpy(root, storage_root);
    *letter    = 'T';
    *removable = false;
    *name      = "Network profile test";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void) letter;
}

bool storage_backend_info(char letter, uint64_t* total, uint64_t* free_bytes)
{
    (void) letter;
    *total      = 1024U;
    *free_bytes = 512U;
    return true;
}

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static size_t read_config(char* buffer, size_t capacity)
{
    const tabos_fd_t file = tabos_fs_open("T:/etc/wifi.conf", TABOS_O_RDONLY, 0U);
    if (file < 0) {
        buffer[0] = '\0';
        return 0U;
    }
    const tabos_ssize_t count = tabos_fs_read(file, buffer, capacity - 1U);
    (void) tabos_fs_close(file);
    const size_t length = count > 0 ? (size_t) count : 0U;
    buffer[length]      = '\0';
    return length;
}

static void write_config(const char* text)
{
    (void) tabos_fs_mkdir("T:/etc", 0755U);
    const tabos_fd_t file = tabos_fs_open("T:/etc/wifi.conf", TABOS_O_WRONLY | TABOS_O_CREAT | TABOS_O_TRUNC, 0600U);
    expect(file >= 0, "test config opens");
    expect(tabos_fs_write(file, text, strlen(text)) == (tabos_ssize_t) strlen(text), "test config writes");
    expect(tabos_fs_close(file) == 0, "test config closes");
}

static void go_online(const char* ssid, const char* password)
{
    expect(network_service_connect(ssid, password, true), "connection starts");
    test_platform_network_set_state(PLATFORM_NETWORK_ONLINE, NULL);
    network_service_update();
}

int main(void)
{
    expect(mkdtemp(storage_root) != NULL && filesystem_init(), "storage initializes");
    expect(network_service_save_current() == NETWORK_PROFILE_NOT_CONNECTED &&
               network_service_forget() == NETWORK_PROFILE_NOT_CONNECTED,
           "profile operations require an initialized service");

    write_config("# lab notes\nversion=1\n[wifi]\nssid=\"home\"\npassword=\"home-secret\"\nname=\"Lab-TabOS\"\n"
                 "auto_connect=false\n[dns]\nservers=\"future\"\n");
    expect(network_service_init(), "service initializes from saved profile");
    network_status_t status;
    expect(network_service_status(&status) && strcmp(status.saved_ssid, "home") == 0 && status.config_available &&
               !status.auto_connect && strcmp(status.hostname, "Lab-TabOS") == 0,
           "status reports the saved network and policy");

    char before[NETWORK_CONFIG_FILE_MAX + 1U];
    char after[NETWORK_CONFIG_FILE_MAX + 1U];
    const size_t before_length = read_config(before, sizeof(before));
    expect(network_service_connect("cafe", "wrong-password", status.auto_connect), "volatile connection starts");
    test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "auth failed");
    network_service_update();
    expect(network_service_save_current() == NETWORK_PROFILE_NOT_CONNECTED,
           "a failed connection cannot be saved");
    expect(read_config(after, sizeof(after)) == before_length && strcmp(before, after) == 0,
           "failed volatile connection leaves wifi.conf byte-identical");
    expect(network_service_status(&status) && strcmp(status.ssid, "cafe") == 0 &&
               strcmp(status.saved_ssid, "home") == 0,
           "status separates the attempted network from the saved one");

    go_online("cafe", "cafe-secret");
    expect(network_service_save_current() == NETWORK_PROFILE_OK, "online connection saves");
    network_config_t saved;
    expect(network_config_load(&saved) == NETWORK_CONFIG_OK && strcmp(saved.ssid, "cafe") == 0 &&
               strcmp(saved.password, "cafe-secret") == 0 && strcmp(saved.name, "Lab-TabOS") == 0 &&
               !saved.auto_connect,
           "save writes current credentials and keeps hostname and policy");
    read_config(after, sizeof(after));
    expect(strstr(after, "# lab notes") != NULL && strstr(after, "[dns]\nservers=\"future\"") != NULL &&
               strstr(after, "home-secret") == NULL,
           "save keeps comments and unknown sections and drops the old password");
    expect(network_service_status(&status) && strcmp(status.saved_ssid, "cafe") == 0 && status.config_available,
           "status reports the newly saved network");

    expect(network_service_set_auto_connect(true) == NETWORK_PROFILE_OK &&
               network_config_load(&saved) == NETWORK_CONFIG_OK && saved.auto_connect &&
               strcmp(saved.password, "cafe-secret") == 0,
           "autoconnect change keeps credentials");
    expect(network_service_status(&status) && status.auto_connect, "status reports autoconnect change");

    expect(network_service_forget() == NETWORK_PROFILE_OK, "forget succeeds");
    expect(network_config_load(&saved) == NETWORK_CONFIG_NO_PROFILE && strcmp(saved.name, "Lab-TabOS") == 0 &&
               saved.auto_connect,
           "forget removes the network and keeps hostname and policy");
    read_config(after, sizeof(after));
    expect(strstr(after, "cafe") == NULL && strstr(after, "# lab notes") != NULL, "forget erases credentials");
    expect(network_service_status(&status) && status.saved_ssid[0] == '\0' && !status.config_available &&
               status.state == NETWORK_STATE_ONLINE,
           "forget clears the saved network without disconnecting");
    expect(network_service_forget() == NETWORK_PROFILE_OK, "forgetting twice is harmless");

    network_service_shutdown();
    expect(network_service_init(), "service restarts without a saved network");
    expect(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE && status.saved_ssid[0] == '\0' &&
               strcmp(status.hostname, "Lab-TabOS") == 0 && status.last_failure[0] == '\0',
           "restart without a profile keeps hostname and reports no failure");

    write_config("version=1\n[wifi]\npassword=\"orphan\"\n");
    go_online("cafe", "cafe-secret");
    const size_t damaged_length = read_config(before, sizeof(before));
    expect(network_service_save_current() == NETWORK_PROFILE_INVALID &&
               network_service_set_auto_connect(false) == NETWORK_PROFILE_INVALID,
           "damaged wifi.conf is reported");
    expect(read_config(after, sizeof(after)) == damaged_length && strcmp(before, after) == 0,
           "damaged wifi.conf is never overwritten");

    expect(tabos_fs_unlink("T:/etc/wifi.conf") == 0, "test config removed");
    expect(network_service_set_auto_connect(false) == NETWORK_PROFILE_OK &&
               network_config_load(&saved) == NETWORK_CONFIG_NO_PROFILE && !saved.auto_connect &&
               strcmp(saved.name, "Lab-TabOS") == 0,
           "autoconnect policy can be written without a saved network");

    network_service_shutdown();
    filesystem_shutdown();
    return failures == 0 ? 0 : 1;
}
