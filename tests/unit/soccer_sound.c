#include <soccer/sound.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static uint64_t now_ms;

uint64_t tabos_monotonic_ms(void)
{
    return now_ms;
}

static int opens;
static int closes;
static int flushes;
static int writes;
static int invalid_write;
static bool fail_open;
static bool fail_write;
static bool zero_write;
static bool fail_flush;
static uint32_t partial_limit = TABOS_AUDIO_IO_MAX;
static uint32_t used;
static int16_t pcm[TABOS_AUDIO_IO_MAX / 2U];

tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config)
{
    ++opens;
    assert(config->channels == 1U && config->sample_rate == 44100U);
    assert(config->direction == TABOS_AUDIO_PLAYBACK && config->route == TABOS_AUDIO_ROUTE_SPEAKER);
    errno = ENODEV;
    return fail_open ? TABOS_AUDIO_STREAM_INVALID : 42;
}

int tabos_audio_close(tabos_audio_stream_t stream)
{
    assert(stream == 42);
    ++closes;
    return 0;
}

int tabos_audio_flush(tabos_audio_stream_t stream)
{
    assert(stream == 42);
    ++flushes;
    used  = 0U;
    errno = EIO;
    return fail_flush ? -1 : 0;
}

int tabos_audio_write(tabos_audio_stream_t stream, const void* bytes, uint32_t count)
{
    assert(stream == 42 && count <= TABOS_AUDIO_IO_MAX && count % 2U == 0U);
    ++writes;
    if (invalid_write != 0) {
        return invalid_write == 1 ? (int) count + 2 : 1;
    }
    if (zero_write) {
        return 0;
    }
    if (fail_write) {
        errno = EAGAIN;
        return -1;
    }
    if (count > partial_limit) {
        count = partial_limit;
    }
    assert(used + count <= sizeof(pcm));
    memcpy((uint8_t*) pcm + used, bytes, count);
    used += count;
    return (int) count;
}

static void effect_timing(void)
{
    soccer_sound_t sound = {.stream = TABOS_AUDIO_STREAM_INVALID};
    soccer_sound_prepare(&sound);
    now_ms = 1000U;
    soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
    int prior_flushes  = flushes;
    int prior_writes   = writes;
    now_ms            += 16U;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
    assert(flushes == prior_flushes && writes == prior_writes);
    soccer_sound_play(&sound, SOCCER_EVENT_SAVE);
    assert(flushes == prior_flushes + 1);
    prior_flushes  = flushes;
    now_ms        += 79U;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes);
    ++now_ms;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes + 1);
    soccer_sound_play(&sound, SOCCER_EVENT_POST);
    soccer_sound_play(&sound, SOCCER_EVENT_GOAL);
    soccer_sound_play(&sound, SOCCER_EVENT_WHISTLE);
    soccer_sound_play(&sound, SOCCER_EVENT_FINISH);
    assert(flushes == prior_flushes + 5);
    prior_flushes  = flushes;
    now_ms        += 159U;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes);
    ++now_ms;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes + 1);
    soccer_sound_play(&sound, SOCCER_EVENT_GOAL);
    soccer_sound_stop(&sound);
    soccer_sound_prepare(&sound);
    prior_flushes = flushes;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes + 1); /* Pause/reset discards the old priority. */
    soccer_sound_toggle(&sound);
    soccer_sound_toggle(&sound);
    soccer_sound_prepare(&sound);
    prior_flushes = flushes;
    soccer_sound_play(&sound, SOCCER_EVENT_PASS);
    assert(flushes == prior_flushes + 1);
    soccer_sound_close(&sound);
}

