#include <tabos/device.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;

static tabos_device_info_t expected = {
    .id           = 65U,
    .device_class = TABOS_DEVICE_CLASS_DISPLAY,
    .state        = TABOS_DEVICE_READY,
    .features     = 7U,
    .name         = TABOS_DEVICE_NAME_DISPLAY,
    .driver       = "test-display",
};

static uint32_t device_count(void)
{
    return 1U;
}

static int device_at(uint32_t index, tabos_device_info_t* info)
{
    if (index != 0U) {
        return -ENOENT;
    }
    *info = expected;
    return 0;
}

static int device_get(tabos_device_id_t id, tabos_device_info_t* info)
{
    return id == expected.id ? device_at(0U, info) : -ENOENT;
}

static int device_find(const char* name, tabos_device_info_t* info)
{
    return strcmp(name, expected.name) == 0 ? device_at(0U, info) : -ENOENT;
}

static int device_subscribe(void)
{
    return 17;
}

static int device_close(int subscription)
{
    return subscription == 17 ? 0 : -EBADF;
}

static int device_read(int subscription, tabos_device_event_t* event)
{
    if (subscription != 17) {
        return -EBADF;
    }
    *event = (tabos_device_event_t) {.type = TABOS_DEVICE_EVENT_ADDED, .device = expected};
    return 0;
}

int main(void)
{
    const tabos_elf_api_t api = {
        .abi_version               = TABOS_ELF_API_VERSION,
        .device_count              = device_count,
        .device_at                 = device_at,
        .device_get                = device_get,
        .device_find               = device_find,
        .device_subscribe          = device_subscribe,
        .device_subscription_close = device_close,
        .device_event_read         = device_read,
    };
    tabos_runtime_api = &api;

    tabos_device_info_t info;
    tabos_device_event_t event;
    if (tabos_device_count() != 1U || tabos_device_at(0U, &info) != 0 || info.id != expected.id ||
        tabos_device_get(expected.id, &info) != 0 || tabos_device_find(TABOS_DEVICE_NAME_DISPLAY, &info) != 0 ||
        tabos_device_at(1U, &info) != -1 || errno != ENOENT || tabos_device_find(NULL, &info) != -1 ||
        errno != EINVAL) {
        return 1;
    }
    const tabos_device_subscription_t subscription = tabos_device_subscribe();
    if (subscription != 17 || tabos_device_event_read(subscription, &event) != 0 ||
        event.type != TABOS_DEVICE_EVENT_ADDED || event.device.id != expected.id ||
        tabos_device_subscription_close(subscription) != 0 || tabos_device_subscription_close(99) != -1 ||
        errno != EBADF) {
        return 1;
    }
    return 0;
}
