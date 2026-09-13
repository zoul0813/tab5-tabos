#include <tester/test.h>

#include <tabos/process.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <tabos/ipc.h>
#include <tabos/surface.h>
#include <tabos/session.h>
#include <tabos/runtime_time.h>
#include <string.h>

static void test_ipc_wait_lifecycle(tester_context_t* context, tabos_ipc_channel_t listener,
                                    tabos_wait_source_t listener_source)
{
    tabos_wait_source_t stale = TABOS_WAIT_SOURCE_INVALID;
    for (unsigned int round = 0U; round < 3U; ++round) {
        tabos_wait_item_t pending = {.source = listener_source, .events = TABOS_WAIT_READABLE};
        tester_expect(context, tabos_wait(&pending, 1U, 0U) == 0 && pending.returned_events == 0U,
                      "empty listener is not ready");
        const tabos_ipc_channel_t client = tabos_ipc_connect();
        tester_expect(context, client > 0, "local IPC wait fixture connects");
        if (client <= 0) {
            return;
        }
        tester_expect(context, tabos_wait(&pending, 1U, 0U) == 1 && pending.returned_events == TABOS_WAIT_READABLE,
                      "connection queued before wait is immediately readable");
        const tabos_ipc_channel_t server = tabos_ipc_accept(listener);
        tester_expect(context, server > 0, "local IPC wait fixture accepts");
        if (server <= 0) {
            (void) tabos_ipc_close(client);
            return;
        }
        const tabos_wait_source_t source = tabos_ipc_wait_source(server);
        tester_expect(context, source > 0, "accepted channel has generic wait source");
        if (stale != TABOS_WAIT_SOURCE_INVALID) {
            pending = (tabos_wait_item_t) {.source = stale, .events = TABOS_WAIT_READABLE};
            tester_expect(context, tabos_wait(&pending, 1U, 0U) == -1 && errno == EBADF,
                          "reused endpoint and source slots do not revive stale wait");
        }
        pending              = (tabos_wait_item_t) {.source = source, .events = TABOS_WAIT_READABLE};
        const uint64_t start = tabos_monotonic_ms();
        tester_expect(context,
                      tabos_wait(&pending, 1U, 20U) == 0 && pending.returned_events == 0U &&
                          tabos_monotonic_ms() - start >= 20U,
                      "empty IPC channel honors finite timeout");
        const tabos_ipc_message_t message = {.kind = 73U};
        tester_expect(context, tabos_ipc_send(client, &message, true) == 0, "queue control before peer close");
        tester_expect(context, tabos_ipc_close(client) == 0, "close peer with pending control");
        pending.events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP | TABOS_WAIT_WRITABLE;
        tester_expect(context,
                      tabos_wait(&pending, 1U, 0U) == 1 &&
                          pending.returned_events == (TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP),
                      "closed peer retains readable control and reports hangup without writable");
        tabos_ipc_message_t received = {0};
        tester_expect(context, tabos_ipc_receive(server, &received) == 0 && received.kind == message.kind,
                      "drain control after peer close");
        tester_expect(context, tabos_wait(&pending, 1U, 0U) == 1 && pending.returned_events == TABOS_WAIT_HANGUP,
                      "drained channel reports only hangup");
        tester_expect(context, tabos_ipc_close(server) == 0, "close waited channel");
        tester_expect(context, tabos_wait(&pending, 1U, 0U) == -1 && errno == EBADF,
                      "channel close invalidates its wait source");
        stale = source;
    }
}

