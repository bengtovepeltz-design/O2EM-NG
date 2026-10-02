/*
 * O2EM-NG SDL3 audio backend
 *
 * Patch 0026H: MAME-style sound-register write/pending handling.
 *
 * Based on the emulator-timed audio pipeline from Patch 0026A.
 *
 * The emulation core now produces audio once per emulated video frame and
 * stores it in a ring buffer. SDL only drains already-generated samples.
 * The U8 format, corrected silence level, averaged high-frequency volume,
 * filter, noise generator, recirculation and IRQ behaviour are retained.
 * The HBLANK-driven 4/16-line shift clock from Patch 0026G is retained.
 * Writes to A7-A9 now set a write latch. At HBLANK, a pending shift waits
 * until the write latch has been consumed, matching MAME's sequencing and
 * preventing a newly loaded 24-bit pattern from shifting immediately.
 */

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cpu.h"
#include "src/mcs48/legacy_observation.h"
#include "types.h"
#include "config.h"
#include "vmachine.h"
#include "audio.h"
#include "audio_config.h"
#if O2EM_USE_AUDIO8245
#include "audio8245.h"
#endif

#define SAMPLE_RATE 44100
#define SOUND_BUFFER_LEN 1056

static constexpr unsigned char kU8Silence = 0x80;
// MAME generates the 16-step volume PWM at the 8245 device clock and routes
// the device at 0.25 gain. At 44.1 kHz, model the audible average directly.
// 16 U8 steps is about 12.5% of the available unsigned 8-bit range.
static constexpr int kMaxOutputAmplitude = 16;

#define AUD_CTRL 0xAA
#define AUD_D0   0xA7
#define AUD_D1   0xA8
#define AUD_D2   0xA9

int sound_IRQ = 0;
FILE* sndlog = nullptr;

static SDL_AudioStream* gAudioStream = nullptr;

static double flt_a = 0.0;
static double flt_b = 0.0;
static unsigned char flt_prv = kU8Silence;

static std::uint32_t gSoundShiftRegister = 0;
static int gSoundHBlankPrescaler = 0;
static bool gSoundWritten = false;
static bool gSoundShiftPending = false;
static int gSoundShiftCount = 0;
static int gNoiseOutputBit = 0;
static std::uint16_t gNoiseLfsr = 0xACE1u;

#if O2EM_USE_AUDIO8245
static Audio8245 gAudio8245;
#endif

// About 1.5 seconds at 44.1 kHz. This is intentionally much larger than the
// normal queue target so temporary frontend stalls do not alter emulated time.
static constexpr std::size_t kRingCapacity = 65536;
static std::array<unsigned char, kRingCapacity> gRing{};
static std::size_t gRingRead = 0;
static std::size_t gRingWrite = 0;
static std::size_t gRingCount = 0;
static std::uint64_t gFrameSampleAccumulator = 0;
static std::uint64_t gRingOverruns = 0;
static std::uint64_t gRingUnderruns = 0;

// Patch 0026B: Debug-only full signal walk. No sound behaviour is changed.
#ifndef NDEBUG
static FILE* gSignalWalkLog = nullptr;
static FILE* gSignalWalkRaw = nullptr;
static int gDiagFrame = 0;
static int gDiagSampleInFrame = 0;
static int gDiagDetailedSamplesRemaining = 0;
static int gDiagLastControl = -1;
static int gDiagLastEnabled = -1;
static std::uint64_t gDiagProducedTotal = 0;
static std::uint64_t gDiagPoppedTotal = 0;
static std::uint64_t gDiagSentTotal = 0;
static constexpr int kSignalWalkFrameLimit = 500;

struct SignalTrace
{
    int control = 0;
    int volume = 0;
    int noise = 0;
    int period = 0;
    int recirculate = 0;
    int enabled = 0;
    int outputBit = 0;
    int noiseBit = 0;
    int mixedBit = 0;
    unsigned raw = 0;
    unsigned filtered = 0;
    std::uint32_t shiftBefore = 0;
    std::uint32_t shiftAfter = 0;
    int dividerBefore = 0;
    int dividerAfter = 0;
    int shiftCountBefore = 0;
    int shiftCountAfter = 0;
    int didShift = 0;
    int irqAfter = 0;
    int filterApplied = 0;
    int filterDelta = 0;
    double filterABefore = 0.0;
    double filterAAfter = 0.0;
    double filterBBefore = 0.0;
    double filterBAfter = 0.0;
    unsigned filterPrevBefore = 0;
};

