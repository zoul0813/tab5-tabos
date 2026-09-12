#include <tabos/internal/ipc.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <string.h>

static int request(uint32_t owner, uint32_t session, uint32_t operation, int channel)
{
    ipc_transport_packet_t packet = {.channel = channel};
    return ipc_service_request(owner, session, operation, &packet);
}

int main(void)
{
    assert(ipc_service_init());
    assert(request(2U, 0U, IPC_TRANSPORT_CONNECT, 0) == -TABOS_EPERM);
    assert(request(2U, 1U, IPC_TRANSPORT_LISTEN, 0) == -TABOS_EPERM);
    const int listener = request(1U, 1U, IPC_TRANSPORT_LISTEN, 0);
    assert(listener > 0);
    assert(request(1U, 1U, IPC_TRANSPORT_LISTEN, 0) == -TABOS_EBUSY);
    assert(request(3U, 3U, IPC_TRANSPORT_CONNECT, 0) == -TABOS_ENOENT);
    const int client = request(2U, 1U, IPC_TRANSPORT_CONNECT, 0);
    assert(client > 0);
    uint32_t ready = 0U;
    assert(ipc_service_poll(1U, listener, TABOS_WAIT_READABLE, &ready) == 0 && ready == TABOS_WAIT_READABLE);
    const int server = request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener);
    assert(server > 0 && server != client);
    assert(request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener) == -TABOS_EAGAIN);
    uint32_t peer = 0U;
    assert(ipc_service_peer(1U, server, &peer) == 0 && peer == 2U);
    assert(ipc_service_poll(3U, server, TABOS_WAIT_READABLE, &ready) == -TABOS_EBADF);
    assert(request(3U, 1U, IPC_TRANSPORT_CLOSE, client) == -TABOS_EBADF);

    ipc_transport_packet_t packet = {
        .channel = client, .message = {.size = 3U, .sender_pid = 999U}
    };
    memcpy(packet.message.data, "one", 3U);
    for (unsigned int index = 0U; index < 8U; ++index) {
        packet.message.token = index;
        assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_SEND, &packet) == 0);
    }
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_SEND, &packet) == -TABOS_EAGAIN);
    assert(ipc_service_poll(2U, client, TABOS_WAIT_WRITABLE, &ready) == 0 && ready == 0U);
    packet.message.kind = 42U;
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_CONTROL, &packet) == 0);
    packet.message.kind = 43U;
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_CONTROL, &packet) == 0);
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_CONTROL, &packet) == -TABOS_EAGAIN);
    packet.channel = server;
    for (unsigned int index = 0U; index < 2U; ++index) {
        assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == 0);
        assert(packet.message.kind == 42U + index && packet.message.sender_pid == 2U);
    }
    for (unsigned int index = 0U; index < 8U; ++index) {
        assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == 0);
        assert(packet.message.token == index && packet.message.sender_pid == 2U);
        assert(memcmp(packet.message.data, "one", 3U) == 0);
    }
    assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == -TABOS_EAGAIN);
    packet.message.size = TABOS_IPC_DATA_MAX + 1U;
    assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_SEND, &packet) == -TABOS_EINVAL);
    ipc_service_close_owner(2U);
    assert(ipc_service_poll(1U, server, TABOS_WAIT_HANGUP, &ready) == 0 && ready == TABOS_WAIT_HANGUP);
    assert(request(1U, 1U, IPC_TRANSPORT_RECEIVE, server) == -TABOS_EPIPE);
    assert(request(1U, 1U, IPC_TRANSPORT_SEND, server) == -TABOS_EPIPE);
    assert(request(2U, 1U, IPC_TRANSPORT_CLOSE, client) == -TABOS_EBADF);
    assert(request(1U, 1U, IPC_TRANSPORT_CLOSE, server) == 0);

    /* Accept backlog is bounded; listener exit disconnects unaccepted peers. */
    int clients[8];
    for (unsigned int index = 0U; index < 8U; ++index) {
        clients[index] = request(2U, 1U, IPC_TRANSPORT_CONNECT, 0);
        assert(clients[index] > 0 && clients[index] != client);
    }
    assert(request(2U, 1U, IPC_TRANSPORT_CONNECT, 0) == -TABOS_EAGAIN);
    ipc_service_close_owner(1U);
    for (unsigned int index = 0U; index < 8U; ++index) {
        assert(ipc_service_poll(2U, clients[index], TABOS_WAIT_HANGUP, &ready) == 0 && ready == TABOS_WAIT_HANGUP);
    }
    ipc_service_close_owner(2U);
    for (unsigned int round = 0U; round < 100U; ++round) {
        const int fresh = request(1U, 1U, IPC_TRANSPORT_LISTEN, 0);
        assert(fresh > 0 && fresh != listener);
        assert(request(2U, 1U, IPC_TRANSPORT_CONNECT, 0) > 0);
        ipc_service_close_owner(1U);
        ipc_service_close_owner(2U);
    }
    ipc_service_shutdown();
    return 0;
}
