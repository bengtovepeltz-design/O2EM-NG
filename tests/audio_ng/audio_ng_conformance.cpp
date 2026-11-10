// O2EM-NG NG-6 i8244/i8245 sound-device conformance suite.
//
// Deterministic, headless tests for the owned AudioDevice and its integration
// with the NG machine. No SDL audio device is opened.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_device.h"
#include "machine.h"
#include "timebase.h"
#include "video/vdc.h"

namespace {

int g_checks = 0;
int g_failures = 0;
int g_skips = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL  %s\n", what);
    }
}

void CheckEq(std::uint64_t actual, std::uint64_t expected, const char* what) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::printf("FAIL  %s (got %llu, expected %llu)\n", what,
                    static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
    }
}

std::uint64_t Fnv1a(const std::int16_t* data, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < n * sizeof(std::int16_t); ++i) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

o2em::ng::AudioDevice MakeAudio(const o2em::ng::HardwareProfile& p,
                                int rate = 48000) {
    o2em::ng::AudioDevice a;
    a.Configure(p, rate);
    a.Reset(0);
    return a;
}

void LoadPattern(o2em::ng::AudioDevice& a, std::uint8_t h, std::uint8_t m,
                 std::uint8_t l, o2em::ng::Tick tick = 0) {
    a.WriteRegister(0xA7, h, tick);
    a.WriteRegister(0xA8, m, tick);
    a.WriteRegister(0xA9, l, tick);
}

// Consume the write latch and reach the first prescaler shift (slow clock).
void FirstSlowShift(o2em::ng::AudioDevice& a, const o2em::ng::HardwareProfile& p) {
    for (int i = 1; i <= 16; ++i)
        a.Hblank(static_cast<o2em::ng::Tick>(i) * p.LineTicks(), false);
}

void TestReset() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    CheckEq(a.SoundRegister(), 0, "reset: sound register");
    CheckEq(a.ShiftHigh(), 0, "reset: shift high");
    CheckEq(a.ShiftMid(), 0, "reset: shift mid");
    CheckEq(a.ShiftLow(), 0, "reset: shift low");
    Check(!a.ShiftOutput(), "reset: output bit");
    Check(!a.IrqLine(), "reset: IRQ");
    CheckEq(a.LastTick(), 0, "reset: last tick");
    Check(a.Pcm().empty(), "reset: no PCM");
    a.PowerOn();
    CheckEq(a.SoundRegister(), 0, "power-on: sound register");
    CheckEq(a.LastTick(), 0, "power-on: tick");
}

void TestRegisters() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    LoadPattern(a, 0x12, 0x34, 0x56, 0);
    CheckEq(a.ShiftHigh(), 0x12, "regs: A7 high byte");
    CheckEq(a.ShiftMid(), 0x34, "regs: A8 middle byte");
    CheckEq(a.ShiftLow(), 0x56, "regs: A9 low byte");
    Check(a.Written(), "regs: A7-A9 set write latch");
    CheckEq(a.ReadRegister(0xA7), 0, "regs: A7 write-only readback");
    CheckEq(a.ReadRegister(0xA8), 0, "regs: A8 write-only readback");
    CheckEq(a.ReadRegister(0xA9), 0, "regs: A9 write-only readback");
    a.WriteRegister(0xAA, 0xFF, 0);
    CheckEq(a.SoundRegister(), 0xBF, "regs: AA bit 6 is not connected");
    CheckEq(a.ReadRegister(0xAA), 0xBF, "regs: AA readback");
    a.WriteRegister(0xAA, 0x00, 0);
    CheckEq(a.SoundRegister(), 0x00, "regs: AA clear");
    a.WriteRegister(0xAB, 0xFF, 0);
    CheckEq(a.SoundRegister(), 0x00, "regs: unused sound register ignored");
}

void TestPrescalerAndShift() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    LoadPattern(a, 0x00, 0x00, 0x01);
    // The first HBLANK consumes the write latch and does not shift.
    a.Hblank(p.LineTicks(), false);
    Check(!a.Written(), "clock: write latch consumed");
    Check(!a.ShiftOutput(), "clock: no shift on write event");
    for (int i = 2; i <= 15; ++i)
        a.Hblank(static_cast<o2em::ng::Tick>(i) * p.LineTicks(), false);
    Check(!a.ShiftOutput(), "clock: slow clock does not shift before 16 lines");
    a.Hblank(16 * p.LineTicks(), false);
    Check(a.ShiftOutput(), "clock: slow clock shifts at 16 lines");
    CheckEq(a.ShiftHigh(), 0x80, "clock: loop feedback enters bit 23");
    CheckEq(a.LastShiftTick(), 16 * p.LineTicks(), "clock: shift timestamp");

    o2em::ng::AudioDevice f = MakeAudio(p);
    f.WriteRegister(0xAA, 0x20, 0); // fast clock
    LoadPattern(f, 0x00, 0x00, 0x01);
    f.Hblank(p.LineTicks(), false); // consume write latch
    for (int i = 2; i <= 3; ++i) f.Hblank(i * p.LineTicks(), false);
    Check(!f.ShiftOutput(), "clock: fast clock before 4 lines");
    f.Hblank(4 * p.LineTicks(), false);
    Check(f.ShiftOutput(), "clock: fast clock shifts at 4 lines");
}

