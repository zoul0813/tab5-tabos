#include <tabos/platform/storage_backend.h>

#include <bsp/m5stack_tab5.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <tabos/filesystem.h>
#include <tabos/config/filesystem.h>

#include <string.h>

static const char* const TAG = "tabos_storage";
static bool mounted;

int storage_backend_sync(char letter)
{
    if (letter != 'T' || !mounted) {
        return TABOS_ENODEV;
    }
    /* Pinned FatFs synchronizes namespace mutations through sync_fs; open-file
     * data/metadata was flushed with VFS fsync before this call. SDMMC writes
     * are synchronous (CTRL_SYNC has no pending driver queue). Check card status
     * without unmounting, closing descriptors, or changing mount identity. */
    sdmmc_card_t* card = bsp_sdcard_get_handle();
    return card != NULL && sdmmc_get_status(card) == ESP_OK ? 0 : TABOS_EIO;
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable, const char** name)
{
    if (index != 0U || letter == NULL || root == NULL || removable == NULL || name == NULL) {
        return false;
    }
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = TABOS_FILESYSTEM_MAX_FILES,
        .allocation_unit_size   = 16U * 1024U,
    };
    bsp_sdcard_cfg_t configuration = {
        .mount = &mount_config,
    };
    const esp_err_t result = bsp_sdcard_sdmmc_mount(&configuration);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "microSD not mounted: %s", esp_err_to_name(result));
        return false;
    }
    static const char mount_point[] = BSP_SD_MOUNT_POINT;
    if (sizeof(mount_point) > root_size) {
        (void) bsp_sdcard_unmount();
        return false;
    }
    memcpy(root, mount_point, sizeof(mount_point));
    *letter    = 'T';
    *removable = true;
    *name      = "FAT";
    mounted    = true;
    ESP_LOGI(TAG, "Mounted microSD at %s", mount_point);
    return true;
}

void storage_backend_unmount(char letter)
{
    if (letter != 'T') {
        return;
    }
    if (!mounted) {
        return;
    }
    (void) bsp_sdcard_unmount();
    mounted = false;
}

bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes)
{
    if (letter != 'T' || !mounted || total_bytes == NULL || free_bytes == NULL) {
        return false;
    }
    return esp_vfs_fat_info(BSP_SD_MOUNT_POINT, total_bytes, free_bytes) == ESP_OK;
}
