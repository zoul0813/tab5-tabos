#ifndef TABOS_IRC_PROTOCOL_H
#define TABOS_IRC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

enum {
    IRC_PROTOCOL_LINE_MAX = 510
};

typedef struct {
        char bytes[IRC_PROTOCOL_LINE_MAX + 2];
        size_t used;
        bool discarding;
} irc_protocol_buffer_t;

typedef void (*irc_protocol_line_fn)(char* line, void* context);
typedef void (*irc_protocol_send_fn)(const char* line, void* context);

typedef struct {
        irc_protocol_buffer_t receive;
        const char* initial_channel;
        char* channel;
        size_t channel_size;
        bool registered;
        irc_protocol_line_fn display;
        irc_protocol_send_fn send;
        void* callback_context;
} irc_protocol_session_t;

void irc_protocol_buffer_init(irc_protocol_buffer_t* buffer);
bool irc_protocol_buffer_feed(irc_protocol_buffer_t* buffer, const char* bytes, size_t count,
                              irc_protocol_line_fn handle_line, void* context);
void irc_protocol_session_init(irc_protocol_session_t* session, const char* initial_channel, char* channel,
                               size_t channel_size, irc_protocol_line_fn display, irc_protocol_send_fn send,
                               void* callback_context);
bool irc_protocol_session_feed(irc_protocol_session_t* session, const char* bytes, size_t count);
bool irc_protocol_session_registered(const irc_protocol_session_t* session);

#endif
