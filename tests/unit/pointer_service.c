#include <tabos/internal/pointer.h>
#include <tabos/filesystem.h>
#include <tabos/wait.h>

#include <stdio.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "pointer service test failed: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    static const int owner_a;
    static const int owner_b;
    expect(pointer_service_init(), "initializes");
    pointer_service_set_device_id(42U);
    const tabos_pointer_stream_t stream_a = pointer_service_open(&owner_a, 42U);
    const tabos_pointer_stream_t stream_b = pointer_service_open(&owner_b, 42U);
    expect(stream_a != TABOS_POINTER_STREAM_INVALID && stream_b != TABOS_POINTER_STREAM_INVALID, "opens streams");

    pointer_service_set_foreground_owner(&owner_a);
    const tabos_pointer_event_t down = {
        .type       = TABOS_POINTER_DOWN,
        .contact_id = 2U,
        .x          = 100,
        .y          = 200,
        .buttons    = TABOS_POINTER_BUTTON_PRIMARY,
    };
    pointer_service_submit(&down);
    uint32_t events = 0U;
    expect(pointer_service_poll(&owner_a, stream_a, TABOS_WAIT_READABLE, &events) == 0 && events == TABOS_WAIT_READABLE,
           "foreground stream becomes readable");
    expect(pointer_service_poll(&owner_b, stream_b, TABOS_WAIT_READABLE, &events) == 0 && events == 0U,
           "background stream not readable");
    tabos_pointer_event_t received;
    expect(pointer_service_read(&owner_b, stream_b, &received) == -TABOS_EACCES,
           "background stream cannot consume events");
    expect(pointer_service_read(&owner_a, stream_a, &received) == 0 && received.type == TABOS_POINTER_DOWN &&
               received.device_id == 42U && received.contact_id == 2U && received.x == 100 && received.y == 200,
           "reads normalized event");

    tabos_pointer_event_t second_down = down;
    second_down.contact_id            = 4U;
    second_down.x                     = 300;
    pointer_service_submit(&second_down);
    expect(pointer_service_read(&owner_a, stream_a, &received) == 0 && received.type == TABOS_POINTER_DOWN &&
               received.contact_id == 4U && received.x == 300,
           "tracks a simultaneous second contact");
    pointer_service_set_foreground_owner(&owner_b);
    pointer_service_set_foreground_owner(&owner_a);
    uint32_t canceled_contacts = 0U;
    while (pointer_service_read(&owner_a, stream_a, &received) == 0) {
        if (received.type == TABOS_POINTER_CANCEL && (received.contact_id == 2U || received.contact_id == 4U)) {
            ++canceled_contacts;
        }
    }
    expect(canceled_contacts == 2U, "focus change cancels all active contacts");

    pointer_service_submit(&down);
    for (uint32_t index = 0U; index < 80U; ++index) {
        tabos_pointer_event_t move = down;
        move.type                  = TABOS_POINTER_MOVE;
        move.x                     = (int32_t) index;
        pointer_service_submit(&move);
    }
    bool saw_cancel = false;
    while (pointer_service_read(&owner_a, stream_a, &received) == 0) {
        saw_cancel = saw_cancel || received.type == TABOS_POINTER_CANCEL;
    }
    expect(saw_cancel, "overflow resets queue with cancellation");

    pointer_service_remove_device();
    expect(pointer_service_poll(&owner_a, stream_a, TABOS_WAIT_HANGUP, &events) == 0 && events == TABOS_WAIT_HANGUP,
           "removal reports hangup");
    expect(pointer_service_close(&owner_a, stream_a) == 0 && pointer_service_close(&owner_a, stream_a) < 0,
           "stale stream rejected");
    pointer_service_close_owner(&owner_b);
    expect(pointer_service_read(&owner_b, stream_b, &received) < 0, "owner cleanup closes stream");
    pointer_service_shutdown();
    return failures == 0 ? 0 : 1;
}