static void test_surface_rejections(tester_context_t* context)
{
    tabos_surface_stats_t baseline = {0};
    tester_expect(context, tabos_surface_stats(&baseline) == 0, "surface rejection allocation baseline");
    const uint32_t dimensions[][2] = {
        {        0U,         2U},
        {        2U,         0U},
        {     1281U,         2U},
        {        2U,       721U},
        {UINT32_MAX, UINT32_MAX}
    };
    for (size_t index = 0U; index < sizeof(dimensions) / sizeof(dimensions[0]); ++index) {
        tester_expect(context,
                      tabos_surface_create(dimensions[index][0], dimensions[index][1]) == -1 && errno == EINVAL,
                      "SDK rejects invalid surface dimensions");
    }
    const tabos_surface_t surface = tabos_surface_create(2U, 2U);
    tester_expect(context, surface > 0, "create SDK surface rejection fixture");
    if (surface <= 0) {
        return;
    }
    const uint16_t original[4] = {1U, 2U, 3U, 4U};
    const uint16_t staged[4]   = {5U, 6U, 7U, 8U};
    tester_expect(context,
                  tabos_surface_upload(surface, 0U, 0U, 2U, 2U, original) == 0 && tabos_surface_commit(surface) == 0,
                  "commit surface rejection baseline");
    const uint32_t rectangles[][4] = {
        {        0U,         0U,         0U,         2U},
        {        0U,         0U,         2U,         0U},
        {        2U,         0U,         1U,         1U},
        {        0U,         2U,         1U,         1U},
        {        1U,         0U,         2U,         1U},
        {        0U,         1U,         1U,         2U},
        {UINT32_MAX,         0U,         1U,         1U},
        {        0U, UINT32_MAX,         1U,         1U},
        {        0U,         0U, UINT32_MAX,         1U},
        {        0U,         0U,         1U, UINT32_MAX},
    };
    for (unsigned int round = 0U; round < 3U; ++round) {
        for (size_t index = 0U; index <= sizeof(rectangles) / sizeof(rectangles[0]); ++index) {
            const bool null_buffer    = index == sizeof(rectangles) / sizeof(rectangles[0]);
            const uint32_t valid[4]   = {0U, 0U, 2U, 2U};
            const uint32_t* rectangle = null_buffer ? valid : rectangles[index];
            uint16_t guarded[6]       = {99U, 99U, 99U, 99U, 99U, 99U};
            uint16_t* output          = null_buffer ? NULL : guarded + 1;
            tester_expect(context, tabos_surface_upload(surface, 0U, 0U, 2U, 2U, staged) == 0,
                          "stage pixels before SDK rejection");
            tester_expect(context,
                          tabos_surface_read(surface, rectangle[0], rectangle[1], rectangle[2], rectangle[3], output) ==
                                  -1 &&
                              errno == EINVAL,
                          "invalid SDK read rejects geometry or null buffer");
            bool unchanged = true;
            for (size_t pixel = 0U; pixel < 6U; ++pixel) {
                unchanged = unchanged && guarded[pixel] == 99U;
            }
            tabos_surface_stats_t stats = {0};
            tester_expect(context,
                          unchanged && tabos_surface_stats(&stats) == 0 &&
                              stats.used_bytes == baseline.used_bytes + 2U * sizeof(original),
                          "failed SDK read preserves output and staging");
            tester_expect(context,
                          tabos_surface_upload(surface, rectangle[0], rectangle[1], rectangle[2], rectangle[3],
                                               null_buffer ? NULL : staged) == -1 &&
                              errno == EINVAL,
                          "invalid SDK upload rejects and aborts staging");
            tabos_surface_info_t info = {0};
            tester_expect(context,
                          tabos_surface_commit(surface) == 0 && tabos_surface_info(surface, &info) == 0 &&
                              info.revision == 1U && tabos_surface_read(surface, 0U, 0U, 2U, 2U, guarded + 1) == 0 &&
                              memcmp(guarded + 1, original, sizeof(original)) == 0 && guarded[0] == 99U &&
                              guarded[5] == 99U,
                          "failed upload preserves committed image and revision");
            tester_expect(
                context, tabos_surface_stats(&stats) == 0 && stats.used_bytes == baseline.used_bytes + sizeof(original),
                "SDK rejected upload releases staging allocation");
        }
    }
    tester_expect(context, tabos_surface_release(surface) == 0, "release SDK surface rejection fixture");
    tabos_surface_stats_t after = {0};
    tester_expect(context, tabos_surface_stats(&after) == 0 && after.used_bytes == baseline.used_bytes,
                  "SDK rejection cases return allocations to baseline");
}

