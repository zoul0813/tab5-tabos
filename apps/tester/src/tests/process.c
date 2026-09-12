#include <tester/test.h>

#include <tabos/process.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <tabos/ipc.h>
#include <tabos/surface.h>
#include <string.h>

void tester_test_concurrent_process(tester_context_t* context)
{
    tester_expect(context, tabos_session_open() > 0, "foreground tester opens inherited GUI session");
    const tabos_ipc_channel_t listener = tabos_ipc_listen();
    tester_expect(context, listener > 0, "session listener opens");
    if (listener <= 0) {
        return;
    }
    const tabos_wait_source_t source = tabos_ipc_wait_source(listener);
    tabos_surface_stats_t baseline   = {0};
    tester_expect(context, tabos_surface_stats(&baseline) == 0, "surface allocation baseline");
    const char* const first_args[]  = {"T:/bin/tester", "--concurrent-peer", "1", NULL};
    const char* const second_args[] = {"T:/bin/tester", "--concurrent-peer", "2", NULL};
    for (unsigned int round = 0U; round < 3U; ++round) {
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
            message.kind = 42U;
            tester_expect(context, tabos_ipc_send(channel, &message, true) == 0, "control reply delivered");
            tester_expect(context, tabos_ipc_close(channel) == 0, "reply channel close preserves queued delivery");
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
}
