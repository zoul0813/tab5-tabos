#include <tester/test.h>

#include <tabos/process.h>
#include <tabos/audio.h>
#include <tabos/network.h>
#include <tabos/device.h>
#include <tabos/pointer.h>
#include <tabos/wait.h>
#include <tabos/ansi.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <tabos/tty.h>
#include <tabos/graphics.h>
#include <tabos/runtime_time.h>
#include <sys/stat.h>
#include <tabos/ipc.h>
#include <tabos/surface.h>
#include <tabos/session.h>

void tester_test_concurrent_process(tester_context_t* context);

static int run_concurrent_peer(const char* index)
{
    const bool first = strcmp(index, "1") == 0;
    if (!first && strcmp(index, "2") != 0) {
        return 1;
    }
    tabos_graphics_t graphics = {0};
    if (tabos_graphics_open(&graphics) == 0) {
        (void) tabos_graphics_close(&graphics);
        return 2;
    }
    const char* own      = first ? "T:/tabos-concurrent-1.tmp" : "T:/tabos-concurrent-2.tmp";
    const char* peer     = first ? "T:/tabos-concurrent-2.tmp" : "T:/tabos-concurrent-1.tmp";
    const int descriptor = open(own, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (descriptor < 0) {
        return 3;
    }
    if (close(descriptor) != 0) {
        return 4;
    }
    const uint64_t deadline = tabos_monotonic_ms() + 10000U;
    struct stat info;
    while (stat(peer, &info) != 0) {
        if (tabos_monotonic_ms() >= deadline) {
            return 5;
        }
        (void) tabos_sleep_ms(1U);
    }
    const tabos_ipc_channel_t channel = tabos_ipc_connect();
    if (channel <= 0) {
        return 6;
    }
    tabos_ipc_message_t message   = {.kind = first ? 1U : 2U};
    const tabos_surface_t surface = tabos_surface_create(2U, 2U);
    uint16_t pixels[4]            = {0xf800U, 0x07e0U, 0x001fU, 0xffffU};
    if (surface <= 0 || tabos_surface_upload(surface, 0U, 0U, 2U, 2U, pixels) != 0 ||
        tabos_surface_commit(surface) != 0 || tabos_surface_grant(surface, channel) != 0) {
        return 10;
    }
    memset(pixels, 0, sizeof(pixels));
    if (tabos_surface_upload(surface, 0U, 0U, 2U, 2U, pixels) != 0) {
        return 11;
    }
    message.size = sizeof(surface);
    memcpy(message.data, &surface, sizeof(surface));
    if (tabos_ipc_send(channel, &message, false) != 0) {
        return 7;
    }
    tabos_wait_item_t pending = {.source = tabos_ipc_wait_source(channel), .events = TABOS_WAIT_READABLE};
    int ready                 = 0;
    for (unsigned int attempt = 0U; attempt < 400U && ready == 0; ++attempt) {
        const int token = tabos_session_control(TABOS_SESSION_CHECKPOINT, 0U, 0U);
        if (token > 0 && tabos_session_control(TABOS_SESSION_ACKNOWLEDGE, (uint32_t) token, 0U) != 0) {
            return 13;
        }
        ready = tabos_wait(&pending, 1U, 25U);
    }
    if (ready != 1 || tabos_ipc_receive(channel, &message) != 0 || message.kind != 42U) {
        return 8;
    }
    if (tabos_ipc_close(channel) != 0) {
        return 9;
    }
    if (first && (tabos_surface_abort(surface) != 0 || tabos_surface_release(surface) != 0)) {
        return 12;
    }
    /* Second peer intentionally leaves committed/staged images for exit cleanup. */
    return first ? 91 : 92;
}

enum {
    PROCESS_LEAK_DESCRIPTOR_COUNT   = 8,
    PROCESS_LEAK_SOCKET_COUNT       = 4,
    PROCESS_LEAK_SUBSCRIPTION_COUNT = 4,
    PROCESS_LEAK_AUDIO_COUNT        = 2,
    PROCESS_LEAK_POINTER_COUNT      = 1,
};

static int run_resource_failure_fixture(void)
{
    void* allocation = malloc(4096U);
    if (allocation == NULL) {
        return 74;
    }
    memset(allocation, 0x5a, 4096U);

    for (unsigned int index = 0U; index < PROCESS_LEAK_DESCRIPTOR_COUNT; ++index) {
        const int descriptor = open("T:/tabos-process-resource.tmp", O_CREAT | O_RDWR, 0644);
        if (descriptor < 0) {
            return 75;
        }
    }
    for (unsigned int index = 0U; index < PROCESS_LEAK_SOCKET_COUNT; ++index) {
        if (tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP) < 0) {
            return 76;
        }
    }
    for (unsigned int index = 0U; index < PROCESS_LEAK_SUBSCRIPTION_COUNT; ++index) {
        const tabos_device_subscription_t subscription = tabos_device_subscribe();
        if (subscription == TABOS_DEVICE_SUBSCRIPTION_INVALID ||
            tabos_device_subscription_wait_source(subscription) == TABOS_WAIT_SOURCE_INVALID) {
            return 77;
        }
    }
    const tabos_audio_config_t audio_config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_SPEAKER,
    };
    for (unsigned int index = 0U; index < PROCESS_LEAK_AUDIO_COUNT; ++index) {
        const tabos_audio_stream_t stream = tabos_audio_open(&audio_config);
        if (stream == TABOS_AUDIO_STREAM_INVALID || tabos_audio_wait_source(stream) == TABOS_WAIT_SOURCE_INVALID) {
            return 78;
        }
    }
    tabos_device_info_t pointer_device;
    if (tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &pointer_device) == 0) {
        for (unsigned int index = 0U; index < PROCESS_LEAK_POINTER_COUNT; ++index) {
            const tabos_pointer_stream_t stream = tabos_pointer_open(pointer_device.id);
            if (stream == TABOS_POINTER_STREAM_INVALID ||
                tabos_pointer_wait_source(stream) == TABOS_WAIT_SOURCE_INVALID) {
                return 85;
            }
        }
    }
    return 73;
}

