#include <tester/test.h>

#include <tabos/device.h>
#include <tabos/pointer.h>
#include <tabos/wait.h>

#include <errno.h>

void tester_test_pointer(tester_context_t* context)
{
    tabos_device_info_t device;
    const bool device_ok = tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &device) == 0 &&
                           device.device_class == TABOS_DEVICE_CLASS_POINTER && device.state == TABOS_DEVICE_READY &&
                           (device.features & TABOS_DEVICE_FEATURE_POINTER_TOUCH) != 0U;
    tester_expect(context, device_ok, "touch0 reports ready pointer input");
    if (!device_ok) {
        errno = 0;
        tester_expect(context, tabos_pointer_open(TABOS_DEVICE_ID_INVALID) == TABOS_POINTER_STREAM_INVALID,
                      "unavailable pointer rejects open");
        return;
    }
    const tabos_pointer_stream_t stream = tabos_pointer_open(device.id);
    const tabos_wait_source_t source    = tabos_pointer_wait_source(stream);
    tabos_wait_item_t item              = {.source = source, .events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP};
    tester_expect(context, stream != TABOS_POINTER_STREAM_INVALID && source != TABOS_WAIT_SOURCE_INVALID,
                  "pointer stream and wait source open");
    tester_expect(context, tabos_wait(&item, 1U, 0U) == 0 && item.returned_events == 0U,
                  "idle pointer source is not readable");
    tabos_pointer_event_t event;
    errno = 0;
    tester_expect(context, tabos_pointer_read(stream, &event) < 0 && errno == EAGAIN,
                  "empty pointer read is nonblocking");
    tester_expect(context, tabos_pointer_close(stream) == 0 && tabos_pointer_close(stream) < 0 && errno == EBADF,
                  "pointer close stales stream handle");
    errno = 0;
    tester_expect(context, tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF, "closed pointer wait source is stale");
}
