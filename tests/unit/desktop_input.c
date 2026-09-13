#include <desktop/input.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <errno.h>

static desktop_input_t delivered[128];
static unsigned int delivered_count;
static unsigned int available;
static int failure;

int tabos_gui_send(tabos_ipc_channel_t channel, uint32_t kind, const tabos_gui_packet_t* packet, bool control)
{
    assert(channel == 7 && !control);
    if (failure != 0 || available == 0U) {
        errno = failure != 0 ? failure : TABOS_EAGAIN;
        return -1;
    }
    --available;
    delivered[delivered_count++] = (desktop_input_t) {.kind = kind, .packet = *packet};
    return 0;
}

int main(void)
{
    desktop_input_queue_t queue = {0};
    tabos_gui_packet_t packet   = {
          .serial = 1U, .data.pointer = {.type = TABOS_POINTER_HOVER, .device_id = 1U}
    };
    for (uint32_t sequence = 1U; sequence <= 100U; ++sequence) {
        packet.input_sequence = sequence;
        packet.data.pointer.x = (int32_t) sequence;
        assert(desktop_input_push(&queue, TABOS_GUI_POINTER, &packet));
    }
    assert(queue.count == 1U);
    packet.data.pointer.type = TABOS_POINTER_DOWN;
    packet.input_sequence    = 101U;
    assert(desktop_input_push(&queue, TABOS_GUI_POINTER, &packet));
    packet.data.pointer.type = TABOS_POINTER_MOVE;
    for (uint32_t sequence = 102U; sequence <= 110U; ++sequence) {
        packet.input_sequence = sequence;
        assert(desktop_input_push(&queue, TABOS_GUI_POINTER, &packet));
    }
    packet.data.pointer.type = TABOS_POINTER_UP;
    packet.input_sequence    = 111U;
    assert(desktop_input_push(&queue, TABOS_GUI_POINTER, &packet));
    assert(queue.count == 4U);
    assert(desktop_input_flush(&queue, 7) == 0 && queue.count == 4U);
    available = 2U;
    assert(desktop_input_flush(&queue, 7) == 0 && queue.count == 2U);
    available = 8U;
    assert(desktop_input_flush(&queue, 7) == 0 && queue.count == 0U);
    assert(delivered_count == 4U);
    assert(delivered[0].packet.input_sequence == 100U && delivered[0].packet.data.pointer.x == 100);
    assert(delivered[1].packet.input_sequence == 101U);
    assert(delivered[2].packet.input_sequence == 110U);
    assert(delivered[3].packet.input_sequence == 111U);

    /* Retry discrete input across repeated eight-message IPC drains, with no duplication. */
    delivered_count = 0U;
    for (uint32_t sequence = 1U; sequence <= DESKTOP_INPUT_DEPTH; ++sequence) {
        packet = (tabos_gui_packet_t) {.input_sequence = sequence, .data.keyboard = {.type = TABOS_INPUT_TEXT}};
        assert(desktop_input_push(&queue, TABOS_GUI_KEYBOARD, &packet));
    }
    assert(!desktop_input_push(&queue, TABOS_GUI_KEYBOARD, &packet));
    packet.data.pointer.type = TABOS_POINTER_HOVER;
    assert(desktop_input_push(&queue, TABOS_GUI_POINTER, &packet));
    assert(queue.count == DESKTOP_INPUT_DEPTH);
    failure = TABOS_EPIPE;
    assert(desktop_input_flush(&queue, 7) == -1 && errno == TABOS_EPIPE && queue.count == DESKTOP_INPUT_DEPTH);
    failure = 0;
    while (queue.count != 0U) {
        available = 8U;
        assert(desktop_input_flush(&queue, 7) == 0);
    }
    assert(delivered_count == DESKTOP_INPUT_DEPTH);
    for (unsigned int index = 0U; index < delivered_count; ++index) {
        assert(delivered[index].kind == TABOS_GUI_KEYBOARD && delivered[index].packet.input_sequence == index + 1U);
    }
    return 0;
}
