/*
 * O2EM-NG SDL3 audio backend
 *
 * Sound generation is based on the original O2EM audio.c logic.
 * Allegro AUDIOSTREAM handling has been replaced with SDL3 audio streams.
 */

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpu.h"
#include "types.h"
#include "config.h"
#include "vmachine.h"
#include "audio.h"

#define SAMPLE_RATE 44100
#define PERIOD1 11
#define PERIOD2 44
#define SOUND_BUFFER_LEN 1056

#define AUD_CTRL 0xAA
#define AUD_D0   0xA7
#define AUD_D1   0xA8
#define AUD_D2   0xA9

int sound_IRQ = 0;
FILE* sndlog = nullptr;

static SDL_AudioStream* gAudioStream = nullptr;

static double flt_a = 0.0;
static double flt_b = 0.0;
static unsigned char flt_prv = 0;

static void filter_audio(unsigned char* buffer, unsigned long len)
{
    static unsigned char temp[SOUND_BUFFER_LEN];

    if (len > SOUND_BUFFER_LEN)
        return;

    std::memcpy(temp, buffer, len);

    for (unsigned long i = 0; i < len; ++i)
    {
        int t = (i == 0)
            ? (temp[0] - flt_prv)
            : (temp[i] - temp[i - 1]);

        if (t)
            flt_b = static_cast<double>(t);

        flt_a += flt_b / 4.0 - flt_a / 80.0;
        flt_b -= flt_b / 4.0;

        if (flt_a > 255.0 || flt_a < -255.0)
            flt_a = 0.0;

        buffer[i] =
            static_cast<unsigned char>((flt_a + 255.0) / 2.0);
    }

    flt_prv = temp[len - 1];
}

static void audio_process(unsigned char* buffer)
{
    unsigned long aud_data =
        VDCwrite[AUD_D2] |
        (VDCwrite[AUD_D1] << 8) |
        (VDCwrite[AUD_D0] << 16);

    int intena = VDCwrite[0xA0] & 0x04;

    int pnt = 0;
    int cnt = 0;

    int noise = VDCwrite[AUD_CTRL] & 0x10;
    int enabled = VDCwrite[AUD_CTRL] & 0x80;

    int rndbit =
        (enabled && noise) ? (std::rand() % 2) : 0;

    while (pnt < SOUND_BUFFER_LEN)
    {
        int pos =
            tweakedaudio ? (pnt / 3) : (MAXLINES - 1);

        int volume = AudioVector[pos] & 0x0F;
        enabled = AudioVector[pos] & 0x80;

        int period =
            (AudioVector[pos] & 0x20) ? PERIOD1 : PERIOD2;

        int re_circ = AudioVector[pos] & 0x40;

        buffer[pnt++] =
            enabled
            ? static_cast<unsigned char>(
                ((aud_data & 0x01) ^ rndbit) *
                (0x10 * volume))
            : 0;

        ++cnt;

        if (cnt >= period)
        {
            cnt = 0;

            aud_data = re_circ
                ? ((aud_data >> 1) |
                   ((aud_data & 1) << 23))
                : (aud_data >> 1);

            rndbit =
                (enabled && noise) ? (std::rand() % 2) : 0;

            if (enabled && intena && !sound_IRQ)
            {
                sound_IRQ = 1;
                ext_IRQ();
            }
        }
    }

    if (app_data.filter)
        filter_audio(buffer, SOUND_BUFFER_LEN);
}

void init_sound_stream(void)
{
    if (!app_data.sound_en)
        return;

    if (gAudioStream)
        return;

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_U8;
    spec.channels = 1;
    spec.freq = SAMPLE_RATE;

    gAudioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        nullptr,
        nullptr);

    if (!gAudioStream)
    {
        std::printf(
            "SDL audio stream creation failed: %s\n",
            SDL_GetError());

        app_data.sound_en = 0;
        return;
    }

    if (!SDL_ResumeAudioStreamDevice(gAudioStream))
    {
        std::printf(
            "SDL audio resume failed: %s\n",
            SDL_GetError());

        SDL_DestroyAudioStream(gAudioStream);
        gAudioStream = nullptr;
        app_data.sound_en = 0;
        return;
    }

    flt_a = 0.0;
    flt_b = 0.0;
    flt_prv = 0;

    std::printf("SDL3 audio initialized: 44100 Hz, mono, 8-bit\n");
}

void init_audio(void)
{
    sound_IRQ = 0;
    sndlog = nullptr;

    if (app_data.sound_en)
        init_sound_stream();
}

void update_audio(void)
{
    if (!app_data.sound_en || !gAudioStream)
        return;

    // Keep a small amount of audio queued.
    // This avoids generating data faster than the device consumes it.
    const int queued = SDL_GetAudioStreamQueued(gAudioStream);

    if (queued < 0)
        return;

    if (queued > SOUND_BUFFER_LEN * 4)
        return;

    unsigned char buffer[SOUND_BUFFER_LEN];

    audio_process(buffer);

    if (!SDL_PutAudioStreamData(
            gAudioStream,
            buffer,
            SOUND_BUFFER_LEN))
    {
        std::printf(
            "SDL_PutAudioStreamData failed: %s\n",
            SDL_GetError());
    }

    if (sndlog)
        std::fwrite(buffer, 1, SOUND_BUFFER_LEN, sndlog);
}

void mute_audio(void)
{
    if (!gAudioStream)
        return;

    SDL_ClearAudioStream(gAudioStream);
    SDL_PauseAudioStreamDevice(gAudioStream);
}

void close_audio(void)
{
    if (gAudioStream)
    {
        SDL_DestroyAudioStream(gAudioStream);
        gAudioStream = nullptr;
    }

    if (sndlog)
    {
        std::fclose(sndlog);
        sndlog = nullptr;
    }

    app_data.sound_en = 0;
}
