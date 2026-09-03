#include <tabos/camera.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int result(int value)
{
    if (value < 0) {
        errno = -value;
        return -1;
    }
    return value;
}

int tabos_camera_get_info(tabos_device_id_t device_id, tabos_camera_info_t* info)
{
    if (info == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->camera_info == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return result(tabos_runtime_api->camera_info(device_id, info));
}

tabos_camera_stream_t tabos_camera_open(const tabos_camera_config_t* config)
{
    if (config == NULL) {
        errno = EINVAL;
        return TABOS_CAMERA_STREAM_INVALID;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->camera_open == NULL) {
        errno = ENOSYS;
        return TABOS_CAMERA_STREAM_INVALID;
    }
    const int value = tabos_runtime_api->camera_open(config);
    if (value < 0) {
        errno = -value;
        return TABOS_CAMERA_STREAM_INVALID;
    }
    return value;
}

int tabos_camera_close(tabos_camera_stream_t stream)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->camera_close != NULL ?
               result(tabos_runtime_api->camera_close(stream)) :
               (errno = ENOSYS, -1);
}

int tabos_camera_acquire(tabos_camera_stream_t stream, tabos_camera_frame_t* frame)
{
    if (frame == NULL) {
        errno = EINVAL;
        return -1;
    }
    return tabos_runtime_api != NULL && tabos_runtime_api->camera_acquire != NULL ?
               result(tabos_runtime_api->camera_acquire(stream, frame)) :
               (errno = ENOSYS, -1);
}

int tabos_camera_copy(tabos_camera_stream_t stream, tabos_camera_lease_t lease, uint32_t offset, void* buffer,
                      uint32_t capacity)
{
    if (buffer == NULL || capacity == 0U) {
        errno = EINVAL;
        return -1;
    }
    return tabos_runtime_api != NULL && tabos_runtime_api->camera_copy != NULL ?
               result(tabos_runtime_api->camera_copy(stream, lease, offset, buffer, capacity)) :
               (errno = ENOSYS, -1);
}

int tabos_camera_release(tabos_camera_stream_t stream, tabos_camera_lease_t lease)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->camera_release != NULL ?
               result(tabos_runtime_api->camera_release(stream, lease)) :
               (errno = ENOSYS, -1);
}
