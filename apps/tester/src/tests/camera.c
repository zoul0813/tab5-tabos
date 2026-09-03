#include <tester/test.h>

#include <tabos/camera.h>
#include <tabos/device.h>
#include <tabos/wait.h>

#include <errno.h>

void tester_test_camera(tester_context_t* context)
{
    tabos_device_info_t device;
    const bool found = tabos_device_find(TABOS_DEVICE_NAME_CAMERA, &device) == 0 &&
                       device.device_class == TABOS_DEVICE_CLASS_CAMERA &&
                       (device.features & TABOS_DEVICE_FEATURE_CAMERA_CAPTURE) != 0U;
    tester_expect(context, found, "camera0 reports capture device");
    if (!found) {
        return;
    }

    tabos_camera_info_t info;
    tester_expect(context,
                  tabos_camera_get_info(device.id, &info) == 0 && info.device_id == device.id &&
                      (info.formats & TABOS_CAMERA_FORMAT_FLAG_RAW8) != 0U,
                  "camera capabilities use copied metadata");
    const tabos_camera_config_t config = {.device_id = device.id,
                                          .format    = TABOS_CAMERA_FORMAT_RAW8,
                                          .width     = info.max_width < 64U ? info.max_width : 64U,
                                          .height    = info.max_height < 48U ? info.max_height : 48U,
                                          .fps       = info.max_fps < 10U ? info.max_fps : 10U};
    const tabos_camera_stream_t stream = tabos_camera_open(&config);
    if (device.state != TABOS_DEVICE_READY) {
        tester_expect(context, stream == TABOS_CAMERA_STREAM_INVALID && errno == ENODEV,
                      "offline camera rejects capture open");
        return;
    }
    const tabos_wait_source_t source = tabos_camera_wait_source(stream);
    tabos_wait_item_t item = {.source = source, .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
    tester_expect(context, stream != TABOS_CAMERA_STREAM_INVALID && source != TABOS_WAIT_SOURCE_INVALID,
                  "camera stream and wait source open");
    tester_expect(context, tabos_wait(&item, 1U, 500U) == 1 && item.returned_events == TABOS_WAIT_READABLE,
                  "camera frame becomes readable");
    tabos_camera_frame_t frame;
    uint8_t bytes[16];
    tester_expect(context,
                  tabos_camera_acquire(stream, &frame) == 0 && frame.lease != TABOS_CAMERA_LEASE_INVALID &&
                      frame.size_bytes > 0U,
                  "camera acquire returns lease and metadata");
    tester_expect(context, tabos_camera_copy(stream, frame.lease, 0U, bytes, sizeof(bytes)) == sizeof(bytes),
                  "camera lease copies bounded frame bytes");
    tester_expect(context,
                  tabos_camera_release(stream, frame.lease) == 0 && tabos_camera_release(stream, frame.lease) < 0 &&
                      errno == EBADF,
                  "camera release stales lease");
    tester_expect(context, tabos_camera_close(stream) == 0, "camera stream closes");
    errno = 0;
    tester_expect(context, tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF, "closed camera wait source is stale");
}
