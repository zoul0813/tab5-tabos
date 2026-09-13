#include "protocol.h"

#include <stdio.h>
#include <string.h>

void irc_protocol_buffer_init(irc_protocol_buffer_t* buffer)
{
    if (buffer == NULL) {
        return;
    }
    buffer->used       = 0U;
    buffer->discarding = false;
}

bool irc_protocol_buffer_feed(irc_protocol_buffer_t* buffer, const char* bytes, size_t count,
                              irc_protocol_line_fn handle_line, void* context)
{
    if (buffer == NULL || (bytes == NULL && count != 0U) || handle_line == NULL) {
        return false;
    }

    bool accepted = true;
    for (size_t index = 0U; index < count; ++index) {
        const char byte = bytes[index];
        if (buffer->discarding) {
            if (byte == '\n') {
                buffer->discarding = false;
                buffer->used       = 0U;
            }
            continue;
        }
        if (byte == '\n') {
            if (buffer->used > 0U && buffer->bytes[buffer->used - 1U] == '\r') {
                buffer->bytes[buffer->used - 1U] = '\0';
                handle_line(buffer->bytes, context);
            }
            buffer->used = 0U;
            continue;
        }
        if (buffer->used == sizeof(buffer->bytes) - 1U) {
            buffer->used       = 0U;
            buffer->discarding = true;
            accepted           = false;
            continue;
        }
        buffer->bytes[buffer->used++] = byte;
    }
    return accepted;
}

static bool make_pong(const char* line, char* output, size_t output_size)
{
    if (line == NULL || output == NULL || strncmp(line, "PING ", 5U) != 0) {
        return false;
    }
    const int length = snprintf(output, output_size, "PONG %s\r\n", line + 5);
    return length >= 0 && (size_t) length < output_size;
}

static bool is_welcome(const char* line)
{
    return line != NULL && strstr(line, " 001 ") != NULL;
}

static void handle_session_line(char* line, void* opaque_session)
{
    irc_protocol_session_t* session = opaque_session;
    char output[IRC_PROTOCOL_LINE_MAX + 8];

    if (make_pong(line, output, sizeof(output))) {
        session->send(output, session->callback_context);
    }
    if (!session->registered && is_welcome(line)) {
        session->registered = true;
        if (session->initial_channel != NULL) {
            (void) snprintf(session->channel, session->channel_size, "%s", session->initial_channel);
            (void) snprintf(output, sizeof(output), "JOIN %s\r\n", session->channel);
            session->send(output, session->callback_context);
        }
    }
    session->display(line, session->callback_context);
}

void irc_protocol_session_init(irc_protocol_session_t* session, const char* initial_channel, char* channel,
                               size_t channel_size, irc_protocol_line_fn display, irc_protocol_send_fn send,
                               void* callback_context)
{
    if (session == NULL) {
        return;
    }
    irc_protocol_buffer_init(&session->receive);
    session->initial_channel  = initial_channel;
    session->channel          = channel;
    session->channel_size     = channel_size;
    session->registered       = false;
    session->display          = display;
    session->send             = send;
    session->callback_context = callback_context;
}

bool irc_protocol_session_feed(irc_protocol_session_t* session, const char* bytes, size_t count)
{
    if (session == NULL || session->channel == NULL || session->channel_size == 0U || session->display == NULL ||
        session->send == NULL) {
        return false;
    }
    return irc_protocol_buffer_feed(&session->receive, bytes, count, handle_session_line, session);
}

bool irc_protocol_session_registered(const irc_protocol_session_t* session)
{
    return session != NULL && session->registered;
}
