#ifndef TABOS_INTERNAL_IPC_H
#define TABOS_INTERNAL_IPC_H

#include <tabos/internal/ipc_transport.h>

bool ipc_service_init(void);
void ipc_service_shutdown(void);
void ipc_service_close_owner(uint32_t owner);
int ipc_service_request(uint32_t owner, uint32_t session, uint32_t operation, ipc_transport_packet_t* packet);
int ipc_service_poll(uint32_t owner, int channel, uint32_t requested, uint32_t* returned);
int ipc_service_peer(uint32_t owner, int channel, uint32_t* peer);

#endif