void tester_test_concurrent_process(tester_context_t* context)
{
    tabos_program_info_t program;
    tester_expect(context,
                  tabos_program_query("T:/bin/tester", &program) == 0 && program.flags == 0U &&
                      program.heap_bytes > 0U && program.image_bytes > 0U,
                  "query unmarked executable without launching it");
    tester_expect(context, tabos_program_query("T:/no-such-program", &program) == -1, "query missing executable fails");
    tester_expect(context, tabos_session_open() > 0, "foreground tester opens inherited GUI session");
    const tabos_ipc_channel_t listener = tabos_ipc_listen();
    tester_expect(context, listener > 0, "session listener opens");
    if (listener <= 0) {
        return;
    }
    const tabos_wait_source_t source = tabos_ipc_wait_source(listener);
    test_ipc_wait_lifecycle(context, listener, source);
    test_surface_rejections(context);
    tabos_surface_stats_t baseline = {0};
    tester_expect(context, tabos_surface_stats(&baseline) == 0, "surface allocation baseline");
    const char* const first_args[]  = {"T:/bin/tester", "--concurrent-peer", "1", NULL};
    const char* const second_args[] = {"T:/bin/tester", "--concurrent-peer", "2", NULL};
    for (unsigned int round = 0U; round < 3U; ++round) {
        tabos_surface_t granted[2]              = {-1, -1};
        tabos_ipc_channel_t channels[2]         = {-1, -1};
        const int first                         = tabos_spawn(first_args[0], 3, first_args);
        const int second                        = tabos_spawn(second_args[0], 3, second_args);
        const tabos_wait_source_t first_source  = tabos_process_wait_source(first);
        const tabos_wait_source_t second_source = tabos_process_wait_source(second);
        tester_expect(context, first_source != TABOS_WAIT_SOURCE_INVALID && second_source != TABOS_WAIT_SOURCE_INVALID,
                      "each child supplies an exit wait source");
        tester_expect(context, first > 0 && second > 0 && first != second, "concurrent children have distinct PIDs");
        for (unsigned int index = 0U; index < 2U; ++index) {
            tabos_wait_item_t pending = {.source = source, .events = TABOS_WAIT_READABLE};
            tester_expect(context, tabos_wait(&pending, 1U, 10000U) == 1, "listener wakes for peer connection");
            const tabos_ipc_channel_t channel = tabos_ipc_accept(listener);
            channels[index]                   = channel;
            tester_expect(context, channel > 0, "accept grants private reply channel");
            if (channel <= 0) {
                continue;
            }
            pending = (tabos_wait_item_t) {.source = tabos_ipc_wait_source(channel), .events = TABOS_WAIT_READABLE};
            tester_expect(context, tabos_wait(&pending, 1U, 10000U) == 1, "channel readable wait wakes");
            tabos_ipc_message_t message = {0};
            tester_expect(context,
                          tabos_ipc_receive(channel, &message) == 0 &&
                              (message.sender_pid == (uint32_t) first || message.sender_pid == (uint32_t) second),
                          "copied IPC carries actual child identity");
            tabos_surface_t surface = -1;
            if (message.size == sizeof(surface)) {
                memcpy(&surface, message.data, sizeof(surface));
            }
            granted[index]            = surface;
            tabos_surface_info_t info = {0};
            uint16_t pixels[4]        = {0};
            tester_expect(context,
                          tabos_surface_info(surface, &info) == 0 && info.width == 2U && info.height == 2U &&
                              info.revision == 1U,
                          "compositor grant exposes committed surface geometry");
            tester_expect(context,
                          tabos_surface_read(surface, 0U, 0U, 2U, 2U, pixels) == 0 && pixels[0] == 0xf800U &&
                              pixels[1] == 0x07e0U && pixels[2] == 0x001fU && pixels[3] == 0xffffU,
                          "cross-process read sees committed pixels while next upload is staged");
            tester_expect(context, tabos_surface_release(surface) == -1 && errno == EBADF,
                          "compositor read grant cannot release client surface");
        }
        const int token = tabos_session_control(TABOS_SESSION_BEGIN, 0U, 0U);
        tester_expect(context, token > 0, "begin resident session pause");
        tester_expect(context, tabos_spawn(first_args[0], 3, first_args) == -EBUSY, "pause closes launch admission");
        int blocker = -1;
        for (unsigned int attempt = 0U; attempt < 200U; ++attempt) {
            blocker = tabos_session_control(TABOS_SESSION_STATUS, (uint32_t) token, 0U);
            if (blocker <= 0) {
                break;
            }
            tabos_sleep_ms(5U);
        }
        tester_expect(context, blocker == 0, "both real RV32 clients acknowledge and remain resident");
        tester_expect(context, tabos_session_control(TABOS_SESSION_RESUME, (uint32_t) token, 0U) == 0,
                      "resume parked RV32 clients");
        for (unsigned int index = 0U; index < 2U; ++index) {
            tabos_ipc_message_t message = {.kind = 42U};
            tester_expect(context, tabos_ipc_send(channels[index], &message, true) == 0, "control reply delivered");
            tester_expect(context, tabos_ipc_close(channels[index]) == 0,
                          "reply channel close preserves queued delivery");
        }
        int first_status  = -1;
        int second_status = -1;
        if (first > 0) {
            tabos_wait_item_t exited = {.source = first_source, .events = TABOS_WAIT_READABLE};
            tester_expect(context, tabos_wait(&exited, 1U, 10000U) == 1, "first child exit wakes generic wait");
            tester_expect(context, tabos_waitpid(first, &first_status) == first, "wait reaps first actual PID");
            tester_expect(context, tabos_waitpid(first, NULL) == -ECHILD, "second reap rejected");
            tester_expect(context, tabos_wait(&exited, 1U, 0U) == -1 && errno == EBADF,
                          "reaping invalidates child wait source");
        }
        if (second > 0) {
            tabos_wait_item_t exited = {.source = second_source, .events = TABOS_WAIT_READABLE};
            tester_expect(context, tabos_wait(&exited, 1U, 10000U) == 1, "second child exit wakes generic wait");
            tester_expect(context, tabos_waitpid(second, &second_status) == second, "wait reaps second actual PID");
        }
        tester_expect(context, first_status == 91 && second_status == 92,
                      "both independent RV32 children progress while parent waits; background display denied");
        for (unsigned int index = 0U; index < 2U; ++index) {
            tabos_surface_info_t stale_info = {.width = 99U, .height = 99U, .revision = 99U};
            uint16_t stale_pixels[4]        = {99U, 99U, 99U, 99U};
            tester_expect(context,
                          granted[index] > 0 && tabos_surface_info(granted[index], &stale_info) == -1 &&
                              errno == EBADF && stale_info.width == 99U && stale_info.height == 99U &&
                              stale_info.revision == 99U,
                          "owner exit revokes surface metadata grant");
            tester_expect(context,
                          tabos_surface_read(granted[index], 0U, 0U, 2U, 2U, stale_pixels) == -1 && errno == EBADF &&
                              stale_pixels[0] == 99U && stale_pixels[1] == 99U && stale_pixels[2] == 99U &&
                              stale_pixels[3] == 99U,
                          "owner exit revokes surface read grant without touching output");
        }
        (void) unlink("T:/tabos-concurrent-1.tmp");
        (void) unlink("T:/tabos-concurrent-2.tmp");
        tabos_surface_stats_t stats = {0};
        tester_expect(context, tabos_surface_stats(&stats) == 0 && stats.used_bytes == baseline.used_bytes,
                      "normal and leaked staged surfaces reclaimed on child exit");
    }
    tester_expect(context, tabos_ipc_close(listener) == 0, "listener cleanup");
}