static SignalTrace gTrace;

static void signal_walk_open()
{
    if (!gSignalWalkLog)
    {
        gSignalWalkLog = std::fopen("sound_signal_walk.log", "w");
        if (gSignalWalkLog)
        {
            std::setvbuf(gSignalWalkLog, nullptr, _IOFBF, 128 * 1024);
            std::fprintf(gSignalWalkLog,
                "O2EM-NG Patch 0026H MAME-style write/pending handling + signal walk\n"
                "HBLANK 4/16 shift clock retained; A7-A9 writes defer pending shifts.\n"
                "Format: SDL_AUDIO_U8, mono, %d Hz\n\n", SAMPLE_RATE);
        }
    }
    if (!gSignalWalkRaw)
        gSignalWalkRaw = std::fopen("sound_output_u8_44100_mono.raw", "wb");
}

static void signal_walk_close()
{
    if (gSignalWalkLog)
    {
        std::fprintf(gSignalWalkLog,
            "\nFINAL produced=%llu popped=%llu sent=%llu overruns=%llu underruns=%llu ring=%zu\n",
            static_cast<unsigned long long>(gDiagProducedTotal),
            static_cast<unsigned long long>(gDiagPoppedTotal),
            static_cast<unsigned long long>(gDiagSentTotal),
            static_cast<unsigned long long>(gRingOverruns),
            static_cast<unsigned long long>(gRingUnderruns), gRingCount);
        std::fclose(gSignalWalkLog);
        gSignalWalkLog = nullptr;
    }
    if (gSignalWalkRaw)
    {
        std::fclose(gSignalWalkRaw);
        gSignalWalkRaw = nullptr;
    }
}
#endif

static int next_noise_bit()
{
    const std::uint16_t feedback = static_cast<std::uint16_t>(
        ((gNoiseLfsr >> 15) ^ (gNoiseLfsr >> 13) ^ gNoiseLfsr) & 1u);

    gNoiseLfsr = static_cast<std::uint16_t>((gNoiseLfsr << 1) | feedback);
    return static_cast<int>((gNoiseLfsr >> 15) & 1u);
}

static void ring_reset()
{
    gRing.fill(kU8Silence);
    gRingRead = 0;
    gRingWrite = 0;
    gRingCount = 0;
    gFrameSampleAccumulator = 0;
    gRingOverruns = 0;
    gRingUnderruns = 0;
}

static void ring_push(unsigned char sample)
{
    if (gRingCount == kRingCapacity)
    {
        // Keep emulated time moving. Drop the oldest sample rather than
        // letting host audio back-pressure change the 8244 state.
        gRingRead = (gRingRead + 1) % kRingCapacity;
        --gRingCount;
        ++gRingOverruns;
    }

    gRing[gRingWrite] = sample;
    gRingWrite = (gRingWrite + 1) % kRingCapacity;
    ++gRingCount;
}

static std::size_t ring_pop(unsigned char* destination, std::size_t count)
{
    const std::size_t available = std::min(count, gRingCount);
    for (std::size_t i = 0; i < available; ++i)
    {
        destination[i] = gRing[gRingRead];
        gRingRead = (gRingRead + 1) % kRingCapacity;
    }
    gRingCount -= available;
    return available;
}


void audio_vdc_control_write(unsigned char old_value, unsigned char new_value, int scanline)
{
#if O2EM_USE_AUDIO8245
    gAudio8245.WriteControl(new_value);
#endif
#ifndef NDEBUG
    signal_walk_open();
    if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit)
    {
        std::fprintf(gSignalWalkLog,
            "AA_WRITE frame=%d line=%d old=%02X new=%02X enabled=%d->%d volume=%d->%d noise=%d->%d fast=%d->%d recirc=%d->%d ring=%zu\n",
            gDiagFrame, scanline, old_value, new_value,
            (old_value >> 7) & 1, (new_value >> 7) & 1,
            old_value & 15, new_value & 15,
            (old_value >> 4) & 1, (new_value >> 4) & 1,
            (old_value >> 5) & 1, (new_value >> 5) & 1,
            (old_value >> 6) & 1, (new_value >> 6) & 1, gRingCount);
        std::fflush(gSignalWalkLog);
    }
    gDiagDetailedSamplesRemaining = 200;
