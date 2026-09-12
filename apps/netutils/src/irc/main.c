#include <tabos/network.h>
#include <tabos/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

enum {
    IRC_PORT    = 6667,
    LINE_MAX    = 480,
    RECEIVE_MAX = 512,
    MEMBER_MAX  = 64,
    NICK_MAX    = 32
};

typedef struct {
        char nickname[NICK_MAX + 1];
        char status;
} member_t;

static member_t members[MEMBER_MAX];

typedef struct {
        tabos_socket_t socket;
} protocol_callbacks_t;

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: irc host nickname [#channel]\n");
}

static int send_line(tabos_socket_t socket, const char* line)
{
    const size_t length = strlen(line);
    return tabos_socket_send(socket, line, (uint32_t) length) == (int) length ? 0 : -1;
}

static void draw_prompt(const char* line)
{
    printf("> %s", line);
}

static void erase_character(void)
{
    fputs("\b \b", stdout);
}

static int send_command(tabos_socket_t socket, char* line)
{
    char* command = line + 1;
    for (size_t index = 0U; command[index] != '\0' && command[index] != ' '; ++index) {
        command[index] = (char) toupper((unsigned char) command[index]);
    }
    char message[LINE_MAX + 3];
    (void) snprintf(message, sizeof(message), "%s\r\n", command);
    return send_line(socket, message);
}

static void remember_names(char* message)
{
    char* names = strstr(message, " 353 ");
    if (names == NULL) {
        return;
    }
    names = strstr(names, " :");
    if (names == NULL) {
        return;
    }
    names += 2;
    for (unsigned int index = 0U; *names != '\0' && *names != '\r' && index < MEMBER_MAX; ++index) {
        char* end = strchr(names, ' ');
        if (end != NULL) {
            *end = '\0';
        }
        char status = '\0';
        if (strchr("~&@%+", names[0]) != NULL) {
            status = names[0];
            ++names;
        }
        (void) snprintf(members[index].nickname, sizeof(members[index].nickname), "%s", names);
        members[index].status = status;
        if (end == NULL) {
            break;
        }
        names = end + 1;
    }
}

static char member_status(const char* nickname)
{
    for (unsigned int index = 0U; index < MEMBER_MAX; ++index) {
        if (strcmp(members[index].nickname, nickname) == 0) {
            return members[index].status;
        }
    }
    return '\0';
}

static bool display_privmsg(char* message)
{
    if (message[0] != ':') {
        return false;
    }
    char* separator = strstr(message, " PRIVMSG ");
    char* body      = separator == NULL ? NULL : strstr(separator + 9, " :");
    if (separator == NULL || body == NULL) {
        return false;
    }
    char* bang = strchr(message + 1, '!');
    if (bang == NULL || bang > separator) {
        return false;
    }
    *bang         = '\0';
    body         += 2;
    char* ending  = strpbrk(body, "\r\n");
    if (ending != NULL) {
        *ending = '\0';
    }
    const char status = member_status(message + 1);
    if (status == '\0') {
        printf("<%s> %s\n", message + 1, body);
    } else {
        printf("<%c%s> %s\n", status, message + 1, body);
    }
    return true;
}

static void display_server_line(char* message, void* context)
{
    (void) context;
    char names_copy[IRC_PROTOCOL_LINE_MAX + 1];

    (void) snprintf(names_copy, sizeof(names_copy), "%s", message);
    remember_names(names_copy);
    if (!display_privmsg(message)) {
        puts(message);
    }
}

static void send_protocol_line(const char* line, void* opaque_callbacks)
{
    const protocol_callbacks_t* callbacks = opaque_callbacks;
    (void) send_line(callbacks->socket, line);
}

