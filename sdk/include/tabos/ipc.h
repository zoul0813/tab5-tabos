#ifndef TABOS_IPC_H
#define TABOS_IPC_H

#include <stdbool.h>
#include <stdint.h>
#include <tabos/wait.h>

#define TABOS_IPC_DATA_MAX 224U
typedef int32_t tabos_ipc_channel_t;

typedef struct {
        uint32_t sender_pid; /* Filled by the OS; sender cannot impersonate a peer. */
        uint32_t kind;
        uint32_t token;
        uint32_t size;
        uint8_t data[TABOS_IPC_DATA_MAX];
} tabos_ipc_message_t;

/* Session owner listens; descendants connect within the inherited session. */
int tabos_session_open(void);
tabos_ipc_channel_t tabos_ipc_listen(void);
tabos_ipc_channel_t tabos_ipc_connect(void);
tabos_ipc_channel_t tabos_ipc_accept(tabos_ipc_channel_t listener);
int tabos_ipc_close(tabos_ipc_channel_t channel);
int tabos_ipc_send(tabos_ipc_channel_t channel, const tabos_ipc_message_t* message, bool control);
int tabos_ipc_receive(tabos_ipc_channel_t channel, tabos_ipc_message_t* message);
tabos_wait_source_t tabos_ipc_wait_source(tabos_ipc_channel_t channel);

#endif
