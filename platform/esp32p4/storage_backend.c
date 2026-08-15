#include <tabos/platform/storage_backend.h>

#include <bsp/m5stack_tab5.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>

#include <string.h>

static const char *const TAG = "tabos_storage";
static bool mounted;

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char *letter, char *root, size_t root_size,
                           bool *removable, const char **name)
{
    if (index != 0U || letter == NULL || root == NULL || removable == NULL || name == NULL) {
        return false;
    }
    const esp_err_t result = bsp_sdcard_mount();
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "microSD not mounted: %s", esp_err_to_name(result));
        return false;
    }
    static const char mount_point[] = BSP_SD_MOUNT_POINT;
    if (sizeof(mount_point) > root_size) {
        (void)bsp_sdcard_unmount();
        return false;
    }
    memcpy(root, mount_point, sizeof(mount_point));
    *letter = 'T';
    *removable = true;
    *name = "FAT";
    mounted = true;
    ESP_LOGI(TAG, "Mounted microSD at %s", mount_point);
    return true;
}

void storage_backend_unmount(char letter)
{
    if (letter != 'T') return;
    if (!mounted) return;
    (void)bsp_sdcard_unmount();
    mounted = false;
}

bool storage_backend_info(char letter, uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (letter != 'T' || !mounted || total_bytes == NULL || free_bytes == NULL) return false;
    return esp_vfs_fat_info(BSP_SD_MOUNT_POINT, total_bytes, free_bytes) == ESP_OK;
}