#else
    (void)old_value; (void)new_value; (void)scanline;
#endif
}

void audio_vdc_write(unsigned short address, unsigned char value)
{
#ifndef NDEBUG
    signal_walk_open();
    if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit)
    {
        std::fprintf(gSignalWalkLog,
            "DATA_WRITE frame=%d sample=%d reg=%02X value=%02X before=%06X hblank_prescaler=%d shiftcount=%d\n",
            gDiagFrame, gDiagSampleInFrame, address & 0xffu, value,
            static_cast<unsigned>(gSoundShiftRegister & 0xffffffu),
            gSoundHBlankPrescaler, gSoundShiftCount);
    }
    gDiagDetailedSamplesRemaining = 200;
#endif
#if O2EM_USE_AUDIO8245
    gAudio8245.WriteRegister(address, value);
#else
    switch (address & 0xFFu)
    {
    case AUD_D0:
        gSoundShiftRegister =
            (gSoundShiftRegister & 0x00FFFFu) |
            (static_cast<std::uint32_t>(value) << 16);
        break;

    case AUD_D1:
        gSoundShiftRegister =
            (gSoundShiftRegister & 0xFF00FFu) |
            (static_cast<std::uint32_t>(value) << 8);
        break;

    case AUD_D2:
        gSoundShiftRegister =
            (gSoundShiftRegister & 0xFFFF00u) |
            static_cast<std::uint32_t>(value);
        break;

    default:
        return;
    }

    gSoundShiftRegister &= 0xFFFFFFu;

    // MAME does not immediately reset the 24-bit shift sequence on each byte.
    // It records that A7-A9 were written, then consumes the latch at HBLANK.
    // This lets the CPU finish loading the pattern before a pending shift runs.
    gSoundWritten = true;
#endif
}

static unsigned char filter_one_sample(unsigned char raw)
{
#ifndef NDEBUG
    gTrace.filterApplied = 1;
    gTrace.filterPrevBefore = flt_prv;
    gTrace.filterABefore = flt_a;
    gTrace.filterBBefore = flt_b;
#endif
    const int t = static_cast<int>(raw) - static_cast<int>(flt_prv);
#ifndef NDEBUG
    gTrace.filterDelta = t;
#endif
    if (t)
        flt_b = static_cast<double>(t);

    flt_a += flt_b / 4.0 - flt_a / 80.0;
    flt_b -= flt_b / 4.0;

    if (flt_a > 255.0 || flt_a < -255.0)
        flt_a = 0.0;

    flt_prv = raw;
    const unsigned char result = static_cast<unsigned char>((flt_a + 255.0) / 2.0);
#ifndef NDEBUG
    gTrace.filterAAfter = flt_a;
    gTrace.filterBAfter = flt_b;
    gTrace.filtered = result;
#endif
    return result;
}

