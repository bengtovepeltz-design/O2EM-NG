// O2EM-NG NG-6 deterministic audio render probe.
//
// Loads BIOS + cartridge, runs a fixed number of machine frames on the NG
// timeline, captures the canonical signed-16-bit mono PCM produced by the
// owned AudioDevice, and writes a WAV (and optional raw PCM) plus a hash.
// Headless: no SDL audio device is opened.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "machine.h"
#include "audio/audio_device.h"
#include "timebase.h"

namespace {

struct CaptureHost final : o2em::ng::MachineHost {
    std::vector<std::int16_t> pcm;
    std::uint64_t frames = 0;
    void PresentFrame(const o2em::ng::Framebuffer&) override {}
    void PushAudio(const std::int16_t* samples, std::size_t count) override {
        pcm.insert(pcm.end(), samples, samples + count);
    }
    void OnFrameComplete(std::uint64_t n) override { frames = n; }
};

std::string Arg(int argc, char** argv, const char* name,
                const std::string& fallback = {}) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
    return fallback;
}

bool Has(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

std::uint64_t Fnv1a(const std::int16_t* data, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    const auto* b = reinterpret_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < n * sizeof(std::int16_t); ++i) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

struct Press {
    int frame;
    int row;
    std::uint8_t bits;
};

std::vector<Press> ParsePlan(const std::string& plan) {
    std::vector<Press> out;
    if (plan.empty() || plan == "none") return out;
    if (plan == "standard" || plan == "vp59")
        return {{250, 0, 0x02}, {310, 5, 0x80}};
    if (plan == "vp48")
        return {{250, 0, 0x40}, {310, 5, 0x20}};
    std::size_t start = 0;
    while (start < plan.size()) {
        const std::size_t comma = plan.find(',', start);
        const std::string item =
            plan.substr(start, comma == std::string::npos ? std::string::npos
                                                          : comma - start);
        int frame = 0, row = 0, bits = 0;
        if (std::sscanf(item.c_str(), "%d:%d:%d", &frame, &row, &bits) == 3)
            out.push_back({frame, row, static_cast<std::uint8_t>(bits)});
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

void WriteLe16(std::FILE* f, std::uint16_t v) {
    const std::uint8_t b[2] = {static_cast<std::uint8_t>(v & 0xFF),
                               static_cast<std::uint8_t>(v >> 8)};
    std::fwrite(b, 1, 2, f);
}

void WriteLe32(std::FILE* f, std::uint32_t v) {
    const std::uint8_t b[4] = {static_cast<std::uint8_t>(v & 0xFF),
                               static_cast<std::uint8_t>((v >> 8) & 0xFF),
                               static_cast<std::uint8_t>((v >> 16) & 0xFF),
                               static_cast<std::uint8_t>((v >> 24) & 0xFF)};
    std::fwrite(b, 1, 4, f);
}

bool WriteWav(const std::string& path, const std::vector<std::int16_t>& pcm,
              int rate) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::uint32_t data_bytes =
        static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
    std::fwrite("RIFF", 1, 4, f);
    WriteLe32(f, 36 + data_bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    WriteLe32(f, 16);
    WriteLe16(f, 1); // PCM
    WriteLe16(f, 1); // mono
    WriteLe32(f, static_cast<std::uint32_t>(rate));
    WriteLe32(f, static_cast<std::uint32_t>(rate * 2));
    WriteLe16(f, 2);
    WriteLe16(f, 16);
    std::fwrite("data", 1, 4, f);
    WriteLe32(f, data_bytes);
    if (data_bytes) std::fwrite(pcm.data(), 1, data_bytes, f);
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string bios = Arg(argc, argv, "--bios",
                                 "D:\\Project\\O2EM-NG\\BIOS\\G7000.bin");
    const std::string cart = Arg(argc, argv, "--cart");
    if (cart.empty()) {
        std::fprintf(stderr, "--cart is required\n");
        return 2;
    }
    const std::string region = Arg(argc, argv, "--region", "pal");
    const int frames = std::atoi(Arg(argc, argv, "--frames", "500").c_str());
    const std::string plan = Arg(argc, argv, "--plan", "none");
    const std::string wav = Arg(argc, argv, "--wav");
    const std::string raw = Arg(argc, argv, "--raw");
    const bool quiet = Has(argc, argv, "--quiet");

    CaptureHost host;
    o2em::ng::MachineConfig config;
    config.bios_path = bios;
    config.cartridge_path = cart;
    config.region = region == "ntsc" ? o2em::ng::Region::Ntsc
                                     : o2em::ng::Region::Pal;
    config.vpp_enabled = Has(argc, argv, "--g7400");
    config.g7400 = config.vpp_enabled;
    auto machine = o2em::ng::Machine::Create(config, host);
    if (!machine->Load()) {
        std::fprintf(stderr, "load failed: bios=%s cart=%s\n", bios.c_str(),
                     cart.c_str());
        return 3;
    }
    machine->PowerOn();

    const std::int64_t frame_ticks = machine->Profile().FrameTicks();
    for (const Press& p : ParsePlan(plan)) {
        o2em::ng::InputTransition down;
        down.tick = static_cast<o2em::ng::Tick>(p.frame) * frame_ticks;
        down.target = o2em::ng::InputTarget::KeyboardRow;
        down.index = static_cast<std::uint8_t>(p.row);
        down.value = p.bits;
        machine->ScheduleInput(down);
        o2em::ng::InputTransition up = down;
        up.tick = static_cast<o2em::ng::Tick>(p.frame + 3) * frame_ticks;
        up.value = 0;
        machine->ScheduleInput(up);
    }

    for (int i = 0; i < frames; ++i) machine->RunFrame();

    const int rate = machine->Audio().SampleRate();
    const std::uint64_t hash = Fnv1a(host.pcm.data(), host.pcm.size());
    if (!wav.empty() && !WriteWav(wav, host.pcm, rate)) {
        std::fprintf(stderr, "wav write failed: %s\n", wav.c_str());
        return 4;
    }
    if (!raw.empty()) {
        std::FILE* f = std::fopen(raw.c_str(), "wb");
        if (!f) return 5;
        if (!host.pcm.empty())
            std::fwrite(host.pcm.data(), 1,
                        host.pcm.size() * sizeof(std::int16_t), f);
        std::fclose(f);
    }
    if (!quiet) {
        std::printf(
            "cart=%s region=%s frames=%d samples=%zu rate=%d "
            "start_tick=0 end_tick=%lld hash=%016llx\n",
            cart.c_str(), region.c_str(), frames, host.pcm.size(), rate,
            static_cast<long long>(machine->Now()),
            static_cast<unsigned long long>(hash));
    }
    return 0;
}