void TestShiftDirection() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    LoadPattern(a, 0x00, 0x00, 0x01);
    FirstSlowShift(a, p);
    Check(a.ShiftOutput(), "shift: first bit is 1");
    CheckEq(a.ShiftLow(), 0x00, "shift: low byte shifted to zero");
    CheckEq(a.ShiftHigh(), 0x80, "shift: feedback high bit");
    // Next shift reads the new low byte bit 0 (which is 0).
    for (int i = 17; i <= 32; ++i)
        a.Hblank(i * p.LineTicks(), false);
    Check(!a.ShiftOutput(), "shift: next bit is 0");

    // Alternating pattern 0x0000AA: bit0=0, bit1=1, bit2=0 ... after feedback.
    o2em::ng::AudioDevice b = MakeAudio(p);
    LoadPattern(b, 0x00, 0x00, 0xAA);
    FirstSlowShift(b, p);
    Check(!b.ShiftOutput(), "shift: alternating first bit");
    for (int i = 17; i <= 32; ++i)
        b.Hblank(i * p.LineTicks(), false);
    Check(b.ShiftOutput(), "shift: alternating second bit");
    for (int i = 33; i <= 48; ++i)
        b.Hblank(i * p.LineTicks(), false);
    Check(!b.ShiftOutput(), "shift: alternating third bit");
}

void TestNoise() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    a.WriteRegister(0xAA, 0x90, 0); // noise + enable
    LoadPattern(a, 0x00, 0x00, 0x20);
    FirstSlowShift(a, p);
    // Pre-shift signal 0x20: output=0, pre-shift bit4 of 0x10 = 1 -> feedback=1.
    CheckEq(a.ShiftHigh() & 0x80, 0x80, "noise: tap enters feedback bit");
    Check(!a.ShiftOutput(), "noise: output remains shift bit 0");

    o2em::ng::AudioDevice b = MakeAudio(p);
    b.WriteRegister(0xAA, 0x80, 0); // enable, no noise
    LoadPattern(b, 0x00, 0x00, 0x20);
    FirstSlowShift(b, p);
    CheckEq(b.ShiftHigh() & 0x80, 0x00, "noise: disabled uses plain feedback");

    // Repeated deterministic noise sequence.
    auto run = [&](std::uint64_t& hash, int& ones) {
        o2em::ng::AudioDevice n = MakeAudio(p);
        n.WriteRegister(0xAA, 0x9F, 0);
        LoadPattern(n, 0x12, 0x34, 0x56);
        n.Hblank(p.LineTicks(), false);
        ones = 0;
        std::vector<std::uint8_t> bits;
        for (int i = 2; i <= 200; ++i) {
            n.Hblank(i * p.LineTicks(), false);
            if ((i & 15) == 0) {
                bits.push_back(n.ShiftOutput() ? 1 : 0);
                ones += bits.back();
            }
        }
        hash = 1469598103934665603ull;
        for (std::uint8_t bit : bits) {
            hash ^= bit;
            hash *= 1099511628211ull;
        }
    };
    std::uint64_t h1 = 0, h2 = 0;
    int o1 = 0, o2 = 0;
    run(h1, o1);
    run(h2, o2);
    CheckEq(h1, h2, "noise: deterministic sequence hash");
    CheckEq(o1, o2, "noise: deterministic output count");
    Check(o1 > 0 && o1 < 13, "noise: sequence is not constant");
}

