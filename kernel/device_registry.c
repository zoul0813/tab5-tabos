#include <tabos/internal/device_registry.h>

#include <tabos/filesystem.h>
#include <tabos/platform/platform.h>

#include <string.h>

enum {
    DEVICE_ID_SLOT_BITS      = 6,
    DEVICE_ID_SLOT_MASK      = (1U << DEVICE_ID_SLOT_BITS) - 1U,
    DEVICE_ID_GENERATION_MAX = UINT32_MAX >> DEVICE_ID_SLOT_BITS,
};

typedef struct {
        tabos_device_info_t info;
        bool used;
        bool present;
} device_registry_entry_t;

typedef struct {
        const void* owner;
        tabos_device_event_t events[DEVICE_EVENT_QUEUE_CAPACITY];
        uint32_t generation;
        size_t head;
        size_t count;
        bool used;
        bool overflow;
} device_subscription_t;

static device_registry_entry_t entries[DEVICE_REGISTRY_CAPACITY];
static device_subscription_t subscriptions[DEVICE_SUBSCRIPTION_CAPACITY];
static platform_mutex_t* registry_mutex;
static uint32_t registry_generation;
static uint32_t next_generation = 1U;
static size_t active_count;

enum {
    DEVICE_SUBSCRIPTION_INDEX_BITS     = 4,
    DEVICE_SUBSCRIPTION_INDEX_MASK     = (1U << DEVICE_SUBSCRIPTION_INDEX_BITS) - 1U,
    DEVICE_SUBSCRIPTION_GENERATION_MAX = INT32_MAX >> DEVICE_SUBSCRIPTION_INDEX_BITS,
};

_Static_assert((1U << DEVICE_SUBSCRIPTION_INDEX_BITS) == DEVICE_SUBSCRIPTION_CAPACITY,
               "subscription index bits must match capacity");

static void publish_event(tabos_device_event_type_t type, const tabos_device_info_t* info)
{
    const tabos_device_event_t event = {.type = type, .device = *info};
    for (size_t index = 0U; index < DEVICE_SUBSCRIPTION_CAPACITY; ++index) {
        device_subscription_t* subscription = &subscriptions[index];
        if (!subscription->used) {
            continue;
        }
        if (subscription->count == DEVICE_EVENT_QUEUE_CAPACITY) {
            subscription->head = (subscription->head + 1U) % DEVICE_EVENT_QUEUE_CAPACITY;
            --subscription->count;
            subscription->overflow = true;
        }
        const size_t tail          = (subscription->head + subscription->count) % DEVICE_EVENT_QUEUE_CAPACITY;
        subscription->events[tail] = event;
        ++subscription->count;
    }
}

static tabos_device_event_type_t state_event(tabos_device_state_t state)
{
    if (state == TABOS_DEVICE_READY) {
        return TABOS_DEVICE_EVENT_READY;
    }
    if (state == TABOS_DEVICE_OFFLINE) {
        return TABOS_DEVICE_EVENT_OFFLINE;
    }
    return TABOS_DEVICE_EVENT_FAULT;
}

static tabos_device_subscription_t subscription_handle(size_t index, uint32_t generation)
{
    return (tabos_device_subscription_t) ((generation << DEVICE_SUBSCRIPTION_INDEX_BITS) | (uint32_t) index);
}

static device_subscription_t* owned_subscription(const void* owner, tabos_device_subscription_t handle)
{
    if (owner == NULL || handle < 0) {
        return NULL;
    }
    const uint32_t value                = (uint32_t) handle;
    const size_t index                  = value & DEVICE_SUBSCRIPTION_INDEX_MASK;
    const uint32_t generation           = value >> DEVICE_SUBSCRIPTION_INDEX_BITS;
    device_subscription_t* subscription = &subscriptions[index];
    return subscription->used && subscription->owner == owner && subscription->generation == generation ? subscription :
                                                                                                          NULL;
}

static bool valid_class(tabos_device_class_t device_class)
{
    return device_class >= TABOS_DEVICE_CLASS_DISPLAY && device_class < TABOS_DEVICE_CLASS_COUNT;
}

static bool valid_state(tabos_device_state_t state)
{
    return state >= TABOS_DEVICE_READY && state < TABOS_DEVICE_STATE_COUNT;
}

static bool valid_text(const char* text, size_t maximum)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    size_t length = 0U;
    while (length <= maximum && text[length] != '\0') {
        ++length;
    }
    return length <= maximum;
}

static tabos_device_id_t make_id(size_t slot)
{
    return (registry_generation << DEVICE_ID_SLOT_BITS) | (uint32_t) (slot + 1U);
}

