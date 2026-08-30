#include <tester/test.h>

#include <tabos/device.h>

#include <errno.h>
#include <string.h>

static bool same_device(const tabos_device_info_t* left, const tabos_device_info_t* right)
{
    return left->id == right->id && left->device_class == right->device_class && left->state == right->state &&
           left->features == right->features && left->last_error == right->last_error &&
           strcmp(left->name, right->name) == 0 && strcmp(left->driver, right->driver) == 0;
}

void tester_test_device(tester_context_t* context)
{
    const size_t count = tabos_device_count();
    tester_expect(context, count > 0U, "device enumeration reports present devices");

    bool entries_valid         = count > 0U;
    bool ids_unique            = true;
    bool display_found         = false;
    bool storage_found         = false;
    tabos_device_id_t ids[32]  = {0};
    const size_t checked_count = count < sizeof(ids) / sizeof(ids[0]) ? count : sizeof(ids) / sizeof(ids[0]);
    for (size_t index = 0U; index < checked_count; ++index) {
        tabos_device_info_t indexed;
        tabos_device_info_t by_id;
        tabos_device_info_t by_name;
        if (tabos_device_at(index, &indexed) != 0 || indexed.id == TABOS_DEVICE_ID_INVALID || indexed.name[0] == '\0' ||
            indexed.driver[0] == '\0' || indexed.device_class >= TABOS_DEVICE_CLASS_COUNT ||
            indexed.state >= TABOS_DEVICE_STATE_COUNT || tabos_device_get(indexed.id, &by_id) != 0 ||
            tabos_device_find(indexed.name, &by_name) != 0 || !same_device(&indexed, &by_id) ||
            !same_device(&indexed, &by_name)) {
            entries_valid = false;
            continue;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (ids[previous] == indexed.id) {
                ids_unique = false;
            }
        }
        ids[index]    = indexed.id;
        display_found = display_found || strcmp(indexed.name, TABOS_DEVICE_NAME_DISPLAY) == 0;
        storage_found = storage_found || strcmp(indexed.name, TABOS_DEVICE_NAME_STORAGE) == 0;
    }
    tester_expect(context, count <= sizeof(ids) / sizeof(ids[0]) && entries_valid,
                  "indexed devices round-trip through ID and name lookup");
    tester_expect(context, ids_unique, "enumerated device IDs are unique");
    tester_expect(context, display_found, "display0 is registered");
    tester_expect(context, storage_found, "storage0 is registered");

    const tabos_device_subscription_t subscription = tabos_device_subscribe();
    tester_expect(context, subscription != TABOS_DEVICE_SUBSCRIPTION_INVALID, "lifecycle subscription opens");
    tabos_device_event_t event;
    errno                  = 0;
    const int read_result  = tabos_device_event_read(subscription, &event);
    const bool queue_empty = read_result < 0 && errno == EAGAIN;
    const bool event_valid = read_result == 0 && event.type < TABOS_DEVICE_EVENT_TYPE_COUNT &&
                             event.device.id != TABOS_DEVICE_ID_INVALID && event.device.name[0] != '\0' &&
                             event.device.driver[0] != '\0' && event.device.device_class < TABOS_DEVICE_CLASS_COUNT &&
                             event.device.state < TABOS_DEVICE_STATE_COUNT;
    tester_expect(context, queue_empty || event_valid, "lifecycle read returns EAGAIN or a valid event");
    tester_expect(context, tabos_device_subscription_close(subscription) == 0, "lifecycle subscription closes");
    errno                  = 0;
    const int stale_result = tabos_device_event_read(subscription, &event);
    tester_expect(context, stale_result < 0 && errno == EBADF, "closed lifecycle subscription is stale");
}