static void audio_hblank_tick(int control, int scanline)
{
#if O2EM_USE_AUDIO8245
    (void)control;
    (void)scanline;
    gAudio8245.HBlank();
    if (gAudio8245.ConsumeInterrupt())
    {
        sound_IRQ = 1;
        if (VDCwrite[0xA0] & 0x04)
            { mcs48::observation::Event(13, 2, 0); ext_IRQ(); }
    }
#else
    // Clock the free-running HBLANK prescaler. A selected boundary marks a
    // shift as pending; it is not necessarily executed on the same HBLANK.
    ++gSoundHBlankPrescaler;
    const int prescalerMask = (control & 0x20) ? 0x03 : 0x0F;
    if ((gSoundHBlankPrescaler & prescalerMask) == 0)
        gSoundShiftPending = true;

    // MAME sequencing: a pending shift is executed only when no A7-A9 write
    // is waiting. If a write is pending, consume the write latch and restart
    // the 24-bit service counter. Any already-pending shift remains pending
    // and will occur at the following HBLANK.
    if (gSoundShiftPending && !gSoundWritten)
    {
        const bool enabled = (control & 0x80) != 0;
        if (!enabled)
        {
            // Preserve Patch 0026G's enabled-gate for this isolated test.
            gSoundShiftPending = false;
            return;
        }

        gSoundShiftPending = false;

        const bool noise = (control & 0x10) != 0;
        const bool recirculate = (control & 0x40) != 0;
        const std::uint32_t before = gSoundShiftRegister & 0xFFFFFFu;
        const std::uint32_t outgoing = gSoundShiftRegister & 1u;

        gSoundShiftRegister >>= 1;
        if (recirculate)
            gSoundShiftRegister |= outgoing << 23;

        gSoundShiftRegister &= 0xFFFFFFu;
        gNoiseOutputBit = noise ? next_noise_bit() : 0;

        if (++gSoundShiftCount >= 24)
        {
            gSoundShiftCount = 0;
            sound_IRQ = 1;
            if (VDCwrite[0xA0] & 0x04)
                { mcs48::observation::Event(13, 2, 0); ext_IRQ(); }
        }

#ifndef NDEBUG
        signal_walk_open();
        if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit && gDiagDetailedSamplesRemaining > 0)
        {
            std::fprintf(gSignalWalkLog,
                "HBLANK_SHIFT frame=%d line=%d ctrl=%02X prescaler=%d pending=%d written=%d shift=%06X->%06X count=%d irq=%d\n",
                gDiagFrame, scanline, control & 0xff, gSoundHBlankPrescaler,
                gSoundShiftPending ? 1 : 0, gSoundWritten ? 1 : 0,
                static_cast<unsigned>(before),
                static_cast<unsigned>(gSoundShiftRegister & 0xFFFFFFu),
                gSoundShiftCount, sound_IRQ);
        }
#endif
    }
    else if (gSoundWritten)
    {
        gSoundShiftCount = 0;
        gSoundWritten = false;

#ifndef NDEBUG
        signal_walk_open();
        if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit && gDiagDetailedSamplesRemaining > 0)
        {
            std::fprintf(gSignalWalkLog,
                "HBLANK_WRITE_LATCH frame=%d line=%d ctrl=%02X prescaler=%d pending=%d shift=%06X count_reset=1\n",
                gDiagFrame, scanline, control & 0xff, gSoundHBlankPrescaler,
                gSoundShiftPending ? 1 : 0,
                static_cast<unsigned>(gSoundShiftRegister & 0xFFFFFFu));
        }
#endif
    }
#endif
}

