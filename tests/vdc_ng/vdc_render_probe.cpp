// O2EM-NG NG-3 deterministic VDC render probe.
//
// Loads the real G7000 BIOS and a cartridge, drives the scripted input at
// exact machine-frame boundaries, runs a fixed number of frames, and dumps
// the real VDC framebuffer (indexed + PPM). Used by the MAME differential and
// the title probe matrix. No SDL, frontend, legacy core or title hacks.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "machine.h"
#include "timebase.h"
#include "video/vdc_state.h"
#include "video/vpp.h"

namespace {

struct ProbeHost final : o2em::ng::MachineHost {
    std::uint64_t frames = 0;
    void PresentFrame(const o2em::ng::Framebuffer&) override {}
    void PushAudio(const std::int16_t*, std::size_t) override {}
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

std::uint64_t Fnv1a(const std::uint8_t* data, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

bool WritePpm(const std::string& path, const o2em::ng::Framebuffer& fb) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", fb.width, fb.height);
    for (int y = 0; y < fb.height; ++y) {
        for (int x = 0; x < fb.width; ++x) {
            const std::uint8_t idx =
                fb.pixels[static_cast<std::size_t>(y) * fb.stride + x] & 0x0F;
            const o2em::ng::VdcRgb& c = o2em::ng::kI8244PaletteRgb[idx];
            std::uint8_t rgb[3] = {c.r, c.g, c.b};
            std::fwrite(rgb, 1, 3, f);
        }
    }
    std::fclose(f);
    return true;
}

bool WriteRaw(const std::string& path, const o2em::ng::Framebuffer& fb) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(fb.pixels.data(), 1, fb.pixels.size(), f);
    std::fclose(f);
    return true;
}

struct Press {
    int frame;
    int row;
    std::uint8_t bits;
};

std::vector<Press> ParsePlan(const std::string& plan) {
    std::vector<Press> out;
    if (plan.empty() || plan == "none") return out;
    if (plan == "standard" || plan == "vp59") {
        return {{250, 0, 0x02}, {310, 5, 0x80}};
    }
    if (plan == "vp48") {
        return {{250, 0, 0x40}, {310, 5, 0x20}};
    }
    if (plan == "c7010") {
        return {{250, 0, 0x02}, {310, 5, 0x10}, {370, 0, 0x04}};
    }
    // Custom: frame:row:bits,frame:row:bits
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
    const std::string plan = Arg(argc, argv, "--plan", "standard");
    const std::string dump = Arg(argc, argv, "--dump");
    const std::string raw = Arg(argc, argv, "--raw");
    const std::string regs_out = Arg(argc, argv, "--regs-out");
    const std::string trace_out = Arg(argc, argv, "--trace-out");
    const int trace_from = std::atoi(Arg(argc, argv, "--trace-from", "0").c_str());
    const int trace_to = std::atoi(Arg(argc, argv, "--trace-to", "999999").c_str());
    const std::string cpu_out = Arg(argc, argv, "--cpu-out");
    const std::string pc_out = Arg(argc, argv, "--pc-out");
    const int pc_count = std::atoi(Arg(argc, argv, "--pc-count", "0").c_str());
    const int hold = std::atoi(Arg(argc, argv, "--hold", "3").c_str());
    const bool quiet = Has(argc, argv, "--quiet");

    ProbeHost host;
    o2em::ng::MachineConfig config;
    config.bios_path = bios;
    config.cartridge_path = cart;
    config.vpp_enabled = Has(argc, argv, "--g7400");
    config.g7400 = config.vpp_enabled;
    config.region = region == "ntsc" ? o2em::ng::Region::Ntsc
                                     : o2em::ng::Region::Pal;
    auto machine = o2em::ng::Machine::Create(config, host);
    if (!machine->Load()) {
        std::fprintf(stderr, "load failed: bios=%s cart=%s\n", bios.c_str(),
                     cart.c_str());
        return 3;
    }
    machine->PowerOn();

    const std::int64_t frame_ticks = machine->Profile().FrameTicks();
    if (!trace_out.empty()) {
        o2em::ng::BusTrace& t = machine->MutableTrace();
        o2em::ng::BusTraceFilter f;
        f.from_tick = static_cast<o2em::ng::Tick>(trace_from) * frame_ticks;
        f.to_tick = static_cast<o2em::ng::Tick>(trace_to) * frame_ticks;
        f.op_mask = static_cast<std::uint16_t>(
            (1u << static_cast<unsigned>(o2em::ng::BusOp::MovxRead)) |
            (1u << static_cast<unsigned>(o2em::ng::BusOp::MovxWrite)));
        f.include_program_fetch = false;
        f.max_records = 400000;
        t.SetFilter(f);
        t.Reset();
        t.SetEnabled(true);
    }
    for (const Press& p : ParsePlan(plan)) {
        o2em::ng::InputTransition down;
        down.tick = static_cast<o2em::ng::Tick>(p.frame) * frame_ticks;
        down.target = o2em::ng::InputTarget::KeyboardRow;
        down.index = static_cast<std::uint8_t>(p.row);
        down.value = p.bits;
        machine->ScheduleInput(down);
        o2em::ng::InputTransition up = down;
        up.tick = static_cast<o2em::ng::Tick>(p.frame + hold) * frame_ticks;
        up.value = 0;
        machine->ScheduleInput(up);
    }

    if (!pc_out.empty() && pc_count > 0) {
        std::FILE* f = std::fopen(pc_out.c_str(), "w");
        if (!f) {
            std::fprintf(stderr, "pc write failed: %s\n", pc_out.c_str());
            return 8;
        }
        for (int i = 0; i < pc_count; ++i) {
            const auto& s = machine->CpuState();
            std::fprintf(f, "%03X %02X %02X %d %d %d %d %llu %d %d %02X\n", s.pc,
                         s.timer, s.prescaler, s.timer_mode, s.timer_flag,
                         s.f1 ? 1 : 0, s.irq_enabled ? 1 : 0,
                         static_cast<unsigned long long>(s.cycles),
                         s.timer_irq_enabled ? 1 : 0,
                         machine->Bus().IrqLine() ? 1 : 0,
                         machine->Vdc().Status());
            machine->RunInstructions(1);
        }
        std::fclose(f);
        return 0;
    }

    std::FILE* cpu_file = cpu_out.empty() ? nullptr : std::fopen(cpu_out.c_str(), "w");
    if (cpu_file) std::fprintf(cpu_file, "frame pc a psw sp p1 p2 cyc irqen\n");
    for (int i = 0; i < frames; ++i) {
        machine->RunFrame();
        if (cpu_file) {
            const auto& s = machine->CpuState();
            std::fprintf(cpu_file, "%d %03X %02X %02X %d %02X %02X %llu %d\n",
                         i + 1, s.pc, s.a, s.psw, s.sp, s.p1, s.p2,
                         static_cast<unsigned long long>(s.cycles),
                         s.irq_enabled ? 1 : 0);
        }
    }
    if (cpu_file) std::fclose(cpu_file);

    const o2em::ng::Framebuffer& fb = machine->GetFramebuffer();
    const std::uint64_t hash = Fnv1a(fb.pixels.data(), fb.pixels.size());
    if (!quiet) {
        std::printf(
            "cart=%s region=%s frames=%d frame#=%llu completed=%llu "
            "size=%dx%d stride=%d hash=%016llx\n",
            cart.c_str(), region.c_str(), frames,
            static_cast<unsigned long long>(fb.frame_number),
            static_cast<unsigned long long>(machine->Vdc().CompletedFrames()),
            fb.width, fb.height, fb.stride,
            static_cast<unsigned long long>(hash));
        std::printf("vdc status=%02X control=%02X irq=%d\n",
                    machine->Vdc().Status(), machine->Vdc().Control(),
                    machine->Vdc().IrqLine() ? 1 : 0);
        if (config.vpp_enabled) {
            std::printf("vpp mix_i8244=%02X mix_ef934x=%02X b_pin=%d r=%02X x=%02X y=%02X "
                        "y0=%02X m=%02X\n",
                        machine->Vpp().MixI8244(), machine->Vpp().MixEf934x(),
                        machine->Vpp().BPin() ? 1 : 0, machine->Vpp().R(),
                        machine->Vpp().X(), machine->Vpp().Y(),
                        machine->Vpp().Y0(), machine->Vpp().M());
        }
    }

    if (!dump.empty() && !WritePpm(dump, fb)) {
        std::fprintf(stderr, "ppm write failed: %s\n", dump.c_str());
        return 4;
    }
    if (!raw.empty() && !WriteRaw(raw, fb)) {
        std::fprintf(stderr, "raw write failed: %s\n", raw.c_str());
        return 5;
    }
    if (!regs_out.empty()) {
        std::FILE* f = std::fopen(regs_out.c_str(), "w");
        if (!f) {
            std::fprintf(stderr, "regs write failed: %s\n", regs_out.c_str());
            return 6;
        }
        const std::uint8_t* r = machine->Vdc().Registers();
        for (int i = 0; i < 0x100; ++i) {
            std::fprintf(f, "%02X%s", r[i], ((i & 15) == 15) ? "\n" : " ");
        }
        std::fprintf(f, "control=%02X status=%02X irq=%d completed=%llu\n",
                     machine->Vdc().Control(), machine->Vdc().Status(),
                     machine->Vdc().IrqLine() ? 1 : 0,
                     static_cast<unsigned long long>(
                         machine->Vdc().CompletedFrames()));
        std::fclose(f);
    }
    if (!trace_out.empty()) {
        std::FILE* f = std::fopen(trace_out.c_str(), "w");
        if (!f) {
            std::fprintf(stderr, "trace write failed: %s\n", trace_out.c_str());
            return 7;
        }
        const auto& prof = machine->Profile();
        const auto& tr = machine->Trace();
        std::fprintf(f, "# tick frame hpos vpos pc op addr data device\n");
        for (const o2em::ng::BusRecord& r : tr.Records()) {
            if (r.device != o2em::ng::BusDevice::Vdc &&
                r.device != o2em::ng::BusDevice::Vpp)
                continue;
            const std::int64_t fr = r.tick / frame_ticks;
            const std::int64_t ph = r.tick % frame_ticks;
            const int vpos = static_cast<int>(ph / prof.LineTicks());
            const int hpos =
                static_cast<int>((ph % prof.LineTicks()) / prof.pixel_ticks);
            std::fprintf(f, "%lld %lld %d %d %04X %s %02X %02X %s\n",
                         static_cast<long long>(r.tick),
                         static_cast<long long>(fr), hpos, vpos, r.pc,
                         o2em::ng::BusOpName(r.op), r.address, r.data,
                         o2em::ng::BusDeviceName(r.device));
        }
        std::fprintf(f, "# records=%llu truncated=%d\n",
                     static_cast<unsigned long long>(tr.Size()),
                     tr.Truncated() ? 1 : 0);
        std::fclose(f);
    }
    return 0;
}