void TestAmplitude() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    LoadPattern(a, 0x00, 0x00, 0x01);
    FirstSlowShift(a, p); // output now 1
    a.WriteRegister(0xAA, 0x8F, 16 * p.LineTicks());
    std::vector<std::int16_t> tmp;
    tmp.clear(); a.Drain(tmp);
    a.AdvanceTo(a.NextSampleTick());
    tmp.clear(); a.Drain(tmp);
    const o2em::ng::Tick start = a.LastTick();
    a.AdvanceTo(start + 30000);
    tmp.clear(); a.Drain(tmp);
    bool max_ok = !tmp.empty();
    for (std::int16_t s : tmp)
        if (s != 15360) max_ok = false;
    Check(max_ok, "amplitude: volume 15 -> 15360");

    a.WriteRegister(0xAA, 0x88, a.LastTick());
    a.AdvanceTo(a.NextSampleTick());
    tmp.clear(); a.Drain(tmp);
    a.AdvanceTo(a.LastTick() + 30000);
    tmp.clear(); a.Drain(tmp);
    bool half_ok = !tmp.empty();
    for (std::int16_t s : tmp)
        if (s != 8192) half_ok = false;
    Check(half_ok, "amplitude: volume 8 -> 8192");

    a.WriteRegister(0xAA, 0x08, a.LastTick()); // enable off
    a.AdvanceTo(a.NextSampleTick());
    tmp.clear(); a.Drain(tmp);
    a.AdvanceTo(a.LastTick() + 30000);
    tmp.clear(); a.Drain(tmp);
    bool off_ok = !tmp.empty();
    for (std::int16_t s : tmp)
        if (s != 0) off_ok = false;
    Check(off_ok, "amplitude: enable off -> silence");

    a.WriteRegister(0xAA, 0x80, a.LastTick()); // volume 0
    a.AdvanceTo(a.NextSampleTick());
    tmp.clear(); a.Drain(tmp);
    a.AdvanceTo(a.LastTick() + 30000);
    tmp.clear(); a.Drain(tmp);
    bool zero_ok = !tmp.empty();
    for (std::int16_t s : tmp)
        if (s != 0) zero_ok = false;
    Check(zero_ok, "amplitude: volume 0 -> silence");

    // Disabled output before any shift is silence too.
    o2em::ng::AudioDevice s = MakeAudio(p);
    s.WriteRegister(0xAA, 0x8F, 0);
    s.AdvanceTo(100000);
    std::vector<std::int16_t> sp;
    s.Drain(sp);
    bool silent = !sp.empty();
    for (std::int16_t v : sp)
        if (v != 0) silent = false;
    Check(silent, "amplitude: silent shift register");
}

void TestTimestampedWrite() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    LoadPattern(a, 0x00, 0x00, 0x01);
    FirstSlowShift(a, p);
    a.WriteRegister(0xAA, 0x8F, 16 * p.LineTicks());
    std::vector<std::int16_t> tmp;
    a.AdvanceTo(a.NextSampleTick());
    tmp.clear(); a.Drain(tmp);
    const o2em::ng::Tick boundary = a.NextSampleTick();
    a.AdvanceTo(boundary); // generate one complete high sample
    a.WriteRegister(0xAA, 0x8F, boundary);
    std::vector<std::int16_t> before;
    a.Drain(before);
    CheckEq(before.size(), 1, "timestamp: one high sample before write");
    CheckEq(static_cast<std::uint64_t>(before[0]), 15360,
            "timestamp: high sample value");
    a.WriteRegister(0xAA, 0x00, a.LastTick()); // disable at boundary
    a.AdvanceTo(a.LastTick() + 30000);
    std::vector<std::int16_t> after;
    a.Drain(after);
    bool silent = !after.empty();
    for (std::int16_t v : after)
        if (v != 0) silent = false;
    Check(silent, "timestamp: new state applies after the write tick");
}

void TestSampleRateAndProfiles() {
    const auto pal = o2em::ng::MakePalProfile();
    const auto ntsc = o2em::ng::MakeNtscProfile();
    const o2em::ng::HardwareProfile* profiles[2] = {&pal, &ntsc};
    const char* profile_names[2] = {"rate: PAL 48 kHz sample count",
                                    "rate: NTSC 48 kHz sample count"};
    for (int pi = 0; pi < 2; ++pi) {
        const auto& p = *profiles[pi];
        o2em::ng::AudioDevice a = MakeAudio(p);
        a.WriteRegister(0xAA, 0x8F, 0);
        LoadPattern(a, 0x00, 0x00, 0x01);
        const o2em::ng::Tick one_second =
            static_cast<o2em::ng::Tick>((p.cpu_hz.num * 12) / p.cpu_hz.den);
        for (o2em::ng::Tick t = p.LineTicks(); t < one_second; t += p.LineTicks())
            a.Hblank(t, false);
        a.AdvanceTo(one_second);
        std::vector<std::int16_t> pcm;
        a.Drain(pcm);
        Check(pcm.size() >= 47999 && pcm.size() <= 48001, profile_names[pi]);
    }

    o2em::ng::AudioDevice a = MakeAudio(pal, 44100);
    a.WriteRegister(0xAA, 0x8F, 0);
    a.AdvanceTo(static_cast<o2em::ng::Tick>((pal.cpu_hz.num * 12) / pal.cpu_hz.den));
    std::vector<std::int16_t> pcm;
    a.Drain(pcm);
    Check(pcm.size() >= 44099 && pcm.size() <= 44101,
          "rate: 44.1 kHz generation");
}

