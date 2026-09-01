#include <tabos/internal/audio.h>

#include <tabos/filesystem.h>
#include <tabos/wait.h>

#include "platform_test.h"

#include <stdint.h>
#include <stdio.h>

static int fail(const char* message)
{
    (void) fprintf(stderr, "%s\n", message);
    audio_service_shutdown();
    return 1;
}

int main(void)
{
    static int owner_a;
    static int owner_b;
    if (!audio_service_init()) {
        return fail("audio service init failed");
    }
    tabos_audio_info_t info;
    const char* driver = NULL;
    int error          = -1;
    if (!audio_service_info(&info, &driver, &error) || driver == NULL || error != 0 || info.capture_channels != 4U ||
        info.sample_rates != TABOS_AUDIO_RATES_ALL || info.default_sample_rate != TABOS_AUDIO_DEFAULT_SAMPLE_RATE ||
        test_platform_audio_sample_rate() != TABOS_AUDIO_DEFAULT_SAMPLE_RATE ||
        (info.features & (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE)) !=
            (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE)) {
        return fail("audio service info mismatch");
    }

    tabos_audio_stream_t playback[4];
    const tabos_audio_config_t playback_config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_SPEAKER,
    };
    const tabos_audio_config_t native_rate_config = {
        .direction   = TABOS_AUDIO_PLAYBACK,
        .channels    = 2U,
        .route       = TABOS_AUDIO_ROUTE_SPEAKER,
        .sample_rate = TABOS_AUDIO_SAMPLE_RATE_11025,
    };
    const tabos_audio_stream_t native_rate = audio_service_open(&owner_a, &native_rate_config);
    tabos_audio_config_t conflicting_rate_config = native_rate_config;
    conflicting_rate_config.sample_rate          = TABOS_AUDIO_SAMPLE_RATE_48000;
    tabos_audio_config_t unsupported_rate_config = native_rate_config;
    unsupported_rate_config.sample_rate          = 12345U;
    if (native_rate <= 0 || test_platform_audio_sample_rate() != TABOS_AUDIO_SAMPLE_RATE_11025 ||
        audio_service_open(&owner_b, &conflicting_rate_config) != -TABOS_EBUSY ||
        audio_service_open(&owner_b, &unsupported_rate_config) != -TABOS_EINVAL ||
        audio_service_close(&owner_a, native_rate) != 0) {
        return fail("sample-rate selection or shared-clock arbitration failed");
    }
    const tabos_audio_stream_t default_rate = audio_service_open(&owner_a, &playback_config);
    if (default_rate <= 0 || test_platform_audio_sample_rate() != TABOS_AUDIO_DEFAULT_SAMPLE_RATE ||
        audio_service_close(&owner_a, default_rate) != 0) {
        return fail("default sample rate was not restored on next open");
    }
    static const uint32_t native_rates[] = {
        TABOS_AUDIO_SAMPLE_RATE_8000,  TABOS_AUDIO_SAMPLE_RATE_11025, TABOS_AUDIO_SAMPLE_RATE_12000,
        TABOS_AUDIO_SAMPLE_RATE_16000, TABOS_AUDIO_SAMPLE_RATE_22050, TABOS_AUDIO_SAMPLE_RATE_24000,
        TABOS_AUDIO_SAMPLE_RATE_32000, TABOS_AUDIO_SAMPLE_RATE_44100, TABOS_AUDIO_SAMPLE_RATE_48000,
        TABOS_AUDIO_SAMPLE_RATE_88200, TABOS_AUDIO_SAMPLE_RATE_96000,
    };
    for (size_t index = 0U; index < sizeof(native_rates) / sizeof(native_rates[0]); ++index) {
        tabos_audio_config_t config = playback_config;
        config.sample_rate          = native_rates[index];
        const tabos_audio_stream_t stream = audio_service_open(&owner_a, &config);
        if (stream <= 0 || test_platform_audio_sample_rate() != native_rates[index] ||
            audio_service_close(&owner_a, stream) != 0) {
            return fail("supported native sample rate did not open");
        }
    }
    const int16_t sample = 2000;
    for (size_t index = 0U; index < 4U; ++index) {
        playback[index] = audio_service_open(&owner_a, &playback_config);
        if (playback[index] == TABOS_AUDIO_STREAM_INVALID ||
            audio_service_write(&owner_a, playback[index], &sample, sizeof(sample)) != (int) sizeof(sample)) {
            return fail("four playback streams did not open and accept PCM");
        }
    }
    int16_t mixed[2] = {0};
    test_platform_audio_render(mixed, 1U);
    if (mixed[0] != 8000 || mixed[1] != 8000) {
        return fail("four-stream mono mix mismatch");
    }
    if (audio_service_set_volume(&owner_a, playback[0], 500U) != 0 ||
        audio_service_write(&owner_a, playback[0], &sample, sizeof(sample)) != (int) sizeof(sample)) {
        return fail("per-stream volume setup failed");
    }
    test_platform_audio_render(mixed, 1U);
    if (mixed[0] != 1000 || mixed[1] != 1000) {
        return fail("per-stream volume mix mismatch");
    }
    uint32_t ready = 0U;
    if (audio_service_poll(&owner_a, playback[0], TABOS_WAIT_WRITABLE, &ready) != 0 || ready != TABOS_WAIT_WRITABLE ||
        audio_service_close(&owner_b, playback[0]) != -TABOS_EBADF) {
        return fail("playback ownership or writable readiness mismatch");
    }
    tabos_audio_status_t status;
    if (audio_service_write(&owner_a, playback[0], &sample, sizeof(sample)) != (int) sizeof(sample) ||
        audio_service_flush(&owner_a, playback[0]) != 0 ||
        audio_service_get_status(&owner_a, playback[0], &status) != 0 || status.buffered_bytes != 0U ||
        audio_service_flush(&owner_b, playback[0]) != -TABOS_EBADF) {
        return fail("playback flush did not discard buffered PCM");
    }

    const tabos_audio_config_t capture_config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_stream_t capture = audio_service_open(&owner_a, &capture_config);
    int16_t captured[2]                = {0};
    if (capture == TABOS_AUDIO_STREAM_INVALID ||
        audio_service_read(&owner_a, capture, captured, sizeof(captured)) != -TABOS_EAGAIN) {
        return fail("empty capture did not return EAGAIN");
    }
    const int16_t stereo_input[] = {1000, 3000, -4000, 2000};
    test_platform_audio_capture(stereo_input, 2U, 2U);
    ready = 0U;
    if (audio_service_poll(&owner_a, capture, TABOS_WAIT_READABLE, &ready) != 0 || ready != TABOS_WAIT_READABLE ||
        audio_service_read(&owner_a, capture, captured, sizeof(captured)) != (int) sizeof(captured) ||
        captured[0] != 2000 || captured[1] != -1000) {
        return fail("capture conversion or readable readiness mismatch");
    }

    int16_t overflow_input[512 * 2U] = {0};
    for (size_t iteration = 0U; iteration < 40U; ++iteration) {
        test_platform_audio_capture(overflow_input, 512U, 2U);
    }
    if (audio_service_get_status(&owner_a, capture, &status) != 0 || status.overruns == 0U ||
        status.buffered_bytes != status.buffer_capacity) {
        return fail("capture overflow accounting mismatch");
    }

    const tabos_audio_config_t multichannel_config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 3U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_stream_t multichannel = audio_service_open(&owner_a, &multichannel_config);
    const int16_t four_channel_input[]      = {100, 200, 300, 400};
    int16_t three_channel_output[3]         = {0};
    test_platform_audio_capture(four_channel_input, 1U, 4U);
    if (multichannel == TABOS_AUDIO_STREAM_INVALID ||
        audio_service_read(&owner_a, multichannel, three_channel_output, sizeof(three_channel_output)) !=
            (int) sizeof(three_channel_output) ||
        three_channel_output[0] != 100 || three_channel_output[1] != 200 || three_channel_output[2] != 300) {
        return fail("backend-reported multichannel capture mismatch");
    }

    test_platform_audio_error(TABOS_EIO);
    ready = 0U;
    if (audio_service_poll(&owner_a, capture, TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP, &ready) != 0 ||
        ready != (TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP) ||
        audio_service_read(&owner_a, capture, captured, sizeof(captured)) != -TABOS_EIO ||
        audio_service_write(&owner_a, playback[0], &sample, sizeof(sample)) != -TABOS_EIO ||
        audio_service_info(NULL, NULL, &error) == false || error != TABOS_EIO) {
        return fail("backend fault did not propagate to streams and waits");
    }

    audio_service_close_owner(&owner_a);
    if (audio_service_get_status(&owner_a, capture, &status) != -TABOS_EBADF ||
        audio_service_close(&owner_a, playback[1]) != -TABOS_EBADF) {
        return fail("owner cleanup did not stale audio handles");
    }
    audio_service_shutdown();
    return 0;
}
