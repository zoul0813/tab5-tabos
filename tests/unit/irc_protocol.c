#include "protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
        char lines[8][IRC_PROTOCOL_LINE_MAX + 1];
        size_t count;
        char sent[4][IRC_PROTOCOL_LINE_MAX + 8];
        size_t sent_count;
} captured_lines_t;

static void capture_line(char* line, void* context)
{
    captured_lines_t* captured = context;
    assert(captured->count < 8U);
    (void) snprintf(captured->lines[captured->count], sizeof(captured->lines[0]), "%s", line);
    ++captured->count;
}

static void capture_send(const char* line, void* context)
{
    captured_lines_t* captured = context;
    assert(captured->sent_count < 4U);
    (void) snprintf(captured->sent[captured->sent_count], sizeof(captured->sent[0]), "%s", line);
    ++captured->sent_count;
}

static void test_fragmented_and_coalesced_messages(void)
{
    irc_protocol_buffer_t buffer;
    captured_lines_t captured = {0};
    irc_protocol_buffer_init(&buffer);

    const char first[] = "PI";
    assert(irc_protocol_buffer_feed(&buffer, first, sizeof(first) - 1U, capture_line, &captured));
    const char second[] = "NG :server\r\n:server 00";
    assert(irc_protocol_buffer_feed(&buffer, second, sizeof(second) - 1U, capture_line, &captured));
    assert(captured.count == 1U);
    const char third[] = "1 nick :Welcome\r\n:alice!user@host PRIV";
    assert(irc_protocol_buffer_feed(&buffer, third, sizeof(third) - 1U, capture_line, &captured));
    assert(captured.count == 2U);
    const char fourth[] = "MSG #tabos :hello\r\n:bob!user@host PRIVMSG #tabos :second\r\n";
    assert(irc_protocol_buffer_feed(&buffer, fourth, sizeof(fourth) - 1U, capture_line, &captured));
    assert(captured.count == 4U);
    assert(strcmp(captured.lines[0], "PING :server") == 0);
    assert(strcmp(captured.lines[1], ":server 001 nick :Welcome") == 0);
    assert(strcmp(captured.lines[2], ":alice!user@host PRIVMSG #tabos :hello") == 0);
    assert(strcmp(captured.lines[3], ":bob!user@host PRIVMSG #tabos :second") == 0);
}

static void test_protocol_output_does_not_touch_draft(void)
{
    char draft[]              = "partially typed message";
    char channel[64]          = {0};
    captured_lines_t captured = {0};
    irc_protocol_session_t session;
    irc_protocol_session_init(&session, "#tabos", channel, sizeof(channel), capture_line, capture_send, &captured);

    const char messages[] = "PING :server\r\n:server 001 nick :Welcome\r\n";
    assert(irc_protocol_session_feed(&session, messages, sizeof(messages) - 1U));
    assert(irc_protocol_session_registered(&session));
    assert(strcmp(channel, "#tabos") == 0);
    assert(captured.sent_count == 2U);
    assert(strcmp(captured.sent[0], "PONG :server\r\n") == 0);
    assert(strcmp(captured.sent[1], "JOIN #tabos\r\n") == 0);
    assert(strcmp(draft, "partially typed message") == 0);
}

static void test_overlong_message_recovers(void)
{
    irc_protocol_buffer_t buffer;
    captured_lines_t captured = {0};
    char overlong[IRC_PROTOCOL_LINE_MAX + 8];
    memset(overlong, 'x', sizeof(overlong));
    irc_protocol_buffer_init(&buffer);

    assert(!irc_protocol_buffer_feed(&buffer, overlong, sizeof(overlong), capture_line, &captured));
    const char recovery[] = "\r\nPING :ok\r\n";
    assert(irc_protocol_buffer_feed(&buffer, recovery, sizeof(recovery) - 1U, capture_line, &captured));
    assert(captured.count == 1U);
    assert(strcmp(captured.lines[0], "PING :ok") == 0);
}

int main(void)
{
    test_fragmented_and_coalesced_messages();
    test_protocol_output_does_not_touch_draft();
    test_overlong_message_recovers();
    puts("irc protocol tests passed");
    return 0;
}