void TestGoldenPcm() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    a.WriteRegister(0xAA, 0xAF, 0); // enable + fast + volume 15
    LoadPattern(a, 0x80, 0x00, 0x01);
    for (int i = 1; i <= 500; ++i)
        a.Hblank(static_cast<o2em::ng::Tick>(i) * p.LineTicks(), true);
    a.AdvanceTo(501 * p.LineTicks());
    std::vector<std::int16_t> pcm;
    a.Drain(pcm);
    const std::uint64_t hash = Fnv1a(pcm.data(), pcm.size());
    CheckEq(hash, 0x746EA72EF3827A7Aull, "golden: deterministic PCM hash");
    CheckEq(pcm.size(), 1545, "golden: deterministic PCM sample count");
}

void TestSaveState() {
    const auto p = o2em::ng::MakePalProfile();
    o2em::ng::AudioDevice a = MakeAudio(p);
    a.WriteRegister(0xAA, 0x9F, 0);
    LoadPattern(a, 0x00, 0x00, 0x20);
    for (int i = 1; i <= 60; ++i) a.Hblank(i * p.LineTicks(), true);
    a.AdvanceTo(61 * p.LineTicks());
    std::vector<std::int16_t> discard;
    a.Drain(discard);

    o2em::ng::SnapshotWriter w;
    Check(a.SaveState(w), "save: VPP-independent audio save");
    a.AdvanceTo(90 * p.LineTicks());
    std::vector<std::int16_t> pcm1;
    a.Drain(pcm1);
    const std::uint64_t h1 = Fnv1a(pcm1.data(), pcm1.size());

    o2em::ng::AudioDevice b = MakeAudio(p);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(b.LoadState(r), "save: restore");
    Check(r.ok, "save: consumed");
    b.AdvanceTo(90 * p.LineTicks());
    std::vector<std::int16_t> pcm2;
    b.Drain(pcm2);
    const std::uint64_t h2 = Fnv1a(pcm2.data(), pcm2.size());
    CheckEq(h2, h1, "save: resumed PCM hash identical");
    CheckEq(b.ShiftHigh(), a.ShiftHigh(), "save: shift high restored");
    CheckEq(b.Prescaler(), a.Prescaler(), "save: prescaler restored");
    CheckEq(b.ShiftCount(), a.ShiftCount(), "save: shift count restored");
    CheckEq(b.IrqLine() ? 1 : 0, a.IrqLine() ? 1 : 0, "save: IRQ restored");
}

// ---------------------------------------------------------------------------
// Machine / CPU / video integration

struct CaptureHost final : o2em::ng::MachineHost {
    std::vector<std::int16_t> pcm;
    std::size_t pushes = 0;
    void PresentFrame(const o2em::ng::Framebuffer&) override {}
    void PushAudio(const std::int16_t* samples, std::size_t count) override {
        ++pushes;
        pcm.insert(pcm.end(), samples, samples + count);
    }
    void OnFrameComplete(std::uint64_t) override {}
};

std::vector<std::uint8_t> MakeBytes(std::size_t n, std::uint8_t v) {
    return std::vector<std::uint8_t>(n, v);
}

void TestCpuTiming() {
    const auto pal = o2em::ng::MakePalProfile();
    CaptureHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    bios[0] = 0x84; bios[1] = 0x00; // JMP 0x400
    std::vector<std::uint8_t> cart = MakeBytes(0x800, 0xFF);
    // MOV A,#20; OUTL P1,A; MOV R0,#AA; MOV A,#8F; MOVX @R0,A
    const std::uint8_t program[] = {0x23, 0x20, 0x39, 0xB8, 0xAA,
                                    0x23, 0x8F, 0x90, 0x84, 0x08};
    for (std::size_t i = 0; i < sizeof(program); ++i) cart[i] = program[i];
    m->LoadBiosBytes(std::move(bios), "audio-bios");
    m->LoadCartridgeBytes(std::move(cart), "audio-cart");
    m->PowerOn();

    bool wrote = false;
    for (int i = 0; i < 12; ++i) {
        m->RunInstructions(1);
        if (m->Audio().SoundRegister() == 0x8F) {
            wrote = true;
            CheckEq(static_cast<std::uint64_t>(m->Audio().LastWriteTick()), 1800,
                    "cpu: sound write tick is instruction-start tick");
            CheckEq(static_cast<std::uint64_t>(m->Now()), 2160,
                    "cpu: MOVX completes two cycles after the write");
            break;
        }
    }
    Check(wrote, "cpu: synthetic program wrote AA");
    CheckEq(m->Audio().SoundRegister(), 0x8F, "cpu: sound register value");
}

