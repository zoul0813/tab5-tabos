#include <pool/sound.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static unsigned int opens, closes, writes;
static int write_mode;
static bool fail_open, fail_flush, fail_close;
static uint32_t used, limit = 2048U;
static int16_t pcm[8192];
tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config)
{
    ++opens;
    assert(config->channels == 1U && config->sample_rate == 44100U);
    assert(config->direction == TABOS_AUDIO_PLAYBACK && config->route == TABOS_AUDIO_ROUTE_SPEAKER);
    if (fail_open) {
        errno = EIO;
        return TABOS_AUDIO_STREAM_INVALID;
    }
    return 42;
}
int tabos_audio_close(tabos_audio_stream_t stream)
{
    assert(stream == 42);
    ++closes;
    if (fail_close) {
        errno = EIO;
        return -1;
    }
    return 0;
}
int tabos_audio_flush(tabos_audio_stream_t stream)
{
    assert(stream == 42);
    used   = 0U;
    writes = 0U;
    if (fail_flush) {
        errno = EIO;
        return -1;
    }
    return 0;
}
int tabos_audio_write(tabos_audio_stream_t stream, const void* bytes, uint32_t count)
{
    assert(stream == 42 && count <= TABOS_AUDIO_IO_MAX && count % 2U == 0U);
    ++writes;
    if (write_mode == 1) {
        errno = EAGAIN;
        return -1;
    }
    if (write_mode == 2) {
        return 0;
    }
    if (write_mode == 3) {
        return 1;
    }
    if (write_mode == 4) {
        return (int) count + 2;
    }
    if (count > limit) {
        count = limit;
    }
    assert(used + count <= sizeof(pcm));
    memcpy((uint8_t*) pcm + used, bytes, count);
    used += count;
    return (int) count;
}
int main(void)
{
    pool_sound_t sound = {.stream = TABOS_AUDIO_STREAM_INVALID};
    pool_sound_play(&sound, POOL_SOUND_CUE);
    assert(opens == 0U && writes == 0U); /* A shot never opens a device. */
    pool_sound_prepare(&sound);
    pool_sound_prepare(&sound);
    /* Recorded from the previous runtime synthesizer, before this fix. */
    static const uint32_t expected[] = {0U, 194204047U, 3306698041U, 882690259U, 405606100U, 2448231417U, 2384085000U};
    uint32_t hashes[7]               = {0};
    for (unsigned int effect = 1U; effect <= 6U; ++effect) {
        pool_sound_play(&sound, (pool_sound_effect_t) effect);
        assert(sound.stream == 42 && writes <= 8U && used > 0U);
        assert(pcm[0] == 0 && pcm[used / 2U - 1U] == 0);
        bool audible  = false;
        uint32_t hash = 2166136261U;
        for (unsigned int i = 0U; i < used / 2U; ++i) {
            assert(pcm[i] >= -4096 && pcm[i] <= 4096);
            audible = audible || pcm[i] != 0;
            hash    = (hash ^ (uint16_t) pcm[i]) * 16777619U;
        }
        assert(audible && hash == expected[effect]);
        for (unsigned int i = 1U; i < effect; ++i) {
            assert(hash != hashes[i]);
        }
        hashes[effect] = hash;
    }
    assert(opens == 1U && closes == 0U);
    /* Multiple effects leave playback running: no expiry can cancel queued PCM. */
    assert(sound.stream == 42);
    pool_sound_play(&sound, POOL_SOUND_NONE);
    assert(opens == 1U && closes == 0U);
    pool_sound_close(&sound);
    pool_sound_close(&sound);
    assert(closes == 1U);
    fail_open = true;
    pool_sound_prepare(&sound);
    assert(sound.stream == TABOS_AUDIO_STREAM_INVALID && sound.error == EIO);
    const unsigned int failed_opens = opens;
    pool_sound_play(&sound, POOL_SOUND_CUE);
    assert(opens == failed_opens); /* No automatic retry on a later shot. */
    fail_open = false;
    pool_sound_prepare(&sound);
    fail_flush = true;
    pool_sound_play(&sound, POOL_SOUND_CUE);
    assert(sound.stream == 42 && sound.error == EIO && closes == 1U);
    fail_flush = false;
    pool_sound_close(&sound);
    for (write_mode = 1; write_mode <= 4; ++write_mode) {
        pool_sound_prepare(&sound);
        const unsigned int before_close = closes;
        pool_sound_play(&sound, POOL_SOUND_POT);
        assert(sound.stream == 42 && sound.error != 0 && writes == 1U && closes == before_close);
        pool_sound_play(&sound, POOL_SOUND_CUE);
        assert(writes == 1U); /* A failed effect disables further IO until an explicit retry. */
        pool_sound_close(&sound);
    }
    write_mode = 0;
    limit      = 2U;
    pool_sound_prepare(&sound);
    pool_sound_play(&sound, POOL_SOUND_RESULT);
    assert(sound.stream == 42 && sound.error == EAGAIN && writes == 8U);
    fail_close = true;
    pool_sound_close(&sound);
    assert(sound.stream == TABOS_AUDIO_STREAM_INVALID);
    fail_close = false;

    pool_game_t a = {0}, b = {0};
    b.shot_set = true;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_CUE);
    a                = b;
    b.contacts.first = 1U;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_BALL);
    a                     = b;
    b.contacts.rail_after = true;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_RAIL);
    a                 = b;
    b.shot_pots.count = 1U;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_POT);
    a            = b;
    b.shot_set   = false;
    b.rules.foul = POOL_SCRATCH;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_FOUL);
    b.rules.complete = true;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_RESULT);
    a = b;
    assert(pool_sound_event(&a, &b) == POOL_SOUND_NONE);
    puts("Pool optional sound, bounded IO, prepared stream reuse and event priority tests passed");
    return 0;
}
