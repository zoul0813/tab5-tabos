#include <tabos/camera.h>
#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stdio.h>

const tabos_elf_api_t* tabos_runtime_api;

static int camera_info_call(tabos_device_id_t device_id, tabos_camera_info_t* info)
{
    info->device_id = device_id;
    return 0;
}

static int camera_open_call(const tabos_camera_config_t* config)
{
    return config->width == 4U ? 7 : -TABOS_EINVAL;
}

static int camera_close_call(int stream)
{
    return stream == 7 ? 0 : -TABOS_EBADF;
}
static int camera_acquire_call(int stream, tabos_camera_frame_t* frame)
{
    frame->lease = 9U;
    return stream == 7 ? 0 : -TABOS_EBADF;
}
static int camera_copy_call(int stream, tabos_camera_lease_t lease, uint32_t offset, void* buffer, uint32_t capacity)
{
    (void) offset;
    *(uint8_t*) buffer = 42U;
    return stream == 7 && lease == 9U && capacity > 0U ? 1 : -TABOS_EBADF;
}
static int camera_release_call(int stream, tabos_camera_lease_t lease)
{
    return stream == 7 && lease == 9U ? 0 : -TABOS_EBADF;
}
static int camera_wait_call(int stream)
{
    return stream == 7 ? 11 : -TABOS_EBADF;
}

int main(void)
{
    const tabos_elf_api_t api = {.abi_version        = TABOS_ELF_API_VERSION,
                                 .camera_info        = camera_info_call,
                                 .camera_open        = camera_open_call,
                                 .camera_close       = camera_close_call,
                                 .camera_acquire     = camera_acquire_call,
                                 .camera_copy        = camera_copy_call,
                                 .camera_release     = camera_release_call,
                                 .camera_wait_source = camera_wait_call};
    tabos_runtime_api         = &api;
    tabos_camera_info_t info;
    const tabos_camera_config_t config = {
        .device_id = 2U, .format = TABOS_CAMERA_FORMAT_RAW8, .width = 4U, .height = 2U, .fps = 10U};
    tabos_camera_frame_t frame;
    uint8_t byte                       = 0U;
    const tabos_camera_stream_t stream = tabos_camera_open(&config);
    if (tabos_camera_get_info(2U, &info) != 0 || info.device_id != 2U || stream != 7 ||
        tabos_camera_wait_source(stream) != 11 || tabos_camera_acquire(stream, &frame) != 0 || frame.lease != 9U ||
        tabos_camera_copy(stream, frame.lease, 0U, &byte, 1U) != 1 || byte != 42U ||
        tabos_camera_release(stream, frame.lease) != 0 || tabos_camera_close(stream) != 0) {
        fprintf(stderr, "camera SDK forwarding failed\n");
        return 1;
    }
    tabos_runtime_api = NULL;
    if (tabos_camera_open(&config) != TABOS_CAMERA_STREAM_INVALID || errno != ENOSYS) {
        fprintf(stderr, "camera SDK unavailable handling failed\n");
        return 1;
    }
    return 0;
}
