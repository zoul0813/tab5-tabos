#include <tabos/internal/audio.h>
#include <tabos/platform/platform.h>
#include <tabos/wait.h>

#include <stdint.h>
#include <stdio.h>

enum {
    TEST_TIMEOUT_MS = 1000,
};

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        (void) fprintf(stderr, "host headless audio test failed: %s\n", message);
        ++failures;
    }
}

static bool wait_for(const void* owner, tabos_audio_stream_t stream, uint32_t requested)
{
    const uint64_t deadline = platform_time_ms() + TEST_TIMEOUT_MS;
    bool audio_woke         = false;
    while (platform_time_ms() < deadline) {
        uint32_t returned = 0U;
        if (audio_service_poll(owner, stream, requested, &returned) != 0) {
            return false;
        }
        if ((returned & requested) != 0U && audio_woke) {
            return true;
        }
        const platform_runtime_events_t events = platform_runtime_wait_until(deadline);
        audio_woke                             = audio_woke || (events & PLATFORM_RUNTIME_EVENT_AUDIO) != 0U;
    }
    return false;
}

int main(void)
{
    static const int owner;
    expect(platform_init(true), "headless platform initializes");
    expect(audio_service_init(), "audio service initializes");

    tabos_audio_info_t info = {0};
    const char* driver      = NULL;
    int error               = -1;
    expect(audio_service_info(&info, &driver, &error) && driver != NULL && error == 0 &&
               (info.features & (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE)) ==
                   (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE),
           "headless backend reports working playback and capture");

    const tabos_audio_config_t playback_config = {
        .direction = TABOS_AUDIO_PLAYBACK, .channels = 2U, .route = TABOS_AUDIO_ROUTE_SPEAKER};
    const tabos_audio_config_t capture_config = {
        .direction = TABOS_AUDIO_CAPTURE, .channels = 1U, .route = TABOS_AUDIO_ROUTE_MICROPHONE};
    const tabos_audio_stream_t playback = audio_service_open(&owner, &playback_config);
    const tabos_audio_stream_t capture  = audio_service_open(&owner, &capture_config);
    expect(playback > 0 && capture > 0, "headless streams open");

    int16_t pcm[TABOS_AUDIO_IO_MAX / sizeof(int16_t)] = {0};
    tabos_audio_status_t status                       = {0};
    while (playback > 0 && audio_service_write(&owner, playback, pcm, sizeof(pcm)) > 0) {}
    expect(audio_service_get_status(&owner, playback, &status) == 0 && status.buffered_bytes == status.buffer_capacity,
           "playback ring fills before regression wait");
    expect(wait_for(&owner, playback, TABOS_WAIT_WRITABLE), "playback progress wakes a writable wait");

    const uint64_t drain_deadline = platform_time_ms() + TEST_TIMEOUT_MS;
    do {
        expect(audio_service_get_status(&owner, playback, &status) == 0, "playback status remains readable");
        if (status.buffered_bytes == 0U) {
            break;
        }
        (void) platform_runtime_wait_until(drain_deadline);
    } while (platform_time_ms() < drain_deadline);
    expect(status.buffered_bytes == 0U, "headless playback drains completely");

    expect(wait_for(&owner, capture, TABOS_WAIT_READABLE), "headless capture becomes readable and wakes wait");
    const int captured = audio_service_read(&owner, capture, pcm, sizeof(pcm));
    expect(captured > 0, "headless capture supplies PCM");
    for (int index = 0; index < captured / (int) sizeof(*pcm); ++index) {
        expect(pcm[index] == 0, "headless capture is deterministic silence");
    }

    audio_service_close_owner(&owner);
    audio_service_shutdown();
    platform_shutdown();
    return failures == 0 ? 0 : 1;
}
