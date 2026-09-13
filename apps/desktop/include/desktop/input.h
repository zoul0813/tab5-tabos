#ifndef DESKTOP_INPUT_H
#define DESKTOP_INPUT_H

#include <tabos/gui.h>

enum {
    DESKTOP_INPUT_DEPTH = 64U
};
typedef struct {
        uint32_t kind;
        tabos_gui_packet_t packet;
} desktop_input_t;
typedef struct {
        desktop_input_t entries[DESKTOP_INPUT_DEPTH];
        unsigned int head;
        unsigned int count;
} desktop_input_queue_t;

/* Motion may be coalesced or dropped; discrete events retain FIFO order. */
bool desktop_input_push(desktop_input_queue_t* queue, uint32_t kind, const tabos_gui_packet_t* packet);
/* EAGAIN retains the unsent tail. Other transport failures return -1. */
int desktop_input_flush(desktop_input_queue_t* queue, tabos_ipc_channel_t channel);

#endif
