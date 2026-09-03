#include <tabos/device.h>
#include <tabos/pointer.h>
#include <tabos/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char* event_name(tabos_pointer_event_type_t type)
{
    switch (type) {
        case TABOS_POINTER_DOWN: return "down";
        case TABOS_POINTER_MOVE: return "move";
        case TABOS_POINTER_UP: return "up";
        case TABOS_POINTER_CANCEL: return "cancel";
        case TABOS_POINTER_EVENT_TYPE_COUNT: break;
    }
    return "unknown";
}

int main(int argc, char** argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: touchtest\nPrint touch and pointer events. Press Q to quit.");
        return 0;
    }
    if (argc != 1) {
        fputs("usage: touchtest\n", stderr);
        return 1;
    }
    tabos_device_info_t device;
    if (tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &device) != 0 || device.state != TABOS_DEVICE_READY) {
        fputs("touchtest: touch0 is unavailable\n", stderr);
        return 1;
    }
    const tabos_pointer_stream_t stream = tabos_pointer_open(device.id);
    const tabos_wait_source_t source    = tabos_pointer_wait_source(stream);
    if (stream == TABOS_POINTER_STREAM_INVALID || source == TABOS_WAIT_SOURCE_INVALID) {
        fprintf(stderr, "touchtest: open failed: %d\n", errno);
        return 1;
    }
    const int input_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (input_flags >= 0) {
        (void) fcntl(STDIN_FILENO, F_SETFL, input_flags | O_NONBLOCK);
    }
    puts("touchtest: touch or move pointer with primary button held; press Q to quit");
    bool running = true;
    while (running) {
        tabos_wait_item_t item = {.source = source, .events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP};
        const int ready        = tabos_wait(&item, 1U, 50U);
        if (ready < 0) {
            fprintf(stderr, "touchtest: wait failed: %d\n", errno);
            break;
        }
        if ((item.returned_events & TABOS_WAIT_HANGUP) != 0U) {
            fputs("touchtest: touch0 disconnected\n", stderr);
            break;
        }
        tabos_pointer_event_t event;
        while (tabos_pointer_read(stream, &event) == 0) {
            printf("%-6s contact=%u x=%ld y=%ld buttons=0x%lx", event_name(event.type), (unsigned int) event.contact_id,
                   (long) event.x, (long) event.y, (unsigned long) event.buttons);
            if ((event.flags & TABOS_POINTER_EVENT_HAS_PRESSURE) != 0U) {
                printf(" pressure=%lu/%u", (unsigned long) event.pressure, TABOS_POINTER_PRESSURE_MAX);
            }
            putchar('\n');
        }
        if (errno != EAGAIN) {
            fprintf(stderr, "touchtest: read failed: %d\n", errno);
            break;
        }
        char input;
        while (read(STDIN_FILENO, &input, 1U) == 1) {
            if (input == 'q' || input == 'Q' || input == 27) {
                running = false;
            }
        }
    }
    if (input_flags >= 0) {
        (void) fcntl(STDIN_FILENO, F_SETFL, input_flags);
    }
    return tabos_pointer_close(stream) == 0 ? 0 : 1;
}
