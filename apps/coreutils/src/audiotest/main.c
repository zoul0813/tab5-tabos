#include <tabos/audio.h>
#include <tabos/runtime_time.h>
#include <tabos/wait.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    AUDIO_BLOCK_FRAMES = 480,
};

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: audiotest [info|tone [speaker|headphone]|level|loopback [speaker|headphone]|route "
                    "speaker|headphone|buffers]\n");
}

static uint32_t route_from_name(const char* name)
{
    if (name == NULL || strcmp(name, "speaker") == 0) {
        return TABOS_AUDIO_ROUTE_SPEAKER;
    }
    if (strcmp(name, "headphone") == 0) {
        return TABOS_AUDIO_ROUTE_HEADPHONE;
    }
    return 0U;
}

static int show_info(void)
{
    tabos_audio_info_t info;
    if (tabos_audio_get_info(&info) != 0) {
        fprintf(stderr, "audiotest: audio unavailable: %s\n", strerror(errno));
        return 1;
    }
    printf("Format: signed 16-bit little-endian PCM, %u Hz\n", TABOS_AUDIO_SAMPLE_RATE);
    printf("Playback: %s\n", (info.features & TABOS_AUDIO_FEATURE_PLAYBACK) != 0U ? "yes" : "no");
    printf("Capture: %s (%lu channels)\n", (info.features & TABOS_AUDIO_FEATURE_CAPTURE) != 0U ? "yes" : "no",
           (unsigned long) info.capture_channels);
    printf("Routes:%s%s%s\n", (info.routes & TABOS_AUDIO_ROUTE_SPEAKER) != 0U ? " speaker" : "",
           (info.routes & TABOS_AUDIO_ROUTE_HEADPHONE) != 0U ? " headphone" : "",
           (info.routes & TABOS_AUDIO_ROUTE_MICROPHONE) != 0U ? " microphone" : "");
    printf("AEC: %s\n", (info.features & TABOS_AUDIO_FEATURE_AEC) != 0U ? "yes" : "no");
    return 0;
}

static int wait_for(tabos_audio_stream_t stream, uint32_t events, uint32_t timeout_ms)
{
    tabos_wait_item_t item = {
        .source = tabos_audio_wait_source(stream),
        .events = events | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP,
    };
    if (item.source == TABOS_WAIT_SOURCE_INVALID || tabos_wait(&item, 1U, timeout_ms) <= 0) {
        return -1;
    }
    return (item.returned_events & (TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP)) == 0U &&
                   (item.returned_events & events) != 0U ?
               0 :
               -1;
}

static int write_all(tabos_audio_stream_t stream, const int16_t* samples, uint32_t bytes)
{
    const uint8_t* data = (const uint8_t*) samples;
    uint32_t written    = 0U;
    while (written < bytes) {
        const int result = tabos_audio_write(stream, data + written, bytes - written);
        if (result > 0) {
            written += (uint32_t) result;
        } else if (errno == EAGAIN && wait_for(stream, TABOS_WAIT_WRITABLE, 1000U) == 0) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

static int tone(uint32_t route)
{
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = route,
    };
    const tabos_audio_stream_t stream = tabos_audio_open(&config);
    if (stream == TABOS_AUDIO_STREAM_INVALID) {
        fprintf(stderr, "audiotest: cannot open playback: %s\n", strerror(errno));
        return 1;
    }
    int16_t samples[AUDIO_BLOCK_FRAMES];
    uint32_t phase = 0U;
    for (unsigned int block = 0U; block < 200U; ++block) {
        for (size_t index = 0U; index < AUDIO_BLOCK_FRAMES; ++index) {
            phase += 440U;
            if (phase >= TABOS_AUDIO_SAMPLE_RATE) {
                phase -= TABOS_AUDIO_SAMPLE_RATE;
            }
            samples[index] = phase < TABOS_AUDIO_SAMPLE_RATE / 2U ? 6000 : -6000;
        }
        if (write_all(stream, samples, sizeof(samples)) != 0) {
            fprintf(stderr, "audiotest: playback failed: %s\n", strerror(errno));
            (void) tabos_audio_close(stream);
            return 1;
        }
    }
    (void) tabos_sleep_ms(100U);
    tabos_audio_status_t status;
    if (tabos_audio_get_status(stream, &status) == 0) {
        printf("Tone complete; underruns=%lu\n", (unsigned long) status.underruns);
    }
    return tabos_audio_close(stream) == 0 ? 0 : 1;
}

static int microphone_level(void)
{
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_stream_t stream = tabos_audio_open(&config);
    if (stream == TABOS_AUDIO_STREAM_INVALID) {
        fprintf(stderr, "audiotest: cannot open microphone: %s\n", strerror(errno));
        return 1;
    }
    int16_t samples[AUDIO_BLOCK_FRAMES];
    puts("Microphone level (2 seconds):");
    for (unsigned int block = 0U; block < 20U; ++block) {
        if (wait_for(stream, TABOS_WAIT_READABLE, 500U) != 0) {
            fprintf(stderr, "audiotest: capture timeout\n");
            (void) tabos_audio_close(stream);
            return 1;
        }
        const int bytes = tabos_audio_read(stream, samples, sizeof(samples));
        if (bytes < 0) {
            fprintf(stderr, "audiotest: capture failed: %s\n", strerror(errno));
            (void) tabos_audio_close(stream);
            return 1;
        }
        int peak = 0;
        for (int index = 0; index < bytes / (int) sizeof(int16_t); ++index) {
            const int magnitude = samples[index] < 0 ? -samples[index] : samples[index];
            if (magnitude > peak) {
                peak = magnitude;
            }
        }
        printf("%5d%s", peak, block % 5U == 4U ? "\n" : " ");
    }
    return tabos_audio_close(stream) == 0 ? 0 : 1;
}

static int loopback(uint32_t route)
{
    const tabos_audio_config_t input_config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_config_t output_config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = route,
    };
    const tabos_audio_stream_t input  = tabos_audio_open(&input_config);
    const tabos_audio_stream_t output = tabos_audio_open(&output_config);
    if (input == TABOS_AUDIO_STREAM_INVALID || output == TABOS_AUDIO_STREAM_INVALID) {
        fprintf(stderr, "audiotest: cannot open loopback streams: %s\n", strerror(errno));
        if (input != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(input);
        }
        if (output != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(output);
        }
        return 1;
    }
    (void) tabos_audio_set_volume(output, 500U);
    int16_t samples[AUDIO_BLOCK_FRAMES];
    puts("Loopback active for 5 seconds.");
    bool ok = true;
    for (unsigned int block = 0U; block < 500U; ++block) {
        if (wait_for(input, TABOS_WAIT_READABLE, 500U) != 0) {
            ok = false;
            break;
        }
        const int bytes = tabos_audio_read(input, samples, sizeof(samples));
        if (bytes > 0 && write_all(output, samples, (uint32_t) bytes) != 0) {
            ok = false;
            break;
        }
        if (bytes < 0 && errno != EAGAIN) {
            ok = false;
            break;
        }
    }
    const int input_result  = tabos_audio_close(input);
    const int output_result = tabos_audio_close(output);
    return ok && input_result == 0 && output_result == 0 ? 0 : 1;
}