int main(void)
{
    soccer_sound_t sound = {.stream = TABOS_AUDIO_STREAM_INVALID};
    uint32_t hashes[10]  = {0};
    soccer_sound_play(&sound, SOCCER_EVENT_NONE);
    soccer_sound_play(&sound, (soccer_event_t) -1);
    soccer_sound_play(&sound, (soccer_event_t) 99);
    assert(opens == 0);
    soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
    assert(opens == 0 && writes == 0); /* Playing cannot start the codec. */
    soccer_sound_prepare(&sound);
    soccer_sound_prepare(&sound);
    assert(opens == 1);
    for (unsigned int effect = 1U; effect <= SOCCER_EVENT_FINISH; ++effect) {
        partial_limit = 2048U; /* Simulate short nonblocking writes. */
        soccer_sound_play(&sound, (soccer_event_t) effect);
        static const uint32_t lengths[] = {0U, 2U, 2U, 2U, 2U, 2U, 2U, 4U, 3U, 4U};
        assert(used == lengths[effect] * 3528U);
        assert(pcm[0] == 0 && pcm[used / 2U - 1U] == 0);
        uint32_t hash = 2166136261U;
        bool audible  = false;
        for (unsigned int i = 0U; i < used / 2U; ++i) {
            assert(pcm[i] >= -4096 && pcm[i] <= 4096);
            audible = audible || pcm[i] != 0;
            hash    = (hash ^ (uint16_t) pcm[i]) * 16777619U;
        }
        assert(audible);
        for (unsigned int i = 1U; i < effect; ++i) {
            assert(hash != hashes[i]);
        }
        hashes[effect] = hash;
        if (effect == SOCCER_EVENT_WHISTLE || effect == SOCCER_EVENT_FINISH) {
            for (unsigned int i = 1764U; i < 3528U; ++i) {
                assert(pcm[i] == 0); /* The gap between whistle notes is silent. */
            }
        }
    }
    assert(opens == 1 && flushes == 9 && closes == 0);
    soccer_sound_stop(&sound);
    assert(sound.stream == TABOS_AUDIO_STREAM_INVALID && closes == 1);
    soccer_sound_toggle(&sound);
    soccer_sound_prepare(&sound);
    soccer_sound_play(&sound, SOCCER_EVENT_WHISTLE);
    assert(sound.muted && opens == 1);
    soccer_sound_toggle(&sound);
    fail_open = true;
    soccer_sound_prepare(&sound);
    assert(sound.stream == TABOS_AUDIO_STREAM_INVALID && sound.error == ENODEV);
    const int failed_opens = opens;
    soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
    assert(opens == failed_opens);
    fail_open = false;
    soccer_sound_prepare(&sound);
    assert(sound.error == 0 && sound.stream == 42);
    /* Playback failures neither close/reopen the device nor retry every game frame. */
    for (unsigned int failure = 0U; failure < 6U; ++failure) {
        fail_write    = failure == 0U;
        zero_write    = failure == 1U;
        fail_flush    = failure == 2U;
        invalid_write = failure == 3U ? 1 : 0;
        if (failure == 4U) {
            invalid_write = 2;
        }
        partial_limit          = failure == 5U ? 128U : 2048U;
        const int prior_closes = closes, prior_opens = opens, prior_writes = writes;
        soccer_sound_play(&sound, SOCCER_EVENT_GOAL);
        assert(sound.error != 0 && sound.stream == 42);
        assert(closes == prior_closes && opens == prior_opens && writes - prior_writes <= 8);
        const int stopped_writes = writes, stopped_flushes = flushes;
        soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
        assert(writes == stopped_writes && flushes == stopped_flushes);
        fail_write = zero_write = fail_flush = false;
        invalid_write                        = 0;
        partial_limit                        = 2048U;
        soccer_sound_prepare(&sound);
        assert(sound.error == 0 && closes == prior_closes + 1 && opens == prior_opens + 1);
        soccer_sound_play(&sound, SOCCER_EVENT_SHOT);
        assert(sound.error == 0 && used == 7056U);
    }
    soccer_sound_close(&sound);
    const int final_closes = closes;
    soccer_sound_close(&sound);
    assert(closes == final_closes);
    effect_timing();
    puts("Soccer sound tests passed");
    return 0;
}
