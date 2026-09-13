#include <tabos/internal/ipc.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static unsigned int allocation_attempt;
static unsigned int fail_allocation;
static unsigned int live_allocations;

static void* failing_calloc(size_t count, size_t size)
{
    ++allocation_attempt;
    if (fail_allocation != 0U && allocation_attempt == fail_allocation) {
        return NULL;
    }
    void* result = calloc(count, size);
    if (result != NULL) {
        ++live_allocations;
    }
    return result;
}

static void tracked_free(void* allocation)
{
    if (allocation != NULL) {
        assert(live_allocations > 0U);
        --live_allocations;
    }
    free(allocation);
}

/* Inject only IPC queue allocations; exercise the production service unchanged. */
#define calloc failing_calloc
#define free   tracked_free
#include "../../kernel/ipc.c"
#undef free
#undef calloc

static int request(uint32_t owner, uint32_t session, uint32_t operation, int channel)
{
    ipc_transport_packet_t packet = {.channel = channel};
    return ipc_service_request(owner, session, operation, &packet);
}

static void allocation_failures(void)
{
    assert(ipc_service_init());
    const int listener = request(1U, 1U, IPC_TRANSPORT_LISTEN, 0);
    assert(listener > 0 && live_allocations == 0U);
    for (unsigned int round = 0U; round < 100U; ++round) {
        for (unsigned int failure = 1U; failure <= 2U; ++failure) {
            allocation_attempt = 0U;
            fail_allocation    = failure;
            assert(request(2U, 1U, IPC_TRANSPORT_CONNECT, 0) == -TABOS_ENOMEM);
            assert(allocation_attempt == failure && live_allocations == 0U);
            uint32_t ready = UINT32_MAX;
            assert(ipc_service_poll(1U, listener, TABOS_WAIT_READABLE, &ready) == 0);
            assert(ready == 0U);
            assert(request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener) == -TABOS_EAGAIN);
            fail_allocation  = 0U;
            const int client = request(2U, 1U, IPC_TRANSPORT_CONNECT, 0);
            const int server = request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener);
            assert(client > 0 && server > 0 && live_allocations == 2U);
            assert(request(2U, 1U, IPC_TRANSPORT_CLOSE, client) == 0);
            assert(request(1U, 1U, IPC_TRANSPORT_CLOSE, server) == 0);
            assert(live_allocations == 0U);
        }
    }

    /* One listener and fifteen pairs leave only one slot: rollback must free it. */
    for (unsigned int index = 0U; index < 15U; ++index) {
        assert(request(2U, 1U, IPC_TRANSPORT_CONNECT, 0) > 0);
        assert(request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener) > 0);
    }
    assert(live_allocations == 30U);
    for (unsigned int index = 0U; index < 100U; ++index) {
        assert(request(2U, 1U, IPC_TRANSPORT_CONNECT, 0) == -TABOS_ENOMEM);
        assert(live_allocations == 30U);
    }
    const int last_slot = request(3U, 3U, IPC_TRANSPORT_LISTEN, 0);
    assert(last_slot > 0);
    assert(request(4U, 4U, IPC_TRANSPORT_LISTEN, 0) == -TABOS_ENOMEM);
    ipc_service_close_owner(2U);
    assert(live_allocations == 15U);
    ipc_service_close_owner(1U);
    assert(live_allocations == 0U);
    ipc_service_shutdown();
    uint32_t ready = 0U;
    assert(ipc_service_init());
    const int fresh = request(3U, 3U, IPC_TRANSPORT_LISTEN, 0);
    assert(fresh > 0 && fresh != last_slot);
    assert(ipc_service_poll(3U, last_slot, TABOS_WAIT_READABLE, &ready) == -TABOS_EBADF);
    ipc_service_shutdown();
}

static void disconnected_queue(void)
{
    assert(ipc_service_init());
    const int listener = request(1U, 1U, IPC_TRANSPORT_LISTEN, 0);
    const int client   = request(2U, 1U, IPC_TRANSPORT_CONNECT, 0);
    const int server   = request(1U, 1U, IPC_TRANSPORT_ACCEPT, listener);
    assert(listener > 0 && client > 0 && server > 0);
    ipc_transport_packet_t packet = {.channel = client, .message = {.token = 123U}};
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_SEND, &packet) == 0);
    packet.message.token = 456U;
    assert(ipc_service_request(2U, 1U, IPC_TRANSPORT_CONTROL, &packet) == 0);
    ipc_service_close_owner(2U);
    const uint32_t events = TABOS_WAIT_READABLE | TABOS_WAIT_WRITABLE | TABOS_WAIT_HANGUP;
    uint32_t ready        = 0U;
    assert(ipc_service_poll(1U, server, events, &ready) == 0);
    assert(ready == (TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP));
    packet.channel = server;
    assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == 0);
    assert(packet.message.token == 456U);
    assert(ipc_service_poll(1U, server, events, &ready) == 0);
    assert(ready == (TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP));
    assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == 0);
    assert(packet.message.token == 123U);
    assert(ipc_service_poll(1U, server, events, &ready) == 0 && ready == TABOS_WAIT_HANGUP);
    assert(request(1U, 1U, IPC_TRANSPORT_RECEIVE, server) == -TABOS_EPIPE);
    ipc_service_shutdown();
    assert(live_allocations == 0U);
}

int main(void)
{
    allocation_failures();
    disconnected_queue();
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
        assert(ipc_service_poll(2U, client, TABOS_WAIT_WRITABLE, &ready) == 0 && ready == 0U);
    }
    for (unsigned int index = 0U; index < 8U; ++index) {
        assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_RECEIVE, &packet) == 0);
        assert(packet.message.token == index && packet.message.sender_pid == 2U);
        assert(memcmp(packet.message.data, "one", 3U) == 0);
        assert(ipc_service_poll(2U, client, TABOS_WAIT_WRITABLE, &ready) == 0);
        assert(ready == TABOS_WAIT_WRITABLE);
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
    assert(live_allocations == 0U);
    return 0;
}
