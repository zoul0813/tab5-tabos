#include <tabos/internal/network.h>
#include <tabos/internal/network_config.h>
#include <tabos/internal/time.h>
#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>

enum {
    HOLD_NONE,
    HOLD_CONNECT,
    HOLD_DISCONNECT,
    HOLD_STATUS,
    HOLD_RESOLVE,
    HOLD_ECHO
};
static atomic_int held_operation;
static atomic_int active_controls;
static atomic_uint notifications;
static atomic_uint_fast64_t now_ms;
static atomic_bool shutdown_done;
static SDL_Semaphore* entered;
static SDL_Semaphore* released;
static platform_network_event_fn event_callback;
static platform_network_state_t backend_state;

static void hold(int operation)
{
    if (atomic_load(&held_operation) == operation) {
        SDL_SignalSemaphore(entered);
        SDL_WaitSemaphore(released);
    }
}

static void control_enter(int operation)
{
    assert(atomic_fetch_add(&active_controls, 1) == 0);
    hold(operation);
}

static void control_leave(void)
{
    assert(atomic_fetch_sub(&active_controls, 1) == 1);
}

network_config_result_t network_config_load(network_config_t* config)
{
    (void) config;
    return NETWORK_CONFIG_NOT_FOUND;
}

const char* network_config_result_name(network_config_result_t result)
{
    (void) result;
    return "unused";
}

uint64_t platform_time_ms(void)
{
    return atomic_load(&now_ms);
}

void platform_runtime_notify(platform_runtime_events_t events)
{
    atomic_fetch_or(&notifications, events);
}

bool platform_network_init(const char* hostname, platform_network_event_fn event)
{
    assert(strcmp(hostname, "TabOS") == 0);
    event_callback = event;
    backend_state  = PLATFORM_NETWORK_OFFLINE;
    return true;
}

void platform_network_shutdown(void)
{
    assert(atomic_load(&active_controls) == 0);
}

bool platform_network_operations_init(void)
{
    return true;
}
bool platform_network_socket_operations_init(void)
{
    return true;
}
bool platform_tls_operations_init(void)
{
    return true;
}
void platform_network_operations_shutdown(void)
{
}
void platform_network_socket_operations_shutdown(void)
{
}
void platform_tls_operations_shutdown(void)
{
}
int platform_network_power_suspend(void)
{
    return 0;
}
int platform_network_power_resume(void)
{
    return 0;
}

bool platform_network_connect(const char* ssid, const char* password)
{
    control_enter(HOLD_CONNECT);
    assert(strcmp(ssid, "test-network") == 0);
    assert(strcmp(password, "test-password") == 0);
    backend_state = PLATFORM_NETWORK_CONNECTING;
    event_callback();
    control_leave();
    return true;
}

bool platform_network_disconnect(void)
{
    control_enter(HOLD_DISCONNECT);
    backend_state = PLATFORM_NETWORK_OFFLINE;
    event_callback();
    control_leave();
    return true;
}

bool platform_network_status(platform_network_status_t* status)
{
    control_enter(HOLD_STATUS);
    *status = (platform_network_status_t) {.state = backend_state, .signal_dbm = -42};
    if (backend_state == PLATFORM_NETWORK_ONLINE) {
        strcpy(status->ipv4, "192.0.2.10");
    }
    control_leave();
    return true;
}

platform_network_operation_result_t platform_network_resolve(const char* hostname, uint32_t family,
                                                             platform_network_address_t* address)
{
    hold(HOLD_RESOLVE);
    assert(strcmp(hostname, "test.invalid") == 0 && family == 4U);
    *address = (platform_network_address_t) {.family = 4U, .text = "192.0.2.10"};
    return PLATFORM_NETWORK_OPERATION_OK;
}

platform_network_operation_result_t platform_network_echo(const platform_network_address_t* address, uint16_t sequence,
                                                          uint16_t payload_bytes, uint32_t timeout_ms,
                                                          platform_network_echo_result_t* result)
{
    hold(HOLD_ECHO);
    assert(strcmp(address->text, "192.0.2.10") == 0 && timeout_ms == 1000U);
    *result = (platform_network_echo_result_t) {.sequence = sequence, .bytes = payload_bytes, .round_trip_ms = 1U};
    return PLATFORM_NETWORK_OPERATION_OK;
}

static int operation_worker(void* unused)
{
    (void) unused;
    switch (atomic_load(&held_operation)) {
        case HOLD_CONNECT: assert(network_service_connect("test-network", "test-password", false)); break;
        case HOLD_DISCONNECT: assert(network_service_disconnect()); break;
        case HOLD_STATUS: network_service_update(); break;
        case HOLD_RESOLVE: {
            network_address_t address;
            assert(network_service_resolve("test.invalid", 4U, &address) == NETWORK_OPERATION_OK);
            assert(strcmp(address.text, "192.0.2.10") == 0);
            break;
        }
        case HOLD_ECHO: {
            const network_address_t address = {.family = 4U, .text = "192.0.2.10"};
            network_echo_result_t result;
            assert(network_service_echo(&address, 7U, 32U, 1000U, &result) == NETWORK_OPERATION_OK);
            assert(result.sequence == 7U && result.bytes == 32U);
            break;
        }
        default: assert(false); break;
    }
    return 0;
}

static int shutdown_worker(void* unused)
{
    (void) unused;
    network_service_shutdown();
    atomic_store(&shutdown_done, true);
    return 0;
}

static int update_worker(void* unused)
{
    (void) unused;
    network_service_update();
    return 0;
}

