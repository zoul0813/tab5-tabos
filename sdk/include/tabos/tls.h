#ifndef TABOS_TLS_H
#define TABOS_TLS_H

#include <stdint.h>

enum {
    TABOS_TLS_MAX = 4,
};

typedef int32_t tabos_tls_t;

tabos_tls_t tabos_tls_connect(const char* hostname, uint16_t port);
int tabos_tls_close(tabos_tls_t connection);
int tabos_tls_send(tabos_tls_t connection, const void* data, uint32_t size);
int tabos_tls_receive(tabos_tls_t connection, void* data, uint32_t capacity);

#endif
