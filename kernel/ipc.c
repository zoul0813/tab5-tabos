#include <tabos/internal/ipc.h>
#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum {
    IPC_CHANNELS      = 32,
    IPC_DATA_DEPTH    = 8,
    IPC_CONTROL_DEPTH = 2,
    IPC_ACCEPT_DEPTH  = 8
};

typedef struct {
        tabos_ipc_message_t data[IPC_DATA_DEPTH];
        tabos_ipc_message_t control[IPC_CONTROL_DEPTH];
        unsigned int data_head, data_count, control_head, control_count;
} ipc_queue_t;

typedef struct {
        int handle;
        uint32_t owner;
        uint32_t session;
        bool listener;
        int peer;
        int pending[IPC_ACCEPT_DEPTH];
        unsigned int pending_count;
        ipc_queue_t* queue;
} ipc_endpoint_t;

static ipc_endpoint_t endpoints[IPC_CHANNELS];
static platform_mutex_t* mutex;
static uint32_t generation = 1U;

static ipc_endpoint_t* endpoint(int handle)
{
    if (handle <= 0) {
        return NULL;
    }
    ipc_endpoint_t* candidate = &endpoints[(unsigned int) handle % IPC_CHANNELS];
    return candidate->handle == handle ? candidate : NULL;
}

static ipc_endpoint_t* owned(uint32_t owner, int handle)
{
    ipc_endpoint_t* result = endpoint(handle);
    return result != NULL && result->owner == owner ? result : NULL;
}

static ipc_endpoint_t* allocate(uint32_t owner, uint32_t session, bool listener)
{
    if (generation >= (uint32_t) INT_MAX / IPC_CHANNELS) {
        return NULL;
    }
    for (unsigned int index = 0U; index < IPC_CHANNELS; ++index) {
        if (endpoints[index].handle == 0) {
            ipc_queue_t* queue = listener ? NULL : calloc(1U, sizeof(*queue));
            if (!listener && queue == NULL) {
                return NULL;
            }
            endpoints[index] = (ipc_endpoint_t) {
                .handle   = (int) (generation++ * IPC_CHANNELS + index),
                .owner    = owner,
                .session  = session,
                .listener = listener,
                .queue    = queue,
            };
            return &endpoints[index];
        }
    }
    return NULL;
}

static void close_endpoint(ipc_endpoint_t* entry)
{
    if (entry->listener) {
        for (unsigned int index = 0U; index < entry->pending_count; ++index) {
            ipc_endpoint_t* child = endpoint(entry->pending[index]);
            if (child != NULL) {
                close_endpoint(child);
            }
        }
    }
    ipc_endpoint_t* peer = endpoint(entry->peer);
    if (peer != NULL) {
        peer->peer = 0;
    }
    free(entry->queue);
    *entry = (ipc_endpoint_t) {0};
}

bool ipc_service_init(void)
{
    if (mutex == NULL) {
        mutex = platform_mutex_create();
    }
    return mutex != NULL;
}

void ipc_service_shutdown(void)
{
    if (mutex == NULL) {
        return;
    }
    for (unsigned int index = 0U; index < IPC_CHANNELS; ++index) {
        if (endpoints[index].handle != 0) {
            close_endpoint(&endpoints[index]);
        }
    }
    platform_mutex_destroy(mutex);
    mutex = NULL;
}