static bool decode_id(tabos_device_id_t id, size_t* slot)
{
    if (id == TABOS_DEVICE_ID_INVALID || id == 0U || slot == NULL) {
        return false;
    }
    const uint32_t slot_code  = id & DEVICE_ID_SLOT_MASK;
    const uint32_t generation = id >> DEVICE_ID_SLOT_BITS;
    if (generation != registry_generation || slot_code == 0U || slot_code > DEVICE_REGISTRY_CAPACITY) {
        return false;
    }
    *slot = slot_code - 1U;
    return true;
}

static device_registry_entry_t* active_entry(tabos_device_id_t id)
{
    size_t slot;
    if (!decode_id(id, &slot) || !entries[slot].used || !entries[slot].present || entries[slot].info.id != id) {
        return NULL;
    }
    return &entries[slot];
}

static bool name_used(const char* name)
{
    for (size_t index = 0U; index < DEVICE_REGISTRY_CAPACITY; ++index) {
        if (entries[index].used && strcmp(entries[index].info.name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool device_registry_init(void)
{
    if (registry_mutex != NULL) {
        return true;
    }
    registry_mutex = platform_mutex_create();
    if (registry_mutex == NULL) {
        return false;
    }
    memset(entries, 0, sizeof(entries));
    memset(subscriptions, 0, sizeof(subscriptions));
    active_count        = 0U;
    registry_generation = next_generation;
    ++next_generation;
    if (next_generation == 0U || next_generation > DEVICE_ID_GENERATION_MAX) {
        next_generation = 1U;
    }
    return true;
}

void device_registry_shutdown(void)
{
    if (registry_mutex == NULL) {
        return;
    }
    platform_mutex_t* mutex = registry_mutex;
    platform_mutex_lock(mutex);
    memset(entries, 0, sizeof(entries));
    memset(subscriptions, 0, sizeof(subscriptions));
    active_count        = 0U;
    registry_generation = 0U;
    registry_mutex      = NULL;
    platform_mutex_unlock(mutex);
    platform_mutex_destroy(mutex);
}

tabos_device_id_t device_registry_register(const device_registry_registration_t* registration)
{
    if (registry_mutex == NULL || registration == NULL || !valid_text(registration->name, TABOS_DEVICE_NAME_MAX) ||
        !valid_text(registration->driver, TABOS_DEVICE_DRIVER_MAX) || !valid_class(registration->device_class) ||
        !valid_state(registration->state)) {
        return TABOS_DEVICE_ID_INVALID;
    }

    platform_mutex_lock(registry_mutex);
    if (name_used(registration->name)) {
        platform_mutex_unlock(registry_mutex);
        return TABOS_DEVICE_ID_INVALID;
    }

    size_t slot = DEVICE_REGISTRY_CAPACITY;
    for (size_t index = 0U; index < DEVICE_REGISTRY_CAPACITY; ++index) {
        if (!entries[index].used) {
            slot = index;
            break;
        }
    }
    if (slot == DEVICE_REGISTRY_CAPACITY) {
        platform_mutex_unlock(registry_mutex);
        return TABOS_DEVICE_ID_INVALID;
    }

    device_registry_entry_t* entry = &entries[slot];
    entry->used                    = true;
    entry->present                 = true;
    entry->info.id                 = make_id(slot);
    entry->info.device_class       = registration->device_class;
    entry->info.state              = registration->state;
    entry->info.features           = registration->features;
    entry->info.last_error         = registration->last_error;
    (void) memcpy(entry->info.name, registration->name, strlen(registration->name) + 1U);
    (void) memcpy(entry->info.driver, registration->driver, strlen(registration->driver) + 1U);
    ++active_count;
    publish_event(TABOS_DEVICE_EVENT_ADDED, &entry->info);
    const tabos_device_id_t id = entry->info.id;
    platform_mutex_unlock(registry_mutex);
    return id;
}

bool device_registry_remove(tabos_device_id_t id)
{
    if (registry_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    device_registry_entry_t* entry = active_entry(id);
    if (entry == NULL) {
        platform_mutex_unlock(registry_mutex);
        return false;
    }
    publish_event(TABOS_DEVICE_EVENT_REMOVED, &entry->info);
    entry->present = false;
    --active_count;
    platform_mutex_unlock(registry_mutex);
    return true;
}

bool device_registry_set_state(tabos_device_id_t id, tabos_device_state_t state, int32_t last_error)
{
    if (registry_mutex == NULL || !valid_state(state)) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    device_registry_entry_t* entry = active_entry(id);
    if (entry == NULL) {
        platform_mutex_unlock(registry_mutex);
        return false;
    }
    if (entry->info.state == state && entry->info.last_error == last_error) {
        platform_mutex_unlock(registry_mutex);
        return true;
    }
    entry->info.state      = state;
    entry->info.last_error = last_error;
    publish_event(state_event(state), &entry->info);
    platform_mutex_unlock(registry_mutex);
    return true;
}

size_t device_registry_count(void)
{
    if (registry_mutex == NULL) {
        return 0U;
    }
    platform_mutex_lock(registry_mutex);
    const size_t count = active_count;
    platform_mutex_unlock(registry_mutex);
    return count;
}

bool device_registry_at(size_t index, tabos_device_info_t* info)
{
    if (registry_mutex == NULL || info == NULL) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    size_t active_index = 0U;
    for (size_t slot = 0U; slot < DEVICE_REGISTRY_CAPACITY; ++slot) {
        if (!entries[slot].present) {
            continue;
        }
        if (active_index == index) {
            *info = entries[slot].info;
            platform_mutex_unlock(registry_mutex);
            return true;
        }
        ++active_index;
    }
    platform_mutex_unlock(registry_mutex);
    return false;
}

bool device_registry_get(tabos_device_id_t id, tabos_device_info_t* info)
{
    if (registry_mutex == NULL || info == NULL) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    const device_registry_entry_t* entry = active_entry(id);
    if (entry == NULL) {
        platform_mutex_unlock(registry_mutex);
        return false;
    }
    *info = entry->info;
    platform_mutex_unlock(registry_mutex);
    return true;
}

bool device_registry_find(const char* name, tabos_device_info_t* info)
{
    if (registry_mutex == NULL || name == NULL || info == NULL) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    for (size_t index = 0U; index < DEVICE_REGISTRY_CAPACITY; ++index) {
        if (entries[index].present && strcmp(entries[index].info.name, name) == 0) {
            *info = entries[index].info;
            platform_mutex_unlock(registry_mutex);
            return true;
        }
    }
    platform_mutex_unlock(registry_mutex);
    return false;
}

tabos_device_subscription_t device_registry_subscribe(const void* owner)
{
    if (registry_mutex == NULL || owner == NULL) {
        return TABOS_DEVICE_SUBSCRIPTION_INVALID;
    }
    platform_mutex_lock(registry_mutex);
    for (size_t index = 0U; index < DEVICE_SUBSCRIPTION_CAPACITY; ++index) {
        device_subscription_t* subscription = &subscriptions[index];
        if (subscription->used) {
            continue;
        }
        uint32_t generation = subscription->generation + 1U;
        if (generation == 0U || generation > DEVICE_SUBSCRIPTION_GENERATION_MAX) {
            generation = 1U;
        }
        *subscription = (device_subscription_t) {.owner = owner, .generation = generation, .used = true};
        const tabos_device_subscription_t handle = subscription_handle(index, generation);
        platform_mutex_unlock(registry_mutex);
        return handle;
    }
    platform_mutex_unlock(registry_mutex);
    return TABOS_DEVICE_SUBSCRIPTION_INVALID;
}

bool device_registry_unsubscribe(const void* owner, tabos_device_subscription_t handle)
{
    if (registry_mutex == NULL) {
        return false;
    }
    platform_mutex_lock(registry_mutex);
    device_subscription_t* subscription = owned_subscription(owner, handle);
    if (subscription == NULL) {
        platform_mutex_unlock(registry_mutex);
        return false;
    }
    const uint32_t generation = subscription->generation;
    *subscription             = (device_subscription_t) {.generation = generation};
    platform_mutex_unlock(registry_mutex);
    return true;
}

void device_registry_unsubscribe_owner(const void* owner)
{
    if (registry_mutex == NULL || owner == NULL) {
        return;
    }
    platform_mutex_lock(registry_mutex);
    for (size_t index = 0U; index < DEVICE_SUBSCRIPTION_CAPACITY; ++index) {
        device_subscription_t* subscription = &subscriptions[index];
        if (subscription->used && subscription->owner == owner) {
            const uint32_t generation = subscription->generation;
            *subscription             = (device_subscription_t) {.generation = generation};
        }
    }
    platform_mutex_unlock(registry_mutex);
}

int device_registry_read_event(const void* owner, tabos_device_subscription_t handle, tabos_device_event_t* event)
{
    if (registry_mutex == NULL || event == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(registry_mutex);
    device_subscription_t* subscription = owned_subscription(owner, handle);
    if (subscription == NULL) {
        platform_mutex_unlock(registry_mutex);
        return -TABOS_EBADF;
    }
    if (subscription->count == 0U) {
        platform_mutex_unlock(registry_mutex);
        return -TABOS_EAGAIN;
    }
    *event             = subscription->events[subscription->head];
    subscription->head = (subscription->head + 1U) % DEVICE_EVENT_QUEUE_CAPACITY;
    --subscription->count;
    if (subscription->overflow) {
        event->flags           |= TABOS_DEVICE_EVENT_OVERFLOW;
        subscription->overflow  = false;
    }
    platform_mutex_unlock(registry_mutex);
    return 0;
}