static unsigned char generate_one_sample(int control)
{
#ifndef NDEBUG
    gTrace = {};
    gTrace.control = control;
    gTrace.shiftBefore = gSoundShiftRegister & 0xFFFFFFu;
    gTrace.dividerBefore = gSoundHBlankPrescaler;
    gTrace.shiftCountBefore = gSoundShiftCount;
#endif
#if O2EM_USE_AUDIO8245
    const int volume = gAudio8245.Volume();
    const bool noise = gAudio8245.NoiseEnabled();
    const int period = gAudio8245.FastClock() ? 4 : 16;
    const bool recirculate = gAudio8245.Recirculate();
    const bool enabled = gAudio8245.Enabled();
    const int outputBit = static_cast<int>(gAudio8245.OutputBit());
    const int mixedBit = outputBit;
#else
    const int volume = control & 0x0F;
    const bool noise = (control & 0x10) != 0;
    const int period = (control & 0x20) ? 4 : 16;
    const bool recirculate = (control & 0x40) != 0;
    const bool enabled = (control & 0x80) != 0;
    const int outputBit = static_cast<int>(gSoundShiftRegister & 1u);
    const int mixedBit = noise ? (outputBit ^ gNoiseOutputBit) : outputBit;
#endif

    // The real 8245 applies volume as a 16-step duty cycle at its multi-MHz
    // device clock. MAME then resamples that high-frequency stream. Generating
    // the PWM directly at 44.1 kHz makes it audible, so use its average value:
    // volume 0 = 0/16, volume 15 = 15/16 of the fixed output amplitude.
    const int averagedAmplitude = (mixedBit && volume > 0)
        ? (kMaxOutputAmplitude * volume) / 15
        : 0;

    unsigned char sample = enabled
        ? static_cast<unsigned char>(kU8Silence + averagedAmplitude)
        : kU8Silence;
#ifndef NDEBUG
    gTrace.volume = volume;
    gTrace.noise = noise ? 1 : 0;
    gTrace.period = period;
    gTrace.recirculate = recirculate ? 1 : 0;
    gTrace.enabled = enabled ? 1 : 0;
    gTrace.outputBit = outputBit;
    gTrace.noiseBit = gNoiseOutputBit;
    gTrace.mixedBit = mixedBit;
    gTrace.raw = sample;
    gTrace.filtered = sample;
#endif

    // Patch 0026G: the sound register is advanced by audio_hblank_tick(),
    // not by the host audio sample rate. Sample generation only reads the
    // current serial output state.

    if (!enabled)
    {
        // SDL_AUDIO_U8 uses 0x80 as its zero-amplitude midpoint. Keep the
        // disabled state truly silent and clear any residual filter energy.
        flt_a = 0.0;
        flt_b = 0.0;
        flt_prv = kU8Silence;
        sample = kU8Silence;
    }
    else if (app_data.filter)
    {
        sample = filter_one_sample(sample);
    }

#ifndef NDEBUG
    gTrace.shiftAfter = gSoundShiftRegister & 0xFFFFFFu;
    gTrace.dividerAfter = gSoundHBlankPrescaler;
    gTrace.shiftCountAfter = gSoundShiftCount;
    gTrace.irqAfter = sound_IRQ;
    gTrace.filtered = sample;
#endif
    return sample;
}

void audio_generate_frame(int frame_rate, int scanline_count)
{
    if (!app_data.sound_en || frame_rate <= 0 || scanline_count <= 0)
        return;

    // Exact long-term sample rate. At the normal rates this yields 882
    // samples for PAL and 735 for NTSC, without reference to SDL queue state.
    gFrameSampleAccumulator += SAMPLE_RATE;
    const int samplesThisFrame =
        static_cast<int>(gFrameSampleAccumulator / static_cast<unsigned>(frame_rate));
    gFrameSampleAccumulator %= static_cast<unsigned>(frame_rate);

    int nextHBlankLine = 0;
    for (int sampleIndex = 0; sampleIndex < samplesThisFrame; ++sampleIndex)
    {
        int line = static_cast<int>(
            (static_cast<std::int64_t>(sampleIndex) * scanline_count) /
            samplesThisFrame);
        line = std::clamp(line, 0, MAXLINES - 1);

        // Finish all earlier scanlines before generating samples belonging
        // to this one. This places the shift event at the HBLANK boundary.
        while (nextHBlankLine < line && nextHBlankLine < scanline_count)
        {
            audio_hblank_tick(AudioVector[nextHBlankLine], nextHBlankLine);
            ++nextHBlankLine;
        }

        const int control = AudioVector[line];
#ifndef NDEBUG
        gDiagSampleInFrame = sampleIndex;
        const int enabledNow = (control >> 7) & 1;
        if (gDiagLastControl != control || gDiagLastEnabled != enabledNow)
        {
            signal_walk_open();
            if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit)
                std::fprintf(gSignalWalkLog,
                    "CONTROL frame=%d sample=%d line=%d old=%02X new=%02X enabled=%d ring=%zu\n",
                    gDiagFrame, sampleIndex, line, gDiagLastControl & 0xff, control & 0xff, enabledNow, gRingCount);
            gDiagDetailedSamplesRemaining = 200;
            gDiagLastControl = control;
            gDiagLastEnabled = enabledNow;
        }
        const std::size_t ringBefore = gRingCount;
#endif
        const unsigned char generated = generate_one_sample(control);
        ring_push(generated);
#ifndef NDEBUG
        ++gDiagProducedTotal;
        if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit && gDiagDetailedSamplesRemaining > 0)
        {
            std::fprintf(gSignalWalkLog,
                "SAMPLE frame=%d index=%d line=%d ctrl=%02X en=%d vol=%d noise=%d fast=%d recirc=%d "
                "shift=%06X->%06X div=%d->%d count=%d->%d didshift=%d out=%d nbit=%d mixed=%d "
                "raw=%u filter=%d prev=%u delta=%d a=%.4f->%.4f b=%.4f->%.4f final=%u "
                "ring=%zu->%zu irq=%d\n",
                gDiagFrame, sampleIndex, line, gTrace.control & 0xff, gTrace.enabled, gTrace.volume,
                gTrace.noise, (gTrace.control >> 5) & 1, gTrace.recirculate,
                static_cast<unsigned>(gTrace.shiftBefore), static_cast<unsigned>(gTrace.shiftAfter),
                gTrace.dividerBefore, gTrace.dividerAfter, gTrace.shiftCountBefore, gTrace.shiftCountAfter,
                gTrace.didShift, gTrace.outputBit, gTrace.noiseBit, gTrace.mixedBit, gTrace.raw,
                gTrace.filterApplied, gTrace.filterPrevBefore, gTrace.filterDelta,
                gTrace.filterABefore, gTrace.filterAAfter, gTrace.filterBBefore, gTrace.filterBAfter,
                generated, ringBefore, gRingCount, gTrace.irqAfter);
            --gDiagDetailedSamplesRemaining;
        }