static int reader_worker(void* unused)
{
    (void) unused;
    for (unsigned int index = 0U; index < 10000U; ++index) {
        network_status_t status;
        assert(network_service_status(&status));
        assert(strcmp(status.ssid, "test-network") == 0);
        assert(status.attempts <= 3U);
        if (status.state == NETWORK_STATE_ONLINE) {
            assert(strcmp(status.ipv4, "192.0.2.10") == 0 && status.signal_dbm == -42);
        }
        (void) network_service_next_deadline();
    }
    return 0;
}

static void make_online(void)
{
    assert(network_service_connect("test-network", "test-password", false));
    backend_state = PLATFORM_NETWORK_ONLINE;
    event_callback();
    network_service_update();
}

int main(void)
{
    entered  = SDL_CreateSemaphore(0U);
    released = SDL_CreateSemaphore(0U);
    assert(entered != NULL && released != NULL);
    network_status_t status;
    assert(!network_service_status(&status));
    assert(network_service_next_deadline() == TIME_DEADLINE_NONE);
    assert(network_service_init());

    for (unsigned int cycle = 0U; cycle < 20U; ++cycle) {
        for (int operation = HOLD_CONNECT; operation <= HOLD_ECHO; ++operation) {
            make_online();
            event_callback();
            atomic_store(&held_operation, operation);
            SDL_Thread* worker = SDL_CreateThread(operation_worker, "network operation", NULL);
            assert(worker != NULL);
            assert(SDL_WaitSemaphoreTimeout(entered, 2000));
            /* These must complete before the blocked backend is released. */
            for (unsigned int index = 0U; index < 100U; ++index) {
                assert(network_service_status(&status));
                assert(strcmp(status.ssid, "test-network") == 0);
                assert(strcmp(status.hostname, "TabOS") == 0);
                assert(network_service_next_deadline() == TIME_DEADLINE_NONE);
                if (operation <= HOLD_STATUS) {
                    network_service_update();
                    assert(!network_service_connect("other-network", "other-password", false));
                    assert(!network_service_disconnect());
                }
            }
            assert(network_service_power_suspend() == -TABOS_EBUSY);
            if (operation >= HOLD_RESOLVE) {
                /* A blocked data operation must not monopolize control. */
                assert(network_service_disconnect());
                network_service_update();
            }
            atomic_store(&notifications, 0U);
            SDL_SignalSemaphore(released);
            SDL_WaitThread(worker, NULL);
            atomic_store(&held_operation, HOLD_NONE);
            if (operation <= HOLD_STATUS) {
                assert((atomic_load(&notifications) & PLATFORM_RUNTIME_EVENT_DEADLINE) != 0U);
            }
            network_service_update();
        }
    }

    SDL_Thread* reader = SDL_CreateThread(reader_worker, "network reader", NULL);
    assert(reader != NULL);
    for (unsigned int index = 0U; index < 1000U; ++index) {
        make_online();
        assert(network_service_disconnect());
    }
    SDL_WaitThread(reader, NULL);

    /* Retry claims the same control slot as an explicit connect. */
    assert(network_service_connect("test-network", "test-password", false));
    backend_state = PLATFORM_NETWORK_FAILED;
    event_callback();
    network_service_update();
    assert(network_service_next_deadline() == 1000U);
    atomic_store(&now_ms, 999U);
    network_service_update();
    assert(network_service_status(&status) && status.attempts == 1U);
    atomic_store(&held_operation, HOLD_CONNECT);
    atomic_store(&now_ms, 1000U);
    SDL_Thread* retry = SDL_CreateThread(update_worker, "network retry", NULL);
    assert(retry != NULL && SDL_WaitSemaphoreTimeout(entered, 2000));
    assert(network_service_status(&status) && status.attempts == 2U);
    assert(network_service_next_deadline() == TIME_DEADLINE_NONE);
    assert(!network_service_disconnect());
    SDL_SignalSemaphore(released);
    SDL_WaitThread(retry, NULL);
    atomic_store(&held_operation, HOLD_NONE);

    /* Late backend notifications cannot revive a manually disconnected link. */
    assert(network_service_disconnect());
    backend_state = PLATFORM_NETWORK_ONLINE;
    event_callback();
    network_service_update();
    assert(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE);
    atomic_store(&notifications, 0U);
    network_service_update();
    assert(atomic_load(&notifications) == 0U);

    make_online();
    atomic_store(&held_operation, HOLD_RESOLVE);
    SDL_Thread* resolver = SDL_CreateThread(operation_worker, "held resolver", NULL);
    assert(resolver != NULL && SDL_WaitSemaphoreTimeout(entered, 2000));
    SDL_Thread* shutdown = SDL_CreateThread(shutdown_worker, "network shutdown", NULL);
    assert(shutdown != NULL);
    const uint64_t deadline = SDL_GetTicks() + 2000U;
    while (network_service_status(&status)) {
        assert(SDL_GetTicks() < deadline);
        SDL_Delay(1U);
    }
    assert(!atomic_load(&shutdown_done));
    SDL_SignalSemaphore(released);
    SDL_WaitThread(resolver, NULL);
    SDL_WaitThread(shutdown, NULL);
    assert(atomic_load(&shutdown_done));
    atomic_store(&held_operation, HOLD_NONE);
    assert(!network_service_status(&status));
    assert(network_service_init());
    network_service_shutdown();
    SDL_DestroySemaphore(entered);
    SDL_DestroySemaphore(released);
    return 0;
}
