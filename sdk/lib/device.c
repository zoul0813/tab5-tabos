#include <tabos/device.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int api_result(int result)
{
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

size_t tabos_device_count(void)
{
    return tabos_runtime_api != NULL && tabos_runtime_api->device_count != NULL ? tabos_runtime_api->device_count() :
                                                                                  0U;
}

int tabos_device_at(size_t index, tabos_device_info_t* info)
{
    if (info == NULL || index > UINT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_at == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->device_at((uint32_t) index, info));
}

int tabos_device_get(tabos_device_id_t id, tabos_device_info_t* info)
{
    if (info == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_get == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->device_get(id, info));
}

int tabos_device_find(const char* name, tabos_device_info_t* info)
{
    if (name == NULL || name[0] == '\0' || info == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_find == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->device_find(name, info));
}

tabos_device_subscription_t tabos_device_subscribe(void)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_subscribe == NULL) {
        errno = ENOSYS;
        return TABOS_DEVICE_SUBSCRIPTION_INVALID;
    }
    return api_result(tabos_runtime_api->device_subscribe());
}

int tabos_device_subscription_close(tabos_device_subscription_t subscription)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_subscription_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->device_subscription_close(subscription));
}

int tabos_device_event_read(tabos_device_subscription_t subscription, tabos_device_event_t* event)
{
    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->device_event_read == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->device_event_read(subscription, event));
}
