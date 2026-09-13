#include <desktop/input.h>
#include <tabos/filesystem.h>
#include <errno.h>

static bool motion(uint32_t kind, const tabos_gui_packet_t* packet)
{
    return kind == TABOS_GUI_POINTER &&
           (packet->data.pointer.type == TABOS_POINTER_MOVE || packet->data.pointer.type == TABOS_POINTER_HOVER);
}

bool desktop_input_push(desktop_input_queue_t* queue, uint32_t kind, const tabos_gui_packet_t* packet)
{
    if (motion(kind, packet)) {
        if (queue->count != 0U) {
            desktop_input_t* last = &queue->entries[(queue->head + queue->count - 1U) % DESKTOP_INPUT_DEPTH];
            const tabos_pointer_event_t* previous = &last->packet.data.pointer;
            const tabos_pointer_event_t* next     = &packet->data.pointer;
            if (last->kind == kind && last->packet.serial == packet->serial && previous->type == next->type &&
                previous->device_id == next->device_id && previous->contact_id == next->contact_id &&
                previous->buttons == next->buttons) {
                last->packet = *packet;
                return true;
            }
        }
        /* Leave room for a press/release even when several contacts move. */
        if (queue->count >= DESKTOP_INPUT_DEPTH - 4U) {
            return true;
        }
    }
    if (queue->count == DESKTOP_INPUT_DEPTH) {
        return false;
    }
    queue->entries[(queue->head + queue->count) % DESKTOP_INPUT_DEPTH] =
        (desktop_input_t) {.kind = kind, .packet = *packet};
    ++queue->count;
    return true;
}

int desktop_input_flush(desktop_input_queue_t* queue, tabos_ipc_channel_t channel)
{
    while (queue->count != 0U) {
        const desktop_input_t* next = &queue->entries[queue->head];
        if (tabos_gui_send(channel, next->kind, &next->packet, false) != 0) {
            return errno == TABOS_EAGAIN ? 0 : -1;
        }
        queue->head = (queue->head + 1U) % DESKTOP_INPUT_DEPTH;
        --queue->count;
    }
    return 0;
}
