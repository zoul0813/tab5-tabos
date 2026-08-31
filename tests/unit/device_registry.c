#include <tabos/internal/device_registry.h>
#include <tabos/filesystem.h>

#include <stdio.h>
#include <string.h>

static device_registry_registration_t registration(const char* name, tabos_device_class_t device_class)
{
    return (device_registry_registration_t) {
        .name         = name,
        .driver       = "test-driver",
        .device_class = device_class,
        .state        = TABOS_DEVICE_READY,
        .features     = UINT64_C(0x100000002),
    };
}

static int test_registration_and_lookup(void)
{
    if (!device_registry_init() || device_registry_count() != 0U) {
        return 1;
    }

    const device_registry_registration_t display = registration(TABOS_DEVICE_NAME_DISPLAY, TABOS_DEVICE_CLASS_DISPLAY);
    const tabos_device_id_t display_id           = device_registry_register(&display);
    if (display_id == TABOS_DEVICE_ID_INVALID || device_registry_count() != 1U) {
        return 1;
    }

    tabos_device_info_t info;
    if (!device_registry_get(display_id, &info) || info.id != display_id ||
        info.device_class != TABOS_DEVICE_CLASS_DISPLAY || info.state != TABOS_DEVICE_READY ||
        info.features != display.features || info.last_error != 0 ||
        strcmp(info.name, TABOS_DEVICE_NAME_DISPLAY) != 0 || strcmp(info.driver, "test-driver") != 0) {
        return 1;
    }
    if (!device_registry_at(0U, &info) || info.id != display_id || device_registry_at(1U, &info) ||
        !device_registry_find(TABOS_DEVICE_NAME_DISPLAY, &info) || info.id != display_id ||
        device_registry_find("missing0", &info)) {
        return 1;
    }

    if (!device_registry_set_state(display_id, TABOS_DEVICE_FAULT, -27) || !device_registry_get(display_id, &info) ||
        info.state != TABOS_DEVICE_FAULT || info.last_error != -27) {
        return 1;
    }
    if (!device_registry_set_state(display_id, TABOS_DEVICE_OFFLINE, 0) || !device_registry_get(display_id, &info) ||
        info.state != TABOS_DEVICE_OFFLINE || info.last_error != 0) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    device_registry_registration_t value = registration("display1", TABOS_DEVICE_CLASS_DISPLAY);
    if (device_registry_register(NULL) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }

    value.name = "";
    if (device_registry_register(&value) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    value.name = "name-is-too-long0";
    if (device_registry_register(&value) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    value        = registration("display1", TABOS_DEVICE_CLASS_DISPLAY);
    value.driver = "driver-name-is-far-too-long-for-registry";
    if (device_registry_register(&value) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    value = registration("display1", TABOS_DEVICE_CLASS_COUNT);
    if (device_registry_register(&value) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    value       = registration("display1", TABOS_DEVICE_CLASS_DISPLAY);
    value.state = TABOS_DEVICE_STATE_COUNT;
    if (device_registry_register(&value) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }

    value = registration(TABOS_DEVICE_NAME_DISPLAY, TABOS_DEVICE_CLASS_DISPLAY);
    return device_registry_register(&value) == TABOS_DEVICE_ID_INVALID ? 0 : 1;
}

static int test_tombstones_and_capacity(tabos_device_id_t display_id)
{
    if (!device_registry_remove(display_id) || device_registry_remove(display_id) ||
        device_registry_set_state(display_id, TABOS_DEVICE_READY, 0)) {
        return 1;
    }
    tabos_device_info_t info;
    if (device_registry_get(display_id, &info) || device_registry_find(TABOS_DEVICE_NAME_DISPLAY, &info) ||
        device_registry_count() != 0U) {
        return 1;
    }

    device_registry_registration_t duplicate = registration(TABOS_DEVICE_NAME_DISPLAY, TABOS_DEVICE_CLASS_DISPLAY);
    if (device_registry_register(&duplicate) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }

    char names[DEVICE_REGISTRY_CAPACITY - 1U][TABOS_DEVICE_NAME_MAX + 1U];
    for (size_t index = 0U; index < DEVICE_REGISTRY_CAPACITY - 1U; ++index) {
        (void) snprintf(names[index], sizeof(names[index]), "sensor%zu", index);
        const device_registry_registration_t sensor = registration(names[index], TABOS_DEVICE_CLASS_SENSOR);
        if (device_registry_register(&sensor) == TABOS_DEVICE_ID_INVALID) {
            return 1;
        }
    }
    if (device_registry_count() != DEVICE_REGISTRY_CAPACITY - 1U) {
        return 1;
    }
    const device_registry_registration_t overflow = registration("overflow0", TABOS_DEVICE_CLASS_SENSOR);
    if (device_registry_register(&overflow) != TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    for (size_t index = 0U; index < DEVICE_REGISTRY_CAPACITY - 1U; ++index) {
        if (!device_registry_at(index, &info) || strcmp(info.name, names[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

static int test_lifecycle_events(void)
{
    static const int owner_a;
    static const int owner_b;
    const tabos_device_subscription_t first  = device_registry_subscribe(&owner_a);
    const tabos_device_subscription_t second = device_registry_subscribe(&owner_b);
    if (first < 0 || second < 0 || first == second) {
        return 1;
    }
    if (device_registry_event_pending(&owner_a, first) != 0 ||
        device_registry_event_pending(&owner_b, first) != -TABOS_EBADF) {
        return 1;
    }

    const device_registry_registration_t display = registration(TABOS_DEVICE_NAME_DISPLAY, TABOS_DEVICE_CLASS_DISPLAY);
    const tabos_device_id_t id                   = device_registry_register(&display);
    tabos_device_event_t event;
    if (id == TABOS_DEVICE_ID_INVALID || device_registry_event_pending(&owner_a, first) != 1 ||
        device_registry_read_event(&owner_a, first, &event) != 0 || event.type != TABOS_DEVICE_EVENT_ADDED ||
        event.flags != 0U || event.device.id != id || device_registry_event_pending(&owner_a, first) != 0 ||
        device_registry_read_event(&owner_a, first, &event) != -TABOS_EAGAIN ||
        device_registry_read_event(&owner_b, first, &event) != -TABOS_EBADF) {
        return 1;
    }
    if (!device_registry_set_state(id, TABOS_DEVICE_OFFLINE, -5) ||
        device_registry_read_event(&owner_a, first, &event) != 0 || event.type != TABOS_DEVICE_EVENT_OFFLINE ||
        event.device.state != TABOS_DEVICE_OFFLINE || event.device.last_error != -5 ||
        !device_registry_set_state(id, TABOS_DEVICE_READY, 0) ||
        device_registry_read_event(&owner_a, first, &event) != 0 || event.type != TABOS_DEVICE_EVENT_READY ||
        !device_registry_set_state(id, TABOS_DEVICE_FAULT, -9) ||
        device_registry_read_event(&owner_a, first, &event) != 0 || event.type != TABOS_DEVICE_EVENT_FAULT ||
        !device_registry_remove(id) || device_registry_read_event(&owner_a, first, &event) != 0 ||
        event.type != TABOS_DEVICE_EVENT_REMOVED || event.device.id != id) {
        return 1;
    }

    while (device_registry_read_event(&owner_b, second, &event) == 0) {}
    const device_registry_registration_t sensor = registration("sensor0", TABOS_DEVICE_CLASS_SENSOR);
    const tabos_device_id_t sensor_id           = device_registry_register(&sensor);
    if (sensor_id == TABOS_DEVICE_ID_INVALID) {
        return 1;
    }
    for (size_t index = 0U; index < DEVICE_EVENT_QUEUE_CAPACITY + 1U; ++index) {
        const tabos_device_state_t state = (index & 1U) == 0U ? TABOS_DEVICE_OFFLINE : TABOS_DEVICE_READY;
        if (!device_registry_set_state(sensor_id, state, 0)) {
            return 1;
        }
    }
    if (device_registry_read_event(&owner_b, second, &event) != 0 ||
        (event.flags & TABOS_DEVICE_EVENT_OVERFLOW) == 0U) {
        return 1;
    }
    if (!device_registry_unsubscribe(&owner_a, first) ||
        device_registry_read_event(&owner_a, first, &event) != -TABOS_EBADF ||
        device_registry_event_pending(&owner_a, first) != -TABOS_EBADF) {
        return 1;
    }
    device_registry_unsubscribe_owner(&owner_b);
    return device_registry_read_event(&owner_b, second, &event) == -TABOS_EBADF ? 0 : 1;
}

int main(void)
{
    if (device_registry_count() != 0U || test_registration_and_lookup() != 0 || test_validation() != 0) {
        return 1;
    }

    tabos_device_info_t display;
    if (!device_registry_find(TABOS_DEVICE_NAME_DISPLAY, &display)) {
        return 1;
    }
    const tabos_device_id_t old_id = display.id;
    if (test_tombstones_and_capacity(old_id) != 0) {
        return 1;
    }

    device_registry_shutdown();
    if (device_registry_count() != 0U || !device_registry_init() || device_registry_get(old_id, &display)) {
        return 1;
    }
    const device_registry_registration_t replacement =
        registration(TABOS_DEVICE_NAME_DISPLAY, TABOS_DEVICE_CLASS_DISPLAY);
    const tabos_device_id_t new_id = device_registry_register(&replacement);
    if (new_id == TABOS_DEVICE_ID_INVALID || new_id == old_id) {
        return 1;
    }
    device_registry_shutdown();
    if (!device_registry_init() || test_lifecycle_events() != 0) {
        return 1;
    }
    device_registry_shutdown();
    return 0;
}