#endif
    }

    // Tick HBLANK for the final scanline and any scanlines that did not map
    // to a host sample due to rate conversion.
    while (nextHBlankLine < scanline_count)
    {
        audio_hblank_tick(AudioVector[nextHBlankLine], nextHBlankLine);
        ++nextHBlankLine;
    }
#ifndef NDEBUG
    signal_walk_open();
    if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit)
    {
        std::fprintf(gSignalWalkLog,
            "FRAME frame=%d fps=%d lines=%d samples=%d ring=%zu produced_total=%llu overruns=%llu underruns=%llu AA=%02X shift=%06X hblank_prescaler=%d pending=%d written=%d\n",
            gDiagFrame, frame_rate, scanline_count, samplesThisFrame, gRingCount,
            static_cast<unsigned long long>(gDiagProducedTotal),
            static_cast<unsigned long long>(gRingOverruns),
            static_cast<unsigned long long>(gRingUnderruns), VDCwrite[AUD_CTRL],
            static_cast<unsigned>(gSoundShiftRegister & 0xffffffu), gSoundHBlankPrescaler,
            gSoundShiftPending ? 1 : 0, gSoundWritten ? 1 : 0);
        std::fflush(gSignalWalkLog);
    }
    ++gDiagFrame;
#endif
}

void init_sound_stream(void)
{
    if (!app_data.sound_en || gAudioStream)
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
        std::printf("SDL audio stream creation failed: %s\n", SDL_GetError());
        app_data.sound_en = 0;
        return;
    }

    if (!SDL_ResumeAudioStreamDevice(gAudioStream))
    {
        std::printf("SDL audio resume failed: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(gAudioStream);
        gAudioStream = nullptr;
        app_data.sound_en = 0;
        return;
    }

    flt_a = 0.0;
    flt_b = 0.0;
    flt_prv = kU8Silence;
    gNoiseLfsr = 0xACE1u;
    gSoundShiftRegister =
        VDCwrite[AUD_D2] |
        (static_cast<std::uint32_t>(VDCwrite[AUD_D1]) << 8) |
        (static_cast<std::uint32_t>(VDCwrite[AUD_D0]) << 16);
    gSoundHBlankPrescaler = 0;
    gSoundWritten = false;
    gSoundShiftPending = false;
    gSoundShiftCount = 0;
    gNoiseOutputBit = 0;
#if O2EM_USE_AUDIO8245
    gAudio8245.Reset();
    gAudio8245.LoadState(VDCwrite[AUD_D0], VDCwrite[AUD_D1],
                         VDCwrite[AUD_D2], VDCwrite[AUD_CTRL]);
#endif
    ring_reset();

#if O2EM_USE_AUDIO8245
    std::printf("SDL3 audio initialized: 44100 Hz, mono, 8-bit, Audio8245 development core\n");
#else
    std::printf("SDL3 audio initialized: 44100 Hz, mono, 8-bit, HBLANK shift clock + write/pending latch\n");
#endif
}

