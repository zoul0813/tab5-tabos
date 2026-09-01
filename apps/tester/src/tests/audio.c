#include <tester/test.h>

#include <tabos/audio.h>
#include <tabos/device.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stdint.h>

void tester_test_audio(tester_context_t* context)
{
    tabos_device_info_t device;
    const bool device_ok = tabos_device_find(TABOS_DEVICE_NAME_AUDIO, &device) == 0 &&
                           device.device_class == TABOS_DEVICE_CLASS_AUDIO && device.state == TABOS_DEVICE_READY &&
                           (device.features & TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK) != 0U &&
                           (device.features & TABOS_DEVICE_FEATURE_AUDIO_CAPTURE) != 0U;
    tester_expect(context, device_ok, "audio0 reports ready playback and capture");

    tabos_audio_info_t info;
    const bool info_ok = tabos_audio_get_info(&info) == 0 && info.capture_channels > 0U &&
                         info.sample_rates == TABOS_AUDIO_RATES_ALL &&
                         info.default_sample_rate == TABOS_AUDIO_DEFAULT_SAMPLE_RATE &&
                         (info.features & (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE)) ==
                             (TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE) &&
                         (info.routes & (TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_MICROPHONE)) ==
                             (TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_MICROPHONE);
    tester_expect(context, info_ok, "audio format capabilities and routes are available");
    if (!info_ok) {
        return;
    }

    const tabos_audio_config_t playback_config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 2U,
        .route     = TABOS_AUDIO_ROUTE_SPEAKER,
    };
    const tabos_audio_stream_t playback       = tabos_audio_open(&playback_config);
    const tabos_wait_source_t playback_source = tabos_audio_wait_source(playback);
    tabos_wait_item_t playback_wait           = {
                  .source = playback_source,
                  .events = TABOS_WAIT_WRITABLE,
    };
    int16_t silence[2] = {0};
    tabos_audio_status_t status;
    tester_expect(
        context,
        playback != TABOS_AUDIO_STREAM_INVALID && playback_source != TABOS_WAIT_SOURCE_INVALID &&
            tabos_wait(&playback_wait, 1U, 0U) == 1 && (playback_wait.returned_events & TABOS_WAIT_WRITABLE) != 0U &&
            tabos_audio_write(playback, silence, sizeof(silence)) == (int) sizeof(silence) &&
            tabos_audio_flush(playback) == 0 && tabos_audio_set_volume(playback, TABOS_AUDIO_VOLUME_MAX / 2U) == 0 &&
            tabos_audio_set_route(playback, TABOS_AUDIO_ROUTE_SPEAKER) == 0 &&
            tabos_audio_get_status(playback, &status) == 0 && status.buffer_capacity > 0U,
        "playback open, wait, write, flush, volume, route, and status work");
    tester_expect(context, tabos_audio_close(playback) == 0 && tabos_audio_close(playback) < 0 && errno == EBADF,
                  "playback close stales stream handle");

    const tabos_audio_config_t capture_config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_stream_t capture       = tabos_audio_open(&capture_config);
    const tabos_wait_source_t capture_source = tabos_audio_wait_source(capture);
    int16_t captured                         = 0;
    errno                                    = 0;
    const int empty_read                     = tabos_audio_read(capture, &captured, sizeof(captured));
    tester_expect(context,
                  capture != TABOS_AUDIO_STREAM_INVALID && capture_source != TABOS_WAIT_SOURCE_INVALID &&
                      (empty_read >= 0 || (empty_read < 0 && errno == EAGAIN)),
                  "capture opens with wait source and nonblocking read semantics");
    tester_expect(context, tabos_audio_close(capture) == 0, "capture closes cleanly");
}
