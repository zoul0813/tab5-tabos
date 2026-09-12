#ifndef TABOS_INTERNAL_IPC_TRANSPORT_H
#define TABOS_INTERNAL_IPC_TRANSPORT_H

#include <tabos/ipc.h>

enum {
    IPC_TRANSPORT_LISTEN,
    IPC_TRANSPORT_CONNECT,
    IPC_TRANSPORT_ACCEPT,
    IPC_TRANSPORT_CLOSE,
    IPC_TRANSPORT_SEND,
    IPC_TRANSPORT_CONTROL,
    IPC_TRANSPORT_RECEIVE,
    IPC_TRANSPORT_WAIT_SOURCE,
};

typedef struct {
        int32_t channel;
        tabos_ipc_message_t message;
} ipc_transport_packet_t;

#endif