void ipc_service_close_owner(uint32_t owner)
{
    if (mutex == NULL) {
        return;
    }
    platform_mutex_lock(mutex);
    for (unsigned int index = 0U; index < IPC_CHANNELS; ++index) {
        if (endpoints[index].handle != 0 && endpoints[index].owner == owner) {
            close_endpoint(&endpoints[index]);
        }
    }
    platform_mutex_unlock(mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
}

static int request_locked(uint32_t owner, uint32_t session, uint32_t operation, ipc_transport_packet_t* packet)
{
    if (operation == IPC_TRANSPORT_LISTEN || operation == IPC_TRANSPORT_CONNECT) {
        if (session == 0U) {
            return -TABOS_EPERM;
        }
        ipc_endpoint_t* listener = NULL;
        for (unsigned int index = 0U; index < IPC_CHANNELS; ++index) {
            if (endpoints[index].handle != 0 && endpoints[index].listener && endpoints[index].session == session) {
                listener = &endpoints[index];
                break;
            }
        }
        if (operation == IPC_TRANSPORT_LISTEN) {
            if (owner != session) {
                return -TABOS_EPERM;
            }
            if (listener != NULL) {
                return -TABOS_EBUSY;
            }
            listener = allocate(owner, session, true);
            return listener == NULL ? -TABOS_ENOMEM : listener->handle;
        }
        if (listener == NULL) {
            return -TABOS_ENOENT;
        }
        if (listener->pending_count == IPC_ACCEPT_DEPTH) {
            return -TABOS_EAGAIN;
        }
        ipc_endpoint_t* client = allocate(owner, session, false);
        ipc_endpoint_t* server = client != NULL ? allocate(listener->owner, session, false) : NULL;
        if (server == NULL) {
            if (client != NULL) {
                close_endpoint(client);
            }
            return -TABOS_ENOMEM;
        }
        client->peer                                 = server->handle;
        server->peer                                 = client->handle;
        listener->pending[listener->pending_count++] = server->handle;
        return client->handle;
    }
    ipc_endpoint_t* entry = owned(owner, packet->channel);
    if (entry == NULL) {
        return -TABOS_EBADF;
    }
    if (operation == IPC_TRANSPORT_CLOSE) {
        close_endpoint(entry);
        return 0;
    }
    if (operation == IPC_TRANSPORT_ACCEPT) {
        if (!entry->listener) {
            return -TABOS_EINVAL;
        }
        while (entry->pending_count > 0U) {
            const int handle = entry->pending[0];
            --entry->pending_count;
            memmove(entry->pending, entry->pending + 1, entry->pending_count * sizeof(entry->pending[0]));
            if (endpoint(handle) != NULL) {
                return handle;
            }
        }
        return -TABOS_EAGAIN;
    }
    if (entry->listener) {
        return -TABOS_EINVAL;
    }
    if (operation == IPC_TRANSPORT_SEND || operation == IPC_TRANSPORT_CONTROL) {
        if (packet->message.size > TABOS_IPC_DATA_MAX) {
            return -TABOS_EINVAL;
        }
        ipc_endpoint_t* peer = endpoint(entry->peer);
        if (peer == NULL) {
            return -TABOS_EPIPE;
        }
        ipc_queue_t* queue            = peer->queue;
        const bool control            = operation == IPC_TRANSPORT_CONTROL;
        unsigned int* count           = control ? &queue->control_count : &queue->data_count;
        const unsigned int head       = control ? queue->control_head : queue->data_head;
        const unsigned int capacity   = control ? IPC_CONTROL_DEPTH : IPC_DATA_DEPTH;
        tabos_ipc_message_t* messages = control ? queue->control : queue->data;
        if (*count == capacity) {
            return -TABOS_EAGAIN;
        }
        tabos_ipc_message_t* copied = &messages[(head + *count) % capacity];
        *copied                     = (tabos_ipc_message_t) {.sender_pid = owner,
                                                             .kind       = packet->message.kind,
                                                             .token      = packet->message.token,
                                                             .size       = packet->message.size};
        memcpy(copied->data, packet->message.data, packet->message.size);
        ++*count;
        return 0;
    }
    if (operation == IPC_TRANSPORT_RECEIVE) {
        ipc_queue_t* queue            = entry->queue;
        const bool control            = queue->control_count != 0U;
        unsigned int* count           = control ? &queue->control_count : &queue->data_count;
        unsigned int* head            = control ? &queue->control_head : &queue->data_head;
        const unsigned int capacity   = control ? IPC_CONTROL_DEPTH : IPC_DATA_DEPTH;
        tabos_ipc_message_t* messages = control ? queue->control : queue->data;
        if (*count == 0U) {
            return entry->peer == 0 ? -TABOS_EPIPE : -TABOS_EAGAIN;
        }
        packet->message = messages[*head];
        *head           = (*head + 1U) % capacity;
        --*count;
        return 0;
    }
    return -TABOS_EINVAL;
}

int ipc_service_request(uint32_t owner, uint32_t session, uint32_t operation, ipc_transport_packet_t* packet)
{
    if (mutex == NULL || packet == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(mutex);
    const int result = request_locked(owner, session, operation, packet);
    platform_mutex_unlock(mutex);
    if (result >= 0) {
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    }
    return result;
}

int ipc_service_peer(uint32_t owner, int channel, uint32_t* peer)
{
    if (mutex == NULL || peer == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(mutex);
    const ipc_endpoint_t* entry  = owned(owner, channel);
    const ipc_endpoint_t* remote = entry != NULL ? endpoint(entry->peer) : NULL;
    if (remote != NULL) {
        *peer = remote->owner;
    }
    platform_mutex_unlock(mutex);
    return remote != NULL ? 0 : -TABOS_EBADF;
}

int ipc_service_poll(uint32_t owner, int channel, uint32_t requested, uint32_t* returned)
{
    if (mutex == NULL || returned == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(mutex);
    const ipc_endpoint_t* entry = owned(owner, channel);
    uint32_t ready              = 0U;
    if (entry != NULL) {
        if (entry->listener) {
            ready = entry->pending_count != 0U ? TABOS_WAIT_READABLE : 0U;
        } else {
            if (entry->queue->data_count != 0U || entry->queue->control_count != 0U) {
                ready |= TABOS_WAIT_READABLE;
            }
            const ipc_endpoint_t* peer = endpoint(entry->peer);
            if (peer == NULL) {
                ready |= TABOS_WAIT_HANGUP;
            } else if (peer->queue->data_count < IPC_DATA_DEPTH) {
                ready |= TABOS_WAIT_WRITABLE;
            }
        }
    }
    *returned = ready & requested;
    platform_mutex_unlock(mutex);
    return entry != NULL ? 0 : -TABOS_EBADF;
}