void init_audio(void)
{
#ifndef NDEBUG
    signal_walk_close();
    gDiagFrame = 0;
    gDiagSampleInFrame = 0;
    gDiagDetailedSamplesRemaining = 200;
    gDiagLastControl = -1;
    gDiagLastEnabled = -1;
    gDiagProducedTotal = 0;
    gDiagPoppedTotal = 0;
    gDiagSentTotal = 0;
    signal_walk_open();
#endif
    sound_IRQ = 0;
    sndlog = nullptr;
    gSoundShiftRegister = 0;
    gSoundHBlankPrescaler = 0;
    gSoundWritten = false;
    gSoundShiftPending = false;
    gSoundShiftCount = 0;
    gNoiseOutputBit = 0;
    gNoiseLfsr = 0xACE1u;
#if O2EM_USE_AUDIO8245
    gAudio8245.Reset();
#endif
    ring_reset();

    if (app_data.sound_en)
        init_sound_stream();
}

void update_audio(void)
{
    if (!app_data.sound_en || !gAudioStream)
        return;

    const int queued = SDL_GetAudioStreamQueued(gAudioStream);
    if (queued < 0)
        return;

    constexpr int kTargetQueuedBytes = SOUND_BUFFER_LEN * 3;
    if (queued >= kTargetQueuedBytes)
        return;

    const std::size_t wanted = static_cast<std::size_t>(kTargetQueuedBytes - queued);
    const std::size_t request = std::min<std::size_t>(wanted, SOUND_BUFFER_LEN * 2);

    unsigned char buffer[SOUND_BUFFER_LEN * 2];
    const std::size_t ringBeforePop = gRingCount;
    const std::size_t popped = ring_pop(buffer, request);
#ifndef NDEBUG
    gDiagPoppedTotal += popped;
#endif
    if (popped == 0)
    {
        ++gRingUnderruns;
        return;
    }

#ifndef NDEBUG
    signal_walk_open();
    if (gSignalWalkLog && gDiagFrame < kSignalWalkFrameLimit)
    {
        unsigned minv = 255, maxv = 0;
        unsigned long long sum = 0;
        for (std::size_t i = 0; i < popped; ++i)
        {
            minv = std::min<unsigned>(minv, buffer[i]);
            maxv = std::max<unsigned>(maxv, buffer[i]);
            sum += buffer[i];
        }
        std::fprintf(gSignalWalkLog,
            "SDL frame=%d queued=%d wanted=%zu request=%zu ring_before=%zu popped=%zu ring_after=%zu "
            "min=%u max=%u avg=%.3f first16=",
            gDiagFrame, queued, wanted, request, ringBeforePop, popped, gRingCount, minv, maxv,
            popped ? static_cast<double>(sum) / popped : 0.0);
        const std::size_t shown = std::min<std::size_t>(16, popped);
        for (std::size_t i = 0; i < shown; ++i)
            std::fprintf(gSignalWalkLog, "%s%02X", i ? " " : "", buffer[i]);
        std::fprintf(gSignalWalkLog, "\n");
        std::fflush(gSignalWalkLog);
    }
    if (gSignalWalkRaw && popped)
        std::fwrite(buffer, 1, popped, gSignalWalkRaw);
#endif
    if (!SDL_PutAudioStreamData(gAudioStream, buffer, static_cast<int>(popped)))
        std::printf("SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
#ifndef NDEBUG
    else
        gDiagSentTotal += popped;
#endif

    if (sndlog)
        std::fwrite(buffer, 1, popped, sndlog);
}

void mute_audio(void)
{
    if (!gAudioStream)
        return;

    SDL_ClearAudioStream(gAudioStream);
    SDL_PauseAudioStreamDevice(gAudioStream);
    ring_reset();
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
#ifndef NDEBUG
    signal_walk_close();
#endif

    app_data.sound_en = 0;
}