void TestVideoTiming() {
    const auto pal = o2em::ng::MakePalProfile();
    CaptureHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    bios[0] = 0x84; bios[1] = 0x00;
    std::vector<std::uint8_t> cart = MakeBytes(0x800, 0xFF);
    // Enable sound, volume 15, fast clock is not set; load 0x000001.
    const std::uint8_t program[] = {
        0x23, 0x20, 0x39,
        0xB8, 0xAA, 0x23, 0x8F, 0x90,
        0xB8, 0xA9, 0x23, 0x01, 0x90,
        0x84, 0x0F};
    for (std::size_t i = 0; i < sizeof(program); ++i) cart[i] = program[i];
    m->LoadBiosBytes(std::move(bios), "audio-video-bios");
    m->LoadCartridgeBytes(std::move(cart), "audio-video-cart");
    m->PowerOn();
    m->RunFrame();
    m->RunFrame();
    const o2em::ng::Tick hbl = static_cast<o2em::ng::Tick>(pal.hblank_start) *
                               pal.pixel_ticks;
    Check(m->Audio().LastShiftTick() > 0, "video: audio shifted");
    CheckEq(static_cast<std::uint64_t>(m->Audio().LastShiftTick() % pal.LineTicks()),
            static_cast<std::uint64_t>(hbl),
            "video: shift occurs on the VDC HBLANK tick");
    Check(!host.pcm.empty(), "video: machine pushed PCM to host");
    Check(host.pushes >= 2, "host: one audio push per frame");
}

void TestMachineSaveState() {
    const auto p = o2em::ng::MakePalProfile();
    CaptureHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto a = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    bios[0] = 0x84; bios[1] = 0x00;
    std::vector<std::uint8_t> cart = MakeBytes(0x800, 0xFF);
    const std::uint8_t program[] = {0x23, 0x20, 0x39, 0xB8, 0xAA, 0x23, 0x8F,
                                    0x90, 0xB8, 0xA9, 0x23, 0x01, 0x90, 0x84, 0x0F};
    for (std::size_t i = 0; i < sizeof(program); ++i) cart[i] = program[i];
    a->LoadBiosBytes(bios, "save-bios");
    a->LoadCartridgeBytes(cart, "save-cart");
    a->PowerOn();
    for (int i = 0; i < 20; ++i) a->RunFrame();
    std::vector<std::uint8_t> state;
    Check(a->SaveState(state), "machine save: save");
    a->RunFrame();
    const std::size_t cut = host.pcm.size();
    a->RunFrame();
    const std::uint64_t h1 = Fnv1a(host.pcm.data() + cut, host.pcm.size() - cut);

    CaptureHost host2;
    auto b = o2em::ng::Machine::Create(config, host2);
    b->LoadBiosBytes(bios, "save-bios");
    b->LoadCartridgeBytes(cart, "save-cart");
    b->PowerOn();
    Check(b->LoadState(state.data(), state.size()), "machine save: restore");
    b->RunFrame();
    const std::size_t cut2 = host2.pcm.size();
    b->RunFrame();
    const std::uint64_t h2 =
        Fnv1a(host2.pcm.data() + cut2, host2.pcm.size() - cut2);
    CheckEq(h2, h1, "machine save: resumed PCM hash identical");
}

} // namespace

int main() {
    std::printf("NG-6 i8244/i8245 audio conformance\n");
    TestReset();
    TestRegisters();
    TestPrescalerAndShift();
    TestShiftDirection();
    TestNoise();
    TestAmplitude();
    TestTimestampedWrite();
    TestSampleRateAndProfiles();
    TestGoldenPcm();
    TestSaveState();
    TestCpuTiming();
    TestVideoTiming();
    TestMachineSaveState();
    std::printf("\nchecks: %d, failures: %d, skips: %d\n", g_checks,
                g_failures, g_skips);
    if (g_failures == 0) {
        std::printf("RESULT: GREEN - NG-6 audio conformance passed.\n");
        return 0;
    }
    std::printf("RESULT: RED - NG-6 audio conformance failures.\n");
    return 1;
}