static int route_test(uint32_t route)
{
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = route,
    };
    const tabos_audio_stream_t stream = tabos_audio_open(&config);
    if (stream == TABOS_AUDIO_STREAM_INVALID || tabos_audio_set_route(stream, route) != 0) {
        fprintf(stderr, "audiotest: route unavailable: %s\n", strerror(errno));
        if (stream != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(stream);
        }
        return 1;
    }
    printf("Route selected: %s\n", route == TABOS_AUDIO_ROUTE_SPEAKER ? "speaker" : "headphone");
    return tabos_audio_close(stream) == 0 ? 0 : 1;
}

static int buffer_test(void)
{
    const tabos_audio_config_t playback_config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_SPEAKER,
    };
    const tabos_audio_config_t capture_config = {
        .direction = TABOS_AUDIO_CAPTURE,
        .channels  = 1U,
        .route     = TABOS_AUDIO_ROUTE_MICROPHONE,
    };
    const tabos_audio_stream_t playback = tabos_audio_open(&playback_config);
    const tabos_audio_stream_t capture  = tabos_audio_open(&capture_config);
    if (playback == TABOS_AUDIO_STREAM_INVALID || capture == TABOS_AUDIO_STREAM_INVALID) {
        fprintf(stderr, "audiotest: cannot open buffer-test streams\n");
        if (playback != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(playback);
        }
        if (capture != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(capture);
        }
        return 1;
    }
    (void) tabos_sleep_ms(750U);
    tabos_audio_status_t playback_status;
    tabos_audio_status_t capture_status;
    const bool ok = tabos_audio_get_status(playback, &playback_status) == 0 &&
                    tabos_audio_get_status(capture, &capture_status) == 0;
    if (ok) {
        printf("Playback underruns: %lu\nCapture overruns: %lu\n", (unsigned long) playback_status.underruns,
               (unsigned long) capture_status.overruns);
    }
    (void) tabos_audio_close(playback);
    (void) tabos_audio_close(capture);
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "info") == 0)) {
        return show_info();
    }
    if (argc >= 2 && argc <= 3 && (strcmp(argv[1], "tone") == 0 || strcmp(argv[1], "loopback") == 0)) {
        const uint32_t route = route_from_name(argc == 3 ? argv[2] : NULL);
        if (route == 0U) {
            usage(stderr);
            return 1;
        }
        return strcmp(argv[1], "tone") == 0 ? tone(route) : loopback(route);
    }
    if (argc == 2 && strcmp(argv[1], "level") == 0) {
        return microphone_level();
    }
    if (argc == 2 && strcmp(argv[1], "buffers") == 0) {
        return buffer_test();
    }
    if (argc == 3 && strcmp(argv[1], "route") == 0) {
        const uint32_t route = route_from_name(argv[2]);
        return route != 0U ? route_test(route) : (usage(stderr), 1);
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    usage(stderr);
    return 1;
}