int main(int argc, char** argv)
{
    (void) setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3 || argc > 4) {
        usage(stderr);
        return 2;
    }
    tabos_network_address_t address;
    if (tabos_network_resolve(argv[1], TABOS_NETWORK_FAMILY_ANY, &address) != 0) {
        fprintf(stderr, "irc: resolve: %s\n", strerror(errno));
        return 1;
    }
    const tabos_socket_t socket            = tabos_socket_open(address.family, TABOS_SOCKET_TCP);
    const tabos_socket_endpoint_t endpoint = {.address = address, .port = IRC_PORT};
    if (socket < 0 || tabos_socket_connect(socket, &endpoint) != 0 || tabos_socket_set_nonblocking(socket, true) != 0) {
        fprintf(stderr, "irc: connect: %s\n", strerror(errno));
        if (socket >= 0) {
            (void) tabos_socket_close(socket);
        }
        return 1;
    }
    char protocol_output[IRC_PROTOCOL_LINE_MAX + 8];
    (void) snprintf(protocol_output, sizeof(protocol_output), "NICK %s\r\nUSER %s 0 * :TabOS\r\n", argv[2], argv[2]);
    if (send_line(socket, protocol_output) != 0) {
        fprintf(stderr, "irc: register: %s\n", strerror(errno));
        (void) tabos_socket_close(socket);
        return 1;
    }
    char channel[LINE_MAX + 1]              = {0};
    const char* initial_channel             = argc == 4 ? argv[3] : NULL;
    protocol_callbacks_t protocol_callbacks = {.socket = socket};
    irc_protocol_session_t protocol_session;
    irc_protocol_session_init(&protocol_session, initial_channel, channel, sizeof(channel), display_server_line,
                              send_protocol_line, &protocol_callbacks);
    const int stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
    (void) fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
    puts("IRC connected. /join #channel, /msg target text, /quit; plain text sends to current channel.");
    draw_prompt("");
    char edit_line[LINE_MAX + 1] = {0};
    size_t used                  = 0U;
    bool running                 = true;
    while (running) {
        tabos_wait_item_t item = {
            .source = tabos_socket_wait_source(socket),
            .events = TABOS_WAIT_READABLE,
        };
        if (tabos_wait(&item, 1U, 0U) > 0 && (item.returned_events & TABOS_WAIT_READABLE) != 0U) {
            char received[RECEIVE_MAX + 1];
            const int count = tabos_socket_receive(socket, received, RECEIVE_MAX);
            if (count <= 0) {
                running = false;
                break;
            }
            fputs("\r\033[2K", stdout);
            if (!irc_protocol_session_feed(&protocol_session, received, (size_t) count)) {
                fputs("irc: discarded overlong server message\n", stderr);
            }
            edit_line[used] = '\0';
            draw_prompt(edit_line);
        }
        char character;
        while (read(STDIN_FILENO, &character, 1U) == 1) {
            if (character == '\n' || character == '\r') {
                edit_line[used] = '\0';
                putchar('\n');
                if (strcmp(edit_line, "/quit") == 0) {
                    running = false;
                    break;
                }
                if (strncmp(edit_line, "/join ", 6U) == 0) {
                    if (!irc_protocol_session_registered(&protocol_session)) {
                        puts("Waiting for IRC registration.");
                        used = 0U;
                        draw_prompt("");
                        continue;
                    }
                    (void) snprintf(channel, sizeof(channel), "%s", edit_line + 6);
                    (void) snprintf(protocol_output, sizeof(protocol_output), "JOIN %s\r\n", channel);
                } else if (strncmp(edit_line, "/msg ", 5U) == 0) {
                    char* target = edit_line + 5;
                    char* text   = strchr(target, ' ');
                    if (text == NULL) {
                        puts("Usage: /msg target text");
                        used = 0U;
                        draw_prompt("");
                        continue;
                    }
                    *text++ = '\0';
                    char message[LINE_MAX + 3];
                    (void) snprintf(message, sizeof(message), "PRIVMSG %s :%s\r\n", target, text);
                    if (send_line(socket, message) != 0) {
                        fprintf(stderr, "irc: send: %s\n", strerror(errno));
                    } else {
                        printf("-> %s: %s\n", target, text);
                    }
                    used = 0U;
                    draw_prompt("");
                    continue;
                } else if (edit_line[0] == '/') {
                    (void) send_command(socket, edit_line);
                    used = 0U;
                    draw_prompt("");
                    continue;
                } else if (channel[0] != '\0') {
                    if (!irc_protocol_session_registered(&protocol_session)) {
                        puts("Waiting for IRC registration.");
                        used = 0U;
                        draw_prompt("");
                        continue;
                    }
                    char message[LINE_MAX + 3];
                    (void) snprintf(message, sizeof(message), "PRIVMSG %s :%s\r\n", channel, edit_line);
                    if (send_line(socket, message) != 0) {
                        fprintf(stderr, "irc: send: %s\n", strerror(errno));
                    } else {
                        printf("<\033[32m%s\033[0m> %s\n", argv[2], edit_line);
                    }
                    used = 0U;
                    draw_prompt("");
                    continue;
                } else {
                    puts("Join a channel first.");
                    used = 0U;
                    draw_prompt("");
                    continue;
                }
                (void) send_line(socket, protocol_output);
                used = 0U;
                draw_prompt("");
            } else if (character == '\b' || character == 0x7f) {
                if (used > 0U) {
                    --used;
                    erase_character();
                }
            } else if ((unsigned char) character >= 32U && used < LINE_MAX) {
                edit_line[used++] = character;
                putchar(character);
            }
        }
        (void) sched_yield();
    }
    (void) fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
    (void) tabos_socket_close(socket);
    return 0;
}