void tester_test_process(tester_context_t* context)
{
    tester_test_concurrent_process(context);
    const int pause_token = tabos_session_control(TABOS_SESSION_BEGIN, 0U, 0U);
    tester_expect(context, pause_token > 0 && tabos_session_control(TABOS_SESSION_STATUS, pause_token, 0U) == 0,
                  "empty GUI session parks before fullscreen chain");
    const char* const arguments[] = {
        "T:/bin/tester",
        "--process-child",
        NULL,
    };

    const int first_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, first_status == 37, "child returns status after grandchild exits");

    const int second_status = tabos_exec(arguments[0], 2, arguments);
    tester_expect(context, second_status == 37, "nested process chain reloads after cleanup");

    const char* const resource_arguments[] = {
        "T:/bin/tester",
        "--process-resource-failure",
        NULL,
    };
    bool failures_returned = true;
    for (unsigned int index = 0U; index < 6U; ++index) {
        const int status = tabos_exec(resource_arguments[0], 2, resource_arguments);
        if (status != 73) {
            printf("    resource child %u returned %d; expected 73\n", index + 1U, status);
            failures_returned = false;
            break;
        }
    }
    tester_expect(context, failures_returned,
                  "failed children return status after leaking owned files, heap, sockets, and subscriptions");

    const int descriptor = open("T:/tabos-process-resource.tmp", O_RDWR);
    tester_expect(context, descriptor >= 3, "child descriptor resources are reclaimed after failure");
    if (descriptor >= 0) {
        (void) close(descriptor);
    }
    tester_expect(context, unlink("T:/tabos-process-resource.tmp") == 0,
                  "parent resumes and removes failed child fixture");
    tester_expect(context, tabos_session_control(TABOS_SESSION_RESUME, pause_token, 0U) == 0,
                  "GUI session resumes after fullscreen chain");
}
