#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/filesystem.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/network_config.h>
#include <tabos/platform/storage_backend.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char storage_root[] = "/tmp/tabos-network-config-XXXXXX";

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
    *name      = "Network config test";
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

static void expect_saved(const char* ssid)
{
    network_config_t loaded;
    assert(network_config_load(&loaded) == NETWORK_CONFIG_OK);
    assert(strcmp(loaded.ssid, ssid) == 0);
    tabos_stat_t status;
    assert(tabos_fs_stat("T:/etc/.wifi.conf.tmp", &status) == -1);
}

int main(void)
{
    assert(mkdtemp(storage_root) != NULL && filesystem_init());
    network_config_t config = {.ssid = "first", .password = "secret", .name = "TabOS", .auto_connect = true};
    assert(network_config_save(NULL) == NETWORK_CONFIG_INVALID);
    assert(network_config_save(&config) == NETWORK_CONFIG_OK);
    expect_saved("first");
    tabos_fd_t file            = tabos_fs_open("T:/etc/wifi.conf", TABOS_O_WRONLY | TABOS_O_APPEND, 0U);
    static const char future[] = "\n[future]\noption=preserved\n";
    assert(file >= 0 && tabos_fs_write(file, future, sizeof(future) - 1U) == (tabos_ssize_t) sizeof(future) - 1);
    assert(tabos_fs_close(file) == 0);
    strcpy(config.ssid, "second");
    assert(network_config_save(&config) == NETWORK_CONFIG_OK);
    expect_saved("second");
    file                                        = tabos_fs_open("T:/etc/wifi.conf", TABOS_O_RDONLY, 0U);
    char contents[NETWORK_CONFIG_FILE_MAX + 1U] = {0};
    assert(file >= 0 && tabos_fs_read(file, contents, sizeof(contents) - 1U) > 0);
    assert(tabos_fs_close(file) == 0 && strstr(contents, future) != NULL);
    assert(tabos_fs_mkdir("T:/etc/.wifi.conf.tmp", 0755U) == 0);
    strcpy(config.ssid, "third");
    assert(network_config_save(&config) == NETWORK_CONFIG_IO_ERROR);
    assert(tabos_fs_rmdir("T:/etc/.wifi.conf.tmp") == 0);
    expect_saved("second");
    assert(network_config_save(&config) == NETWORK_CONFIG_OK);
    expect_saved("third");
    assert(tabos_fs_unlink("T:/etc/wifi.conf") == 0 && tabos_fs_rmdir("T:/etc") == 0);
    filesystem_shutdown();
    assert(rmdir(storage_root) == 0);
    return 0;
}