static int run_process_fixture(int argc, char** argv)
{
    if (argc < 2 || argv == NULL) {
        return -1;
    }
    if (argc == 3 && strcmp(argv[1], "--foreign-wait-source") == 0) {
        char* end              = NULL;
        const long parsed      = strtol(argv[2], &end, 10);
        tabos_wait_item_t item = {
            .source = (tabos_wait_source_t) parsed,
            .events = TABOS_WAIT_READABLE,
        };
        errno               = 0;
        const bool rejected = end != argv[2] && *end == '\0' && tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF;
        return rejected ? 79 : 80;
    }
    if (argc != 2) {
        return -1;
    }
    if (strcmp(argv[1], "--input-source") == 0) {
        if (ioctl(0, TABOS_TTY_SET_MODE, (uint32_t) 0U) != 0) {
            return 1;
        }
        return tabos_input_wait_source();
    }
    if (strcmp(argv[1], "--process-leaf") == 0) {
        return 23;
    }
    if (strcmp(argv[1], "--process-resource-failure") == 0) {
        return run_resource_failure_fixture();
    }
    if (strcmp(argv[1], "--network-disconnect") == 0) {
        return tabos_network_disconnect() == 0 ? 81 : 82;
    }
    if (strcmp(argv[1], "--network-connect-saved") == 0) {
        return tabos_network_connect_saved() == 0 ? 83 : 84;
    }
    if (strcmp(argv[1], "--process-child") != 0) {
        return -1;
    }

    const char* const arguments[] = {
        "T:/bin/tester",
        "--process-leaf",
        NULL,
    };
    const int status = tabos_exec(arguments[0], 2, arguments);
    return status == 23 ? 37 : 38;
}

int main(int argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "--concurrent-peer") == 0) {
        return run_concurrent_peer(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "--concurrent") == 0) {
        tester_context_t context = {.argc = argc, .argv = argv};
        tester_test_concurrent_process(&context);
        printf("Concurrent assertions: %u; failures: %u\n", context.assertions, context.failures);
        return context.failures == 0U ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--input") == 0) {
        tester_context_t context = {.argc = argc, .argv = argv};
        tester_test_input(&context);
        printf("Input assertions: %u; failures: %u\n", context.assertions, context.failures);
        return context.failures == 0U ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--camera-leak") == 0) {
        return tester_camera_leak_fixture();
    }
    if (argc == 2 && strcmp(argv[1], "--camera-cleanup") == 0) {
        tester_context_t context = {.argc = argc, .argv = argv};
        tester_test_camera_cleanup(&context);
        printf("Camera cleanup assertions: %u; failures: %u\n", context.assertions, context.failures);
        return context.failures == 0U ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--camera-services") == 0) {
        tester_context_t context = {.argc = argc, .argv = argv};
        tester_test_camera_services(&context);
        printf("Camera service assertions: %u; failures: %u\n", context.assertions, context.failures);
        return context.failures == 0U ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--camera") == 0) {
        tester_context_t context = {.argc = argc, .argv = argv};
        tester_test_camera(&context);
        printf("Camera assertions: %u; failures: %u\n", context.assertions, context.failures);
        return context.failures == 0U ? 0 : 1;
    }
    const int fixture_status = run_process_fixture(argc, argv);
    if (fixture_status >= 0) {
        return fixture_status;
    }

    static const tester_test_t tests[] = {
        {                       "Arguments",  tester_test_arguments},
        {                    "Standard I/O",      tester_test_stdio},
        {                            "Heap",       tester_test_heap},
        {"Filesystem and working directory", tester_test_filesystem},
        {               "Nonblocking input",      tester_test_input},
        {        "Nested process execution",    tester_test_process},
        {     "Time and system information",    tester_test_runtime},
        {          "Device registry access",     tester_test_device},
        {             "Battery integration",    tester_test_battery},
        {               "Audio integration",      tester_test_audio},
        {             "Pointer integration",    tester_test_pointer},
        {              "Camera integration",     tester_test_camera},
        {              "TCP/UDP networking",    tester_test_network},
        {             "Fullscreen graphics",   tester_test_graphics},
    };
    tester_context_t context = {.argc = argc, .argv = argv};

    puts("TabOS SDK tester");
    for (unsigned int index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        tester_run_test(&context, &tests[index]);
    }
    printf("\nAssertions: %u; failures: %u\n", context.assertions, context.failures);
    puts(context.failures == 0U ? "[" ANSI_GREEN "PASS" ANSI_RESET "] TabOS SDK tester" :
                                  "[" ANSI_RED "FAIL" ANSI_RESET "] TabOS SDK tester");
    return context.failures == 0U ? 0 : 1;
}
