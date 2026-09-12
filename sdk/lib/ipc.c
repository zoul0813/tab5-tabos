#include <tabos/ipc.h>
#include <tabos/internal/elf_api.h>
#include <tabos/filesystem.h>
#include <errno.h>
#include <stddef.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int call(uint32_t operation, ipc_transport_packet_t* packet)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->ipc == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int result = tabos_runtime_api->ipc(operation, packet);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

int tabos_session_open(void)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->session_open == NULL || tabos_runtime_api->yield == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result;
    do {
        result = tabos_runtime_api->session_open();
        if (result == TABOS_ELF_EXEC_PENDING) {
            tabos_runtime_api->yield();
        }
    } while (result == TABOS_ELF_EXEC_PENDING);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

tabos_ipc_channel_t tabos_ipc_listen(void)
{
    ipc_transport_packet_t packet = {0};
    return call(IPC_TRANSPORT_LISTEN, &packet);
}

tabos_ipc_channel_t tabos_ipc_connect(void)
{
    ipc_transport_packet_t packet = {0};
    return call(IPC_TRANSPORT_CONNECT, &packet);
}

tabos_ipc_channel_t tabos_ipc_accept(tabos_ipc_channel_t listener)
{
    ipc_transport_packet_t packet = {.channel = listener};
    return call(IPC_TRANSPORT_ACCEPT, &packet);
}

int tabos_ipc_close(tabos_ipc_channel_t channel)
{
    ipc_transport_packet_t packet = {.channel = channel};
    return call(IPC_TRANSPORT_CLOSE, &packet);
}

int tabos_ipc_send(tabos_ipc_channel_t channel, const tabos_ipc_message_t* message, bool control)
{
    if (message == NULL || message->size > TABOS_IPC_DATA_MAX) {
        errno = EINVAL;
        return -1;
    }
    ipc_transport_packet_t packet = {.channel = channel, .message = *message};
    return call(control ? IPC_TRANSPORT_CONTROL : IPC_TRANSPORT_SEND, &packet);
}

int tabos_ipc_receive(tabos_ipc_channel_t channel, tabos_ipc_message_t* message)
{
    if (message == NULL) {
        errno = EINVAL;
        return -1;
    }
    ipc_transport_packet_t packet = {.channel = channel};
    const int result              = call(IPC_TRANSPORT_RECEIVE, &packet);
    if (result == 0) {
        *message = packet.message;
    }
    return result;
}

tabos_wait_source_t tabos_ipc_wait_source(tabos_ipc_channel_t channel)
{
    ipc_transport_packet_t packet = {.channel = channel};
    return call(IPC_TRANSPORT_WAIT_SOURCE, &packet);
}
