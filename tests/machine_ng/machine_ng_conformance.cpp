// O2EM-NG NG-2 machine conformance suite.
//
// Standalone: builds src/core_ng/ only. No SDL, frontend, database, legacy
// cpu.cpp or vmachine.cpp. Real BIOS/cartridge tests use local assets when
// present; deterministic synthetic fixtures are always exercised.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "machine.h"
#include "input/adapter.h"
#include "o2em_keys.h"
#include "special/special_hardware.h"

namespace {

int g_checks = 0;
int g_failures = 0;
int g_skips = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

void CheckEq(std::uint64_t actual, std::uint64_t expected, const char* what) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::printf("FAIL: %s (actual=%llu expected=%llu)\n", what,
                    static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
    }
}

struct NullHost final : o2em::ng::MachineHost {
    void PresentFrame(const o2em::ng::Framebuffer&) override {}
    void PushAudio(const std::int16_t*, std::size_t) override {}
    void OnFrameComplete(std::uint64_t) override {}
};

std::uint64_t Fnv1aBytes(std::uint64_t h, const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

bool ReadFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long n = std::ftell(f);
    if (n <= 0) {
        std::fclose(f);
        return false;
    }
    std::rewind(f);
    out.resize(static_cast<std::size_t>(n));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

std::string Join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '\\' || a.back() == '/') return a + b;
    return a + "\\" + b;
}

std::string Env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::vector<std::string> SearchRoots() {
    std::vector<std::string> roots;
    const std::string env = Env("O2EM_NG_ROOT");
    if (!env.empty()) roots.push_back(env);
    roots.push_back(".");
    roots.push_back("D:\\Project\\O2EM-NG");
    // Derive from this source file when the compiler recorded an absolute path.
    const std::string file = __FILE__;
    const std::size_t slash = file.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::string dir = file.substr(0, slash);
        roots.push_back(Join(Join(Join(dir, ".."), ".."), ".."));
    }
    return roots;
}

bool FindAsset(const std::string& relative) {
    std::vector<std::uint8_t> tmp;
    for (const auto& root : SearchRoots()) {
        if (ReadFile(Join(root, relative), tmp)) return true;
    }
    return false;
}

bool LoadAsset(const std::string& relative, std::vector<std::uint8_t>& out) {
    for (const auto& root : SearchRoots()) {
        if (ReadFile(Join(root, relative), out)) return true;
    }
    return false;
}

std::string LocalCartPath(const std::string& name) {
    const std::string env = Env("O2EM_NG_ROM_DIR");
    if (!env.empty()) {
        const std::string p = Join(env, name);
        std::vector<std::uint8_t> tmp;
        if (ReadFile(p, tmp)) return p;
    }
    const std::vector<std::string> dirs = {
        "D:\\LaunchBox\\Games\\Philips Videopac+",
        "Roms", "ROMS"
    };
    for (const auto& d : dirs) {
        const std::string p = Join(d, name);
        std::vector<std::uint8_t> tmp;
        if (ReadFile(p, tmp)) return p;
    }
    return {};
}

std::vector<std::uint8_t> MakeBytes(std::size_t size, std::uint8_t fill) {
    return std::vector<std::uint8_t>(size, fill);
}

bool LoadRealCart(const std::string& name, std::vector<std::uint8_t>& cart);
bool LoadRealBios(std::vector<std::uint8_t>& bios);

void PutJmp(std::vector<std::uint8_t>& m, std::uint16_t at,
            std::uint16_t target) {
    m[at] = static_cast<std::uint8_t>(0x04 | (((target >> 8) & 7) << 5));
    m[at + 1] = static_cast<std::uint8_t>(target & 0xFF);
}

o2em::ng::HardwareProfile TestProfile() {
    // Small deterministic profile: 100x100 pixels, VBL at line 50/h 89.
    auto p = o2em::ng::MakePalProfile();
    p.name = "test";
    p.htotal = 100;
    p.vtotal = 100;
    p.visible_w = 100;
    p.visible_h = 100;
    p.vblank_start = 50;
    p.vblank_end = 99;
    p.hblank_start = 90;
    p.hblank_end = 99;
    p.bgate_start = 95;
    p.pixel_ticks = 1;
    return p;
}

// ---------------------------------------------------------------------------
// Timebase and scheduler

void TestTimebase() {
    const auto pal = o2em::ng::MakePalProfile();
    const auto ntsc = o2em::ng::MakeNtscProfile();
    CheckEq(static_cast<std::uint64_t>(o2em::ng::kTicksPerCpuCycle), 180,
            "timebase: 180 ticks per CPU machine cycle");
    CheckEq(static_cast<std::uint64_t>(pal.LineTicks()), 4560,
            "timebase: PAL line ticks");
    CheckEq(static_cast<std::uint64_t>(pal.FrameTicks()), 1427280,
            "timebase: PAL frame ticks");
    CheckEq(static_cast<std::uint64_t>(pal.pixel_ticks), 10,
            "timebase: PAL pixel ticks");
    CheckEq(static_cast<std::uint64_t>(pal.VblankIrqOffset()), 1107170,
            "timebase: PAL VBL IRQ offset (line 242, h 365)");
    CheckEq(static_cast<std::uint64_t>(pal.FirstVblankIrqOffset()), 1426370,
            "timebase: PAL first VBL IRQ offset (line 312, h 365)");
    CheckEq(static_cast<std::uint64_t>(ntsc.FrameTicks()), 1076985,
            "timebase: NTSC frame ticks");
    CheckEq(static_cast<std::uint64_t>(ntsc.pixel_ticks), 9,
            "timebase: NTSC pixel ticks");
    CheckEq(static_cast<std::uint64_t>(ntsc.VblankIrqOffset()), 994275,
            "timebase: NTSC VBL IRQ offset (line 192, h 365)");
    CheckEq(static_cast<std::uint64_t>(ntsc.FirstVblankIrqOffset()), 1076175,
            "timebase: NTSC first VBL IRQ offset (line 262, h 365)");
    CheckEq(static_cast<std::uint64_t>(pal.cpu_hz.num), 17734476,
            "timebase: PAL CPU numerator");
    CheckEq(static_cast<std::uint64_t>(pal.cpu_hz.den), 3,
            "timebase: PAL CPU denominator");
    CheckEq(static_cast<std::uint64_t>(ntsc.cpu_hz.num), 10738635,
            "timebase: NTSC CPU numerator");
    CheckEq(static_cast<std::uint64_t>(ntsc.cpu_hz.den), 2,
            "timebase: NTSC CPU denominator");
    const auto c52 = o2em::ng::MakePalC52Profile();
    CheckEq(static_cast<std::uint64_t>(c52.FrameTicks()), 1427280,
            "timebase: C52 PAL frame ticks");
    CheckEq(static_cast<std::uint64_t>(c52.cpu_hz.num), 17812000,
            "timebase: C52 CPU numerator");
    CheckEq(static_cast<std::uint64_t>(c52.cpu_hz.den), 3,
            "timebase: C52 CPU denominator");
    Check(o2em::ng::CpuCyclesToTicks(3) == 540, "timebase: cycle conversion");
    Check(o2em::ng::MulDivFloor(1000, 3, 7) == 428,
          "timebase: rational floor");

    const double pal_rate = pal.pixel_hz.ToDouble() /
                            (double(pal.htotal) * double(pal.vtotal));
    const double ntsc_rate = ntsc.pixel_hz.ToDouble() /
                             (double(ntsc.htotal) * double(ntsc.vtotal));
    Check(pal_rate > 49.6 && pal_rate < 49.8, "timebase: PAL frame rate");
    Check(ntsc_rate > 59.7 && ntsc_rate < 59.9, "timebase: NTSC frame rate");
}

void TestSchedulerOrdering() {
    o2em::ng::Scheduler s;
    std::vector<int> order;
    s.Register(o2em::ng::HandlerId::Heartbeat,
               [&](const o2em::ng::SchedulerEvent& e) {
                   order.push_back(static_cast<int>(e.payload));
               });
    s.Schedule(100, o2em::ng::EventPriority::Video, o2em::ng::EventKind::VblankStart,
               o2em::ng::HandlerId::Heartbeat, 1);
    s.Schedule(50, o2em::ng::EventPriority::Irq, o2em::ng::EventKind::IrqTransition,
               o2em::ng::HandlerId::Heartbeat, 2);
    s.Schedule(50, o2em::ng::EventPriority::Video, o2em::ng::EventKind::VblankStart,
               o2em::ng::HandlerId::Heartbeat, 3);
    s.Schedule(50, o2em::ng::EventPriority::Video, o2em::ng::EventKind::VblankStart,
               o2em::ng::HandlerId::Heartbeat, 4);
    s.Schedule(200, o2em::ng::EventPriority::Audio, o2em::ng::EventKind::AudioTick,
               o2em::ng::HandlerId::Heartbeat, 5);
    const std::size_t n = s.AdvanceTo(100);
    CheckEq(n, 4, "scheduler: four due events");
    Check(order.size() == 4 && order[0] == 2 && order[1] == 3 &&
              order[2] == 4 && order[3] == 1,
          "scheduler: (tick, priority, sequence) order");
    CheckEq(static_cast<std::uint64_t>(s.Now()), 100, "scheduler: now advance");
    s.AdvanceTo(300);
    Check(order.size() == 5 && order[4] == 5, "scheduler: later event");
}

void TestSchedulerSnapshot() {
    o2em::ng::Scheduler s;
    std::vector<int> order;
    s.Register(o2em::ng::HandlerId::Heartbeat,
               [&](const o2em::ng::SchedulerEvent& e) {
                   order.push_back(static_cast<int>(e.payload));
               });
    s.Schedule(10, o2em::ng::EventPriority::Cpu, o2em::ng::EventKind::CpuBusAccess,
               o2em::ng::HandlerId::Heartbeat, 7);
    s.Schedule(20, o2em::ng::EventPriority::Input, o2em::ng::EventKind::InputTransition,
               o2em::ng::HandlerId::Heartbeat, 8);
    o2em::ng::SnapshotWriter w;
    Check(s.SaveState(w), "scheduler snapshot: save");
    s.Clear();
    CheckEq(static_cast<std::uint64_t>(s.PendingCount()), 0,
            "scheduler snapshot: cleared");
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(s.LoadState(r), "scheduler snapshot: restore");
    CheckEq(static_cast<std::uint64_t>(s.PendingCount()), 2,
            "scheduler snapshot: pending restored");
    s.AdvanceTo(30);
    Check(order.size() == 2 && order[0] == 7 && order[1] == 8,
          "scheduler snapshot: replay order");
}

// ---------------------------------------------------------------------------
// Cartridge and mapper

void TestCartridgeClassification() {
    const std::vector<std::size_t> sizes = {0x0800, 0x1000, 0x2000, 0x8000,
                                            0x0C00, 0x1800, 0x3000, 0x4000};
    const std::vector<o2em::ng::CartridgeKind> kinds = {
        o2em::ng::CartridgeKind::Normal2K, o2em::ng::CartridgeKind::Normal4K,
        o2em::ng::CartridgeKind::Normal8K, o2em::ng::CartridgeKind::Normal32K,
        o2em::ng::CartridgeKind::ThreeK3K, o2em::ng::CartridgeKind::ThreeK6K,
        o2em::ng::CartridgeKind::ThreeK12K, o2em::ng::CartridgeKind::Rally16K};
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        o2em::ng::Cartridge c;
        Check(c.Load(MakeBytes(sizes[i], 0xA5), "x"), "cart: load");
        Check(c.info.kind == kinds[i], "cart: kind classification");
        CheckEq(c.info.size, sizes[i], "cart: size");
        CheckEq(c.info.crc32, o2em::ng::Crc32(c.rom.data(), c.rom.size()),
                "cart: crc identity");
    }
    o2em::ng::Cartridge bad;
    Check(bad.Load(MakeBytes(0x10000, 0), "bad"), "cart: load unsupported");
    Check(bad.info.kind == o2em::ng::CartridgeKind::Unsupported,
          "cart: unsupported is explicit");
    Check(!o2em::ng::MakeMapper(bad), "cart: unsupported has no mapper");
}

void TestStandardMapper() {
    std::vector<std::uint8_t> rom(0x1000);
    for (std::size_t i = 0; i < 0x800; ++i) {
        rom[i] = static_cast<std::uint8_t>(0x10 + (i & 0x0F));
        rom[0x800 + i] = static_cast<std::uint8_t>(0x20 + (i & 0x0F));
    }
    o2em::ng::Cartridge cart;
    cart.Load(rom, "4k");
    auto mapper = o2em::ng::MakeMapper(cart);
    Check(mapper != nullptr, "standard mapper: create");
    mapper->Reset(cart);
    CheckEq(mapper->Bank(), 0, "standard mapper: reset bank");
    CheckEq(mapper->ReadProgram(0x400), rom[0x000], "standard mapper: bank0 400");
    CheckEq(mapper->ReadProgram(0x800), rom[0x400], "standard mapper: bank0 800");
    CheckEq(mapper->ReadProgram(0xC00), rom[0x400], "standard mapper: 0C00 mirror");
    CheckEq(mapper->ReadProgram(0xBFF), rom[0x7FF], "standard mapper: bank0 BFF");
    mapper->WriteP1(0x01);
    CheckEq(mapper->Bank(), 1, "standard mapper: P11 bank");
    CheckEq(mapper->ReadProgram(0x400), rom[0x800], "standard mapper: bank1 400");
    CheckEq(mapper->ReadProgram(0xC00), rom[0xC00], "standard mapper: bank1 C00");
    mapper->WriteP1(0x02);
    CheckEq(mapper->Bank(), 2, "standard mapper: P11+P10 bank");
    CheckEq(mapper->ReadProgram(0x400), rom[0x000], "standard mapper: 4K bank alias");
}

void TestEightKMapper() {
    std::vector<std::uint8_t> rom(0x2000);
    for (std::size_t i = 0; i < rom.size(); ++i)
        rom[i] = static_cast<std::uint8_t>((i >> 11) * 0x40 + (i & 0x3F));
    o2em::ng::Cartridge cart;
    cart.Load(rom, "8k");
    auto mapper = o2em::ng::MakeMapper(cart);
    mapper->Reset(cart);
    for (int bank = 0; bank < 4; ++bank) {
        mapper->WriteP1(static_cast<std::uint8_t>(bank));
        CheckEq(mapper->ReadProgram(0x400), rom[bank * 0x800],
                "8K mapper: bank base");
        CheckEq(mapper->ReadProgram(0x7FF), rom[bank * 0x800 + 0x3FF],
                "8K mapper: bank end");
    }
}

void Test32KMapper() {
    std::vector<std::uint8_t> rom(0x8000);
    for (std::size_t i = 0; i < rom.size(); ++i)
        rom[i] = static_cast<std::uint8_t>((i >> 11) * 0x10 + (i & 0x0F));
    o2em::ng::Cartridge cart;
    cart.Load(rom, "32k");
    Check(cart.info.kind == o2em::ng::CartridgeKind::Normal32K,
          "32K mapper: kind");
    auto mapper = o2em::ng::MakeMapper(cart);
    mapper->Reset(cart);
    mapper->WriteP1(0x03);
    CheckEq(mapper->ReadProgram(0x400), rom[0x1800], "32K mapper: bank3 base");
    CheckEq(mapper->ReadProgram(0xC00), rom[0x1C00], "32K mapper: bank3 upper");
}

void TestThreeKMapper() {
    const std::size_t pages = 4;
    std::vector<std::uint8_t> rom(pages * 0xC00);
    for (std::size_t p = 0; p < pages; ++p)
        for (std::size_t i = 0; i < 0xC00; ++i)
            rom[p * 0xC00 + i] = static_cast<std::uint8_t>((p << 4) | (i & 0x0F));
    o2em::ng::Cartridge cart;
    cart.Load(rom, "12k");
    Check(cart.info.kind == o2em::ng::CartridgeKind::ThreeK12K,
          "3K mapper: kind");
    auto mapper = o2em::ng::MakeMapper(cart);
    mapper->Reset(cart);
    for (int bank = 0; bank < 4; ++bank) {
        mapper->WriteP1(static_cast<std::uint8_t>(bank));
        CheckEq(mapper->ReadProgram(0x400), rom[bank * 0xC00],
                "3K mapper: page base");
        CheckEq(mapper->ReadProgram(0x7FF), rom[bank * 0xC00 + 0x3FF],
                "3K mapper: page mid");
        CheckEq(mapper->ReadProgram(0xC00), rom[bank * 0xC00 + 0x800],
                "3K mapper: page upper");
    }
}

void TestRallyMapper() {
    std::vector<std::uint8_t> rom(0x4000);
    for (std::size_t i = 0; i < rom.size(); ++i)
        rom[i] = static_cast<std::uint8_t>((i >> 11) * 0x20 + (i & 0x1F));
    o2em::ng::Cartridge cart;
    cart.Load(rom, "16k");
    Check(cart.info.kind == o2em::ng::CartridgeKind::Rally16K,
          "rally mapper: kind");
    auto mapper = o2em::ng::MakeMapper(cart);
    mapper->Reset(cart);
    mapper->WriteP1(0x00); // P10=0, P14=0: latch enabled
    CheckEq(mapper->Control(), 0x00, "rally mapper: control");
    mapper->IoWrite(0x80, 0x01);
    CheckEq(mapper->Bank(), 1, "rally mapper: latch bank");
    CheckEq(mapper->ReadProgram(0x400), rom[0x0800], "rally mapper: bank1 400");
    mapper->WriteP1(0x01); // P10=1 forces last 8K bank
    CheckEq(mapper->ReadProgram(0x400), rom[0x3800], "rally mapper: forced bank");
    mapper->WriteP1(0x00);
    mapper->IoWrite(0x00, 0x02); // A7=0: no latch write
    CheckEq(mapper->Bank(), 1, "rally mapper: A7 required");
}

void TestFourInOneMapper() {
    std::vector<std::uint8_t> rom(0x1000);
    for (std::size_t i = 0; i < rom.size(); ++i)
        rom[i] = static_cast<std::uint8_t>((i >> 8) * 0x20 + (i & 0x1F));
    o2em::ng::Cartridge cart;
    cart.Load(rom, "4in1");
    // Synthetic layout: force the cartridge hardware class. The automatic
    // classification is validated with the real VP40 image below.
    cart.info.kind = o2em::ng::CartridgeKind::FourInOne4K;
    auto mapper = o2em::ng::MakeMapper(cart);
    Check(mapper != nullptr, "4in1 mapper: create");
    mapper->Reset(cart);
    CheckEq(mapper->ReadProgram(0x400), rom[0x400], "4in1: program 400");
    CheckEq(mapper->ReadProgram(0xBFF), rom[0xBFF], "4in1: program BFF");
    CheckEq(mapper->ReadProgram(0xC00), rom[0xC00], "4in1: program C00");
    CheckEq(mapper->ReadProgram(0xFFF), rom[0xFFF], "4in1: program FFF");
    mapper->WriteP1(0x00);
    CheckEq(mapper->IoRead(0x10), 0xFF, "4in1: data ROM disabled");
    mapper->WriteP1(0x02);
    mapper->WriteP2(0x03);
    CheckEq(mapper->IoRead(0x10), rom[(3u << 8) | 0x10],
            "4in1: P2-banked data ROM");
    mapper->WriteP2(0x0F);
    CheckEq(mapper->IoRead(0x20), rom[(15u << 8) | 0x20],
            "4in1: P2 low nibble selects data page");
    o2em::ng::SnapshotWriter w;
    mapper->Snapshot(w);
    mapper->WriteP1(0x00);
    mapper->WriteP2(0x00);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(mapper->Restore(r), "4in1 mapper: restore");
    CheckEq(mapper->Control(), 0x02, "4in1 mapper: control restored");
    CheckEq(mapper->Bank(), 0x0F, "4in1 mapper: bank restored");

    std::vector<std::uint8_t> real;
    if (!LoadRealCart("vp_40.bin", real)) {
        ++g_skips;
        std::printf("SKIP: vp_40.bin not present for 4in1 classification\n");
        return;
    }
    o2em::ng::Cartridge c;
    Check(c.Load(std::move(real), "vp_40.bin"), "4in1: VP40 loads");
    Check(c.info.kind == o2em::ng::CartridgeKind::FourInOne4K,
          "4in1: VP40 hardware class");
    auto m = o2em::ng::MakeMapper(c);
    Check(m != nullptr, "4in1: VP40 mapper");
    m->Reset(c);
    CheckEq(m->ReadProgram(0x400), c.rom[0x400],
            "4in1: VP40 program entry is upper 3KB");
}

void TestMapperSnapshot() {
    std::vector<std::uint8_t> rom(0x2000, 0x33);
    o2em::ng::Cartridge cart;
    cart.Load(rom, "8k");
    auto mapper = o2em::ng::MakeMapper(cart);
    mapper->Reset(cart);
    mapper->WriteP1(0x03);
    o2em::ng::SnapshotWriter w;
    mapper->Snapshot(w);
    mapper->WriteP1(0x00);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(mapper->Restore(r), "mapper snapshot: restore");
    CheckEq(mapper->Bank(), 3, "mapper snapshot: bank restored");
}

// ---------------------------------------------------------------------------
// Machine bus decode

struct BusFixture {
    o2em::ng::HardwareProfile profile = o2em::ng::MakePalProfile();
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    o2em::ng::Cartridge cart;
    std::unique_ptr<o2em::ng::Mapper> mapper;
    o2em::ng::Vdc vdc;
    o2em::ng::MachineInput input;
    o2em::ng::BusTrace trace;
    o2em::ng::SpecialHardware* special = nullptr;
    o2em::ng::MachineBus bus;

    void Init(std::vector<std::uint8_t> rom) {
        cart.Load(std::move(rom), "test");
        mapper = o2em::ng::MakeMapper(cart);
        if (mapper) mapper->Reset(cart);
        vdc.Configure(profile);
        bus.Configure(profile, &bios, &cart, mapper.get(), &vdc, nullptr, &input,
                      &trace, special);
        bus.PowerOn();
        bus.SetAccessContext(0, 0);
    }

    void P1(std::uint8_t v) { bus.PortWrite(1, v); }
    void P2(std::uint8_t v) { bus.PortWrite(2, v); }
};

void TestProgramDecode() {
    BusFixture f;
    f.Init(MakeBytes(0x0800, 0x00));
    for (int i = 0; i < 0x400; ++i) f.bios[i] = static_cast<std::uint8_t>(i);
    f.cart.rom[0] = 0x11;
    f.cart.rom[0x3FF] = 0x22;
    f.cart.rom[0x400] = 0x33;
    f.cart.rom[0x7FF] = 0x44;
    CheckEq(f.bus.ProgramRead(0x000), 0x00, "program: BIOS start");
    CheckEq(f.bus.ProgramRead(0x3FF), 0xFF, "program: BIOS end");
    CheckEq(f.bus.ProgramRead(0x400), 0x11, "program: cart 400");
    CheckEq(f.bus.ProgramRead(0x7FF), 0x22, "program: cart 7FF");
    CheckEq(f.bus.ProgramRead(0x800), 0x33, "program: cart 800");
    CheckEq(f.bus.ProgramRead(0xBFF), 0x44, "program: cart BFF");
    CheckEq(f.bus.ProgramRead(0xC00), 0x33, "program: 2K C00 mirror");
    CheckEq(f.bus.ProgramRead(0xFFF), 0x44, "program: 2K FFF mirror");

    f.Init(MakeBytes(0x1000, 0x00));
    f.cart.rom[0x000] = 0x10;
    f.cart.rom[0x800] = 0x20;
    f.cart.rom[0xC00] = 0x30;
    CheckEq(f.bus.ProgramRead(0x400), 0x10, "program: 4K bank0");
    f.P1(0x01);
    CheckEq(f.bus.ProgramRead(0x400), 0x20, "program: 4K bank1");
    CheckEq(f.bus.ProgramRead(0xC00), 0x30, "program: 4K bank1 C00");
    f.P1(0xFF);
    CheckEq(f.bus.ProgramRead(0x000), 0x00, "program: BIOS immune to P1");

    f.Init(MakeBytes(0x3000, 0x00));
    f.cart.rom[0x000] = 0xA0;
    f.cart.rom[0x0800] = 0xB0;
    f.cart.rom[0x2400] = 0xC0;
    f.cart.rom[0x2C00] = 0xD0;
    f.P1(0x03);
    CheckEq(f.bus.ProgramRead(0x400), 0xC0, "program: 3K bank3");
    CheckEq(f.bus.ProgramRead(0xC00), 0xD0, "program: 3K bank3 upper");
    f.P1(0x00);
    CheckEq(f.bus.ProgramRead(0xC00), 0xB0, "program: 3K bank0 upper");
}

void TestExternalRamAndOpenBus() {
    BusFixture f;
    f.Init(MakeBytes(0x0800, 0x00));
    f.P1(0xAF); // P16=0, P14=0, P13=1
    f.bus.DataWrite(0x00, 0x12);
    f.bus.DataWrite(0x7F, 0x34);
    f.bus.DataWrite(0x80, 0x56);
    f.bus.DataWrite(0xFF, 0x78);
    CheckEq(f.bus.DataRead(0x00), 0x12, "extRAM: 0x00");
    CheckEq(f.bus.DataRead(0x7F), 0x34, "extRAM: 0x7F");
    CheckEq(f.bus.DataRead(0x80), 0xFF, "extRAM: 0x80 not owned");
    CheckEq(f.bus.DataRead(0xFF), 0xFF, "extRAM: 0xFF not owned");
    CheckEq(f.bus.ExternalRam()[0x00], 0x12, "extRAM: byte 0");
    CheckEq(f.bus.ExternalRam()[0x7F], 0x34, "extRAM: byte 7F");

    f.P1(0xBF); // P14=1
    CheckEq(f.bus.DataRead(0x00), 0xFF, "extRAM: P14 high deselects read");
    f.P1(0xEF); // P14=0, P16=1
    CheckEq(f.bus.DataRead(0x00), 0x12, "extRAM: read not gated by P16");
    f.bus.DataWrite(0x00, 0x99);
    CheckEq(f.bus.DataRead(0x00), 0x12, "extRAM: write gated by P16");

    f.P1(0xFF);
    CheckEq(f.bus.DataRead(0x00), 0xFF, "open bus: deterministic 0xFF");
    CheckEq(f.bus.DataRead(0x80), 0xFF, "open bus: high address 0xFF");

    // VDC AND with open bus/cartridge. A0 is a readable stored register.
    f.P1(0xA7); // P13=0, P16=0
    f.bus.DataWrite(0xA0, 0x20);
    CheckEq(f.bus.DataRead(0xA0), 0x20, "vdc: A0 read");
    f.bus.DataWrite(0xA3, 0x0F);
    CheckEq(f.bus.DataRead(0xA3), 0x00, "vdc: write-only A3 reads 0");
    f.P1(0xBF); // VDC deselected
    CheckEq(f.bus.DataRead(0xA0), 0xFF, "vdc: deselected open bus");
}

void TestP1P2Decode() {
    BusFixture f;
    f.Init(MakeBytes(0x0800, 0x00));
    f.P1(0x5A);
    CheckEq(f.bus.P1(), 0x5A, "P1: latch");
    CheckEq(f.bus.PortRead(1), 0x5A, "P1: read pins");
    CheckEq(static_cast<std::uint64_t>(f.bus.DecodeMovxDevice(0x00, false)),
            static_cast<std::uint64_t>(o2em::ng::BusDevice::OpenBus),
            "P1 decode: no select");

    f.P1(0xFB); // P12=0, P16=1
    f.P2(0x00);
    f.input.keyboard.rows[0] = 0x01;
    CheckEq(f.bus.PortRead(2), 0xEF, "P2: keyboard bit0");
    f.input.keyboard.rows[0] = 0x80;
    CheckEq(f.bus.PortRead(2), 0x0F, "P2: keyboard bit7");
    f.input.keyboard.rows[0] = 0x00;
    CheckEq(f.bus.PortRead(2), 0xFF, "P2: no key");
    f.P1(0xFF);
    f.input.keyboard.rows[0] = 0x01;
    CheckEq(f.bus.PortRead(2), 0xFF, "P2: P12 high disables matrix");

    f.input = o2em::ng::MachineInput{};
    f.P1(0xFB);
    f.P2(0x00);
    f.input.p1.up = true;
    f.input.p1.fire = true;
    CheckEq(f.bus.BusRead(), 0xEE, "BUS: joystick 1 active-low");
    f.P2(0x01);
    f.input.p2.left = true;
    CheckEq(f.bus.BusRead(), 0xF7, "BUS: joystick 2");
    f.P2(0x02);
    CheckEq(f.bus.BusRead(), 0xFF, "BUS: select 2 has no joystick");
    f.P1(0xFF);
    CheckEq(f.bus.BusRead(), 0xFF, "BUS: P12 high");

    f.P1(0x00);
    f.P2(0x0A);
    CheckEq(f.bus.P2(), 0x0A, "P2: latch");
}

struct FakeSpecialHardware final : o2em::ng::SpecialHardware {
    int power_ons = 0;
    int resets = 0;
    int p1_writes = 0;
    int frame_ticks = 0;
    unsigned last_cycles = 0;
    unsigned last_frame_cycles = 0;
    std::uint8_t last_p1 = 0xFF;
    int external_writes = 0;
    std::uint8_t last_write_address = 0;
    std::uint8_t last_write_value = 0;

    void PowerOn() override { ++power_ons; }
    void Reset() override { ++resets; }
    void WriteP1(std::uint8_t value) override {
        ++p1_writes;
        last_p1 = value;
    }
    bool ExternalRead(std::uint8_t address, std::uint8_t& value) override {
        if (address != 0xA0) return false;
        value = 0x42;
        return true;
    }
    bool ExternalWrite(std::uint8_t address, std::uint8_t value) override {
        ++external_writes;
        last_write_address = address;
        last_write_value = value;
        return address >= 0x80;
    }
    void RunCpuCycles(unsigned cycles, unsigned frame_cycles) override {
        last_cycles = cycles;
        last_frame_cycles = frame_cycles;
    }
    void FrameTick() override { ++frame_ticks; }
};

void TestSpecialHardwareBus() {
    BusFixture f;
    FakeSpecialHardware special;
    f.special = &special;
    f.Init(MakeBytes(0x0800, 0x00));

    f.P1(0x00); // P10/P14 select the special module; P13=1
    CheckEq(special.last_p1, 0x00, "special: P1 forwarded");
    Check(special.p1_writes >= 1, "special: P1 write observed");

    // A claimed write suppresses the normal bus when P13 is high.
    f.bus.DataWrite(0x80, 0x5A);
    CheckEq(special.external_writes, 1, "special: write forwarded");
    CheckEq(special.last_write_address, 0x80, "special: write address");
    CheckEq(special.last_write_value, 0x5A, "special: write value");

    // A claimed read is returned directly.
    CheckEq(f.bus.DataRead(0xA0), 0x42, "special: claimed read value");
    // An unclaimed address keeps the normal path.
    f.P1(0xFF);
    CheckEq(f.bus.DataRead(0x10), 0xFF, "special: unclaimed read open bus");
}

void TestMovxRouting() {
    BusFixture f;
    f.Init(MakeBytes(0x1000, 0x00));
    f.cart.rom[0] = 0xAA;
    f.P1(0xAF); // P16=0, P14=0, P13=1, P12=1
    f.bus.DataWrite(0x20, 0x5A);
    CheckEq(f.bus.DataRead(0x20), 0x5A, "MOVX: external RAM routing");
    CheckEq(f.bus.ExternalRam()[0x20], 0x5A, "MOVX: RAM side effect");

    f.P1(0xA7); // P13=0, P14=0, P16=0
    f.bus.DataWrite(0xA0, 0x20);
    CheckEq(f.vdc.Control(), 0x20, "MOVX: VDC A0 write");
    CheckEq(f.bus.DataRead(0xA0), 0x20, "MOVX: VDC A0 read");
    CheckEq(f.bus.ExternalRam()[0x20], 0x5A, "MOVX: A7=1 excludes RAM");

    f.P1(0xBF); // P14=1, P13=1, P16=0
    f.bus.DataWrite(0x80, 0x33);
    CheckEq(f.bus.DataRead(0x80), 0xFF, "MOVX: cart I/O read");

    f.P1(0xAF);
    f.P2(0x00);
    f.trace.Reset();
    f.trace.SetEnabled(true);
    f.trace.SetFilter({0, INT64_MAX, 0, 0xFFF, 0xFFFF, 64, true});
    f.bus.SetAccessContext(static_cast<o2em::ng::Tick>(10 * o2em::ng::kTicksPerCpuCycle), 0x400);
    f.bus.DataWrite(0x10, 0x44);
    f.bus.DataRead(0x10);
    Check(f.trace.Size() == 2, "MOVX trace: two records");
    if (f.trace.Size() == 2) {
        Check(f.trace.Records()[0].op == o2em::ng::BusOp::MovxWrite,
              "MOVX trace: write op");
        CheckEq(f.trace.Records()[0].tick,
                static_cast<std::uint64_t>(10 * o2em::ng::kTicksPerCpuCycle),
                "MOVX trace: tick");
        CheckEq(f.trace.Records()[0].p1, 0xAF, "MOVX trace: P1");
        CheckEq(f.trace.Records()[0].cpu_cycle, 10, "MOVX trace: cpu cycle");
        CheckEq(static_cast<std::uint64_t>(f.trace.Records()[0].device),
                static_cast<std::uint64_t>(o2em::ng::BusDevice::ExtRam),
                "MOVX trace: device");
    }
}

// ---------------------------------------------------------------------------
// Machine lifecycle and full-machine tests

struct MachineFixture {
    NullHost host;
    o2em::ng::MachineConfig config;
    std::unique_ptr<o2em::ng::Machine> machine;

    explicit MachineFixture(bool small = false) {
        config.region = o2em::ng::Region::Pal;
        if (small) {
            config.use_custom_profile = true;
            config.custom_profile = TestProfile();
        }
        machine = o2em::ng::Machine::Create(config, host);
    }
};

void TestSpecialHardwareMachineLifecycle() {
    MachineFixture f(true);
    auto special = std::make_unique<FakeSpecialHardware>();
    FakeSpecialHardware* observed = special.get();
    f.machine->AttachSpecialHardware(std::move(special));
    Check(f.machine->LoadBiosBytes(MakeBytes(0x400, 0x00), "special-bios"),
          "special: BIOS load");
    Check(f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "special-cart"),
          "special: cart load");
    f.machine->PowerOn();
    CheckEq(observed->power_ons, 1, "special: power-on forwarded");
    f.machine->RunInstructions(4);
    Check(observed->last_cycles > 0, "special: CPU cycles interleaved");
    Check(observed->last_frame_cycles > 0, "special: frame budget provided");
    f.machine->RunFrame();
    CheckEq(observed->frame_ticks, 1, "special: frame tick forwarded");
    f.machine->Reset();
    CheckEq(observed->resets, 1, "special: reset forwarded");
}


void TestMovxCpuRouting() {
    MachineFixture f(true);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    const std::uint8_t prog[] = {
        0x23, 0xAF, 0x39,             // MOV A,#AF; OUTL P1,A
        0xB8, 0x10, 0xB9, 0x20,       // MOV R0,#10; MOV R1,#20
        0x23, 0x11, 0x90,             // MOV A,#11; MOVX @R0,A
        0x23, 0x22, 0x91,             // MOV A,#22; MOVX @R1,A
        0x80, 0xAA,                   // MOVX A,@R0; MOV R2,A
        0x81, 0xAB,                   // MOVX A,@R1; MOV R3,A
        0x04, 0x20                    // JMP 0x020
    };
    // Correct the final JMP page/address (0x04,0x20 -> 0x020).
    for (std::size_t i = 0; i < sizeof(prog); ++i) bios[0x020 + i] = prog[i];
    f.machine->LoadBiosBytes(std::move(bios), "movx-cpu");
    f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    f.machine->PowerOn();
    f.machine->RunInstructions(30);
    CheckEq(f.machine->Bus().ExternalRam()[0x10], 0x11,
            "MOVX CPU: @R0 write");
    CheckEq(f.machine->Bus().ExternalRam()[0x20], 0x22,
            "MOVX CPU: @R1 write");
    CheckEq(f.machine->CpuState().ram[2], 0x11, "MOVX CPU: A,@R0 read");
    CheckEq(f.machine->CpuState().ram[3], 0x22, "MOVX CPU: A,@R1 read");
}

void TestPalNtscProfiles() {
    for (o2em::ng::Region region : {o2em::ng::Region::Pal,
                                    o2em::ng::Region::Ntsc}) {
        MachineFixture f;
        f.config.region = region;
        f.machine = o2em::ng::Machine::Create(f.config, f.host);
        f.machine->LoadBiosBytes(MakeBytes(0x400, 0x00), "synthetic");
        f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
        f.machine->PowerOn();
        const auto& p = f.machine->Profile();
        CheckEq(static_cast<std::uint64_t>(p.FrameTicks()),
                region == o2em::ng::Region::Pal ? 1427280u : 1076985u,
                "profile: frame ticks");
        const auto info = f.machine->GetInfo();
        Check(info.active_region == region, "profile: active region");
        f.machine->RunFrame();
        Check(f.machine->Now() >= p.FrameTicks(), "profile: frame 1");
        f.machine->RunFrame();
        Check(f.machine->Now() >= 2 * p.FrameTicks(), "profile: frame 2");
    }
}

void TestPowerOnReset() {
    MachineFixture f;
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x400);
    // Cartridge program: set P1=AF; R0=10; A=5A; MOVX @R0,A; loop.
    std::vector<std::uint8_t> cart = MakeBytes(0x800, 0x00);
    const std::uint8_t prog[] = {0x23, 0xAF, 0x39, 0xB8, 0x30,
                                 0x23, 0x5A, 0xA0, 0xB8, 0x10,
                                 0x90, 0x84, 0x0A};
    for (std::size_t i = 0; i < sizeof(prog); ++i) cart[i] = prog[i];
    f.machine->LoadBiosBytes(std::move(bios), "synthetic");
    f.machine->LoadCartridgeBytes(std::move(cart), "cart");
    f.machine->PowerOn();
    f.machine->RunInstructions(20);
    CheckEq(f.machine->Bus().ExternalRam()[0x10], 0x5A,
            "lifecycle: extRAM written by program");
    CheckEq(f.machine->CpuState().ram[0x30], 0x5A,
            "lifecycle: internal RAM written by program");
    const o2em::ng::Tick now_before = f.machine->Now();
    const std::uint64_t cycles_before = f.machine->CpuState().cycles;
    f.machine->Reset();
    CheckEq(f.machine->Now(), now_before, "reset: machine time preserved");
    CheckEq(f.machine->CpuState().cycles, cycles_before, "reset: cycle count preserved");
    CheckEq(f.machine->CpuState().p1, 0xFF, "reset: P1 input mode");
    CheckEq(f.machine->CpuState().p2, 0xFF, "reset: P2 input mode");
    CheckEq(f.machine->CpuState().pc, 0x000, "reset: PC zero");
    CheckEq(f.machine->Bus().ExternalRam()[0x10], 0x5A,
            "reset: external RAM preserved");
    CheckEq(f.machine->CpuState().ram[0x30], 0x5A,
            "reset: internal RAM preserved");
    f.machine->PowerOn();
    CheckEq(f.machine->Now(), 0, "power-on: time cleared");
    CheckEq(f.machine->CpuState().cycles, 0, "power-on: cycles cleared");
    CheckEq(f.machine->Bus().ExternalRam()[0x10], 0x00,
            "power-on: external RAM cleared");
    CheckEq(f.machine->CpuState().ram[0x30], 0x00,
            "power-on: internal RAM cleared");
}

void TestIrqIntegration() {
    MachineFixture f(true);
    const auto prof = f.machine->Profile();
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    PutJmp(bios, 0x003, 0x010);
    // Handler: A=AA; R7=A; INC R6; R0=A1; MOVX A,@R0; RETR
    const std::uint8_t handler[] = {0x23, 0xAA, 0xAF, 0x1E, 0xB8, 0xA1, 0x80, 0x93};
    for (std::size_t i = 0; i < sizeof(handler); ++i) bios[0x010 + i] = handler[i];
    bios[0x020] = 0x23; bios[0x021] = 0xA7; // MOV A,#A7 (VDC CS)
    bios[0x022] = 0x39; // OUTL P1,A
    bios[0x023] = 0x05; // EN I
    PutJmp(bios, 0x024, 0x024);
    f.machine->LoadBiosBytes(std::move(bios), "irq");
    f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    f.machine->PowerOn();
    f.machine->RunUntil(prof.FirstVblankIrqOffset() - o2em::ng::kTicksPerCpuCycle);
    Check(!f.machine->CpuState().irq_in_service,
          "IRQ: not in service before VBL");
    CheckEq(f.machine->CpuState().ram[7], 0x00, "IRQ: handler not yet run");
    f.machine->RunUntil(prof.FirstVblankIrqOffset() +
                        30 * o2em::ng::kTicksPerCpuCycle);
    CheckEq(f.machine->CpuState().ram[7], 0xAA, "IRQ: handler ran");
    CheckEq(f.machine->CpuState().ram[6], 0x01, "IRQ: handler count 1");
    Check(!f.machine->Bus().IrqLine(), "IRQ: A1 read cleared VDC line");
    Check(!f.machine->CpuState().irq_in_service, "IRQ: RETR left service");
    f.machine->RunUntil(prof.FrameTicks() + prof.VblankIrqOffset() +
                        30 * o2em::ng::kTicksPerCpuCycle);
    CheckEq(f.machine->CpuState().ram[6], 0x02, "IRQ: second VBL serviced");
    Check(!f.machine->Bus().IrqLine(), "IRQ: second line cleared");
}

void TestIrqHeldAcrossDisable() {
    // Dedicated profile: a long frame with VBL early enough to fall inside a
    // ~770 machine-cycle DIS I delay loop, and far from the next VBL.
    o2em::ng::HardwareProfile hp = o2em::ng::MakePalProfile();
    hp.name = "irq-held";
    hp.htotal = 100;
    hp.vtotal = 100;
    hp.visible_w = 100;
    hp.visible_h = 100;
    hp.pixel_ticks = o2em::ng::kTicksPerCpuCycle;
    hp.vblank_start = 5;
    hp.vblank_end = 50;
    hp.hblank_start = 90;
    hp.hblank_end = 99;
    hp.bgate_start = 95;
    NullHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    config.use_custom_profile = true;
    config.custom_profile = hp;
    auto machine = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    PutJmp(bios, 0x003, 0x010);
    const std::uint8_t handler[] = {0x23, 0xAA, 0xAF, 0x1E, 0xB8,
                                    0xA1, 0x80, 0x93};
    for (std::size_t i = 0; i < sizeof(handler); ++i) bios[0x010 + i] = handler[i];
    bios[0x020] = 0x23; bios[0x021] = 0xA7; // MOV A,#A7 (VDC+RAM)
    bios[0x022] = 0x39; // OUTL P1,A
    bios[0x023] = 0x15; // DIS I
    bios[0x024] = 0xBA; bios[0x025] = 0xFF; // MOV R2,#FF
    bios[0x026] = 0x00; // NOP
    bios[0x027] = 0xEA; bios[0x028] = 0x26; // DJNZ R2,0x026
    bios[0x029] = 0x05; // EN I
    PutJmp(bios, 0x02A, 0x02A);
    machine->LoadBiosBytes(std::move(bios), "irq-held");
    machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    machine->PowerOn();
    // VBL at line 5 -> 106020 ticks, inside the ~770-cycle DIS I loop.
    machine->RunUntil(120000);
    CheckEq(machine->CpuState().ram[6], 0x00,
            "IRQ held: DIS I suppressed service");
    Check(machine->Bus().IrqLine(), "IRQ held: line asserted");
    machine->RunUntil(150000);
    CheckEq(machine->CpuState().ram[6], 0x01,
            "IRQ held: serviced after EN I");
    Check(!machine->Bus().IrqLine(), "IRQ held: line cleared by A1");
}

void TestTimerIrqIntegration() {
    MachineFixture f(true);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    PutJmp(bios, 0x007, 0x030);
    bios[0x020] = 0x15; // DIS I
    bios[0x021] = 0x25; // EN TCNTI
    bios[0x022] = 0x23; bios[0x023] = 0xFE; // MOV A,#FE
    bios[0x024] = 0x62; // MOV T,A
    bios[0x025] = 0x55; // STRT T
    PutJmp(bios, 0x026, 0x026);
    bios[0x030] = 0x23; bios[0x031] = 0x55; // MOV A,#55
    bios[0x032] = 0xAE; // MOV R6,A
    bios[0x033] = 0x93; // RETR
    f.machine->LoadBiosBytes(std::move(bios), "timer-irq");
    f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    f.machine->PowerOn();
    f.machine->RunCycles(200);
    CheckEq(f.machine->CpuState().ram[6], 0x55, "timer IRQ: handler ran");
    Check(!f.machine->CpuState().irq_in_service, "timer IRQ: RETR");
    Check(f.machine->CpuState().timer_flag != 0, "timer IRQ: timer flag");
}

void TestT0T1Integration() {
    // T0 branch.
    {
        MachineFixture f(true);
        std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
        PutJmp(bios, 0x000, 0x020);
        bios[0x020] = 0x36; bios[0x021] = 0x26; // JT0 0x026
        bios[0x022] = 0x23; bios[0x023] = 0x11;
        PutJmp(bios, 0x024, 0x028);
        bios[0x026] = 0x23; bios[0x027] = 0x22;
        PutJmp(bios, 0x028, 0x028);
        f.machine->LoadBiosBytes(std::move(bios), "t0");
        f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
        f.machine->PowerOn();
        f.machine->SetPinOverrides(true, false, false, false);
        f.machine->RunInstructions(12);
        CheckEq(f.machine->CpuState().a, 0x11, "T0: low branch");
        f.machine->PowerOn();
        f.machine->SetPinOverrides(true, true, false, false);
        f.machine->RunInstructions(12);
        CheckEq(f.machine->CpuState().a, 0x22, "T0: high branch");
    }
    // T1 branch.
    {
        MachineFixture f(true);
        std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
        PutJmp(bios, 0x000, 0x020);
        bios[0x020] = 0x56; bios[0x021] = 0x26; // JT1 0x026
        bios[0x022] = 0x23; bios[0x023] = 0x11;
        PutJmp(bios, 0x024, 0x028);
        bios[0x026] = 0x23; bios[0x027] = 0x22;
        PutJmp(bios, 0x028, 0x028);
        f.machine->LoadBiosBytes(std::move(bios), "t1");
        f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
        f.machine->PowerOn();
        f.machine->SetPinOverrides(false, false, true, false);
        f.machine->RunInstructions(12);
        CheckEq(f.machine->CpuState().a, 0x11, "T1: low branch");
        f.machine->PowerOn();
        f.machine->SetPinOverrides(false, false, true, true);
        f.machine->RunInstructions(12);
        CheckEq(f.machine->CpuState().a, 0x22, "T1: high branch");
    }
    // Counter falling edges and overflow.
    {
        MachineFixture f(true);
        std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
        PutJmp(bios, 0x000, 0x020);
        bios[0x020] = 0x15; // DIS I
        bios[0x021] = 0x23; bios[0x022] = 0xFE; // MOV A,#FE
        bios[0x023] = 0x62; // MOV T,A
        bios[0x024] = 0x45; // STRT CNT
        bios[0x025] = 0x00; // NOP
        PutJmp(bios, 0x026, 0x025);
        f.machine->LoadBiosBytes(std::move(bios), "counter");
        f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
        f.machine->PowerOn();
        f.machine->SetPinOverrides(false, false, true, false);
        f.machine->SchedulePinTransition(2400, true, true);
        f.machine->SchedulePinTransition(4800, true, false);
        f.machine->SchedulePinTransition(7200, true, true);
        f.machine->SchedulePinTransition(9600, true, false);
        f.machine->SchedulePinTransition(12000, true, true);
        f.machine->SchedulePinTransition(14400, true, false);
        f.machine->RunUntil(11000);
        CheckEq(f.machine->CpuState().timer, 0x00,
                "T1 counter: two falling edges wrap FE->00");
        Check(f.machine->CpuState().timer_flag != 0,
              "T1 counter: overflow flag");
        f.machine->RunUntil(16000);
        CheckEq(f.machine->CpuState().timer, 0x01,
                "T1 counter: repeated falling edge after overflow");
    }
}

void TestInputScheduling() {
    MachineFixture f(true);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    bios[0x020] = 0x23; bios[0x021] = 0xFB; // MOV A,#FB
    bios[0x022] = 0x39; // OUTL P1,A
    bios[0x023] = 0x27; // CLR A
    bios[0x024] = 0x3A; // OUTL P2,A
    bios[0x025] = 0x08; // INS A,BUS
    bios[0x026] = 0xAF; // MOV R7,A
    PutJmp(bios, 0x027, 0x025);
    f.machine->LoadBiosBytes(std::move(bios), "input");
    f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    f.machine->PowerOn();
    o2em::ng::InputTransition down;
    down.tick = static_cast<o2em::ng::Tick>(300 * o2em::ng::kTicksPerCpuCycle);
    down.target = o2em::ng::InputTarget::Controller1;
    down.value = 0x10; // fire
    Check(f.machine->ScheduleInput(down), "input: schedule down");
    o2em::ng::InputTransition up;
    up.tick = static_cast<o2em::ng::Tick>(600 * o2em::ng::kTicksPerCpuCycle);
    up.target = o2em::ng::InputTarget::Controller1;
    up.value = 0x00;
    Check(f.machine->ScheduleInput(up), "input: schedule up");
    f.machine->RunUntil(static_cast<o2em::ng::Tick>(400 * o2em::ng::kTicksPerCpuCycle));
    CheckEq(f.machine->CpuState().ram[7], 0xEF, "input: fire active-low");
    f.machine->RunUntil(static_cast<o2em::ng::Tick>(800 * o2em::ng::kTicksPerCpuCycle));
    CheckEq(f.machine->CpuState().ram[7], 0xFF, "input: fire released");

    o2em::ng::InputState immediate;
    immediate.p1_buttons = 0x10;
    f.machine->SetInput(immediate);
    f.machine->RunInstructions(10);
    CheckEq(f.machine->CpuState().ram[7], 0xEF, "input: SetInput immediate fire");
    f.machine->SetInput(o2em::ng::InputState{});
    f.machine->RunInstructions(10);
    CheckEq(f.machine->CpuState().ram[7], 0xFF, "input: SetInput immediate release");

    // Keyboard matrix scheduled transition.
    MachineFixture k(true);
    bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    bios[0x020] = 0x23; bios[0x021] = 0xFB; // P12 low
    bios[0x022] = 0x39;
    bios[0x023] = 0x23; bios[0x024] = 0xF0; // MOV A,#F0
    bios[0x025] = 0x3A; // OUTL P2,A (row 0, upper bits input)
    bios[0x026] = 0x0A; // IN A,P2
    bios[0x027] = 0xAF; // MOV R7,A
    PutJmp(bios, 0x028, 0x026);
    k.machine->LoadBiosBytes(std::move(bios), "input-kbd");
    k.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    k.machine->PowerOn();
    o2em::ng::InputTransition key;
    key.tick = static_cast<o2em::ng::Tick>(300 * o2em::ng::kTicksPerCpuCycle);
    key.target = o2em::ng::InputTarget::KeyboardRow;
    key.index = 0;
    key.value = 0x01;
    Check(k.machine->ScheduleInput(key), "input: schedule key");
    o2em::ng::InputTransition keyUp = key;
    keyUp.tick = static_cast<o2em::ng::Tick>(600 * o2em::ng::kTicksPerCpuCycle);
    keyUp.value = 0x00;
    Check(k.machine->ScheduleInput(keyUp), "input: schedule key release");
    k.machine->RunUntil(static_cast<o2em::ng::Tick>(400 * o2em::ng::kTicksPerCpuCycle));
    CheckEq(k.machine->CpuState().ram[7], 0xE0, "input: keyboard active-low");
    k.machine->RunUntil(static_cast<o2em::ng::Tick>(800 * o2em::ng::kTicksPerCpuCycle));
    CheckEq(k.machine->CpuState().ram[7], 0xF0, "input: keyboard released");
}

void TestInputAdapter() {
    using o2em::ng::BuildMachineInput;
    using o2em::ng::ControllerBits;
    using o2em::ng::ControllerState;
    using o2em::ng::MachineInput;
    using o2em::ng::O2emKeyMatrixPosition;
    using o2em::ng::SetO2emControllers;
    using o2em::ng::SetO2emKey;
    using o2em::ng::SetO2emKeyboard;

    int row = -1, bit = -1;
    Check(O2emKeyMatrixPosition(KEY_ENTER, row, bit) && row == 5 && bit == 7,
          "adapter: Enter -> row5 bit7");
    Check(O2emKeyMatrixPosition(KEY_1, row, bit) && row == 0 && bit == 1,
          "adapter: 1 -> row0 bit1");
    Check(O2emKeyMatrixPosition(KEY_0, row, bit) && row == 0 && bit == 0,
          "adapter: 0 -> row0 bit0");
    Check(O2emKeyMatrixPosition(KEY_8, row, bit) && row == 1 && bit == 0,
          "adapter: 8 -> row1 bit0");
    Check(O2emKeyMatrixPosition(KEY_9, row, bit) && row == 1 && bit == 1,
          "adapter: 9 -> row1 bit1");
    Check(O2emKeyMatrixPosition(KEY_SPACE, row, bit) && row == 1 && bit == 4,
          "adapter: Space -> row1 bit4");
    Check(O2emKeyMatrixPosition(KEY_W, row, bit) && row == 2 && bit == 1,
          "adapter: W -> row2 bit1");
    Check(O2emKeyMatrixPosition(KEY_Q, row, bit) && row == 3 && bit == 0,
          "adapter: Q -> row3 bit0");
    Check(O2emKeyMatrixPosition(KEY_A, row, bit) && row == 4 && bit == 0,
          "adapter: A -> row4 bit0");
    Check(O2emKeyMatrixPosition(KEY_Y, row, bit) && row == 5 && bit == 4,
          "adapter: Y -> row5 bit4");
    Check(O2emKeyMatrixPosition(KEY_N, row, bit) && row == 5 && bit == 5,
          "adapter: N -> row5 bit5");
    Check(!O2emKeyMatrixPosition(0, row, bit), "adapter: no key");

    bool pressed[64] = {};
    pressed[KEY_1] = true;
    pressed[KEY_ENTER] = true;
    pressed[KEY_Y] = true;
    MachineInput mi = BuildMachineInput(pressed, 64, {}, {});
    CheckEq(mi.keyboard.rows[0], 0x02, "adapter: matrix row0");
    CheckEq(mi.keyboard.rows[5], 0x90, "adapter: matrix row5 (Y + Enter)");
    CheckEq(mi.keyboard.rows[1], 0x00, "adapter: matrix row1 empty");

    // Simultaneous keys in one row keep both bits; the 74148 scan picks the
    // highest-priority (most significant) bit.
    bool p2[64] = {};
    p2[KEY_0] = true;
    p2[KEY_7] = true;
    MachineInput m2 = BuildMachineInput(p2, 64, {}, {});
    CheckEq(m2.keyboard.rows[0], 0x81, "adapter: simultaneous row bits");

    SetO2emKey(mi, KEY_ENTER, false);
    CheckEq(mi.keyboard.rows[5], 0x10, "adapter: release clears one bit");

    ControllerState c1;
    c1.up = true;
    c1.fire = true;
    ControllerState c2;
    c2.down = true;
    c2.left = true;
    SetO2emControllers(mi, c1, c2);
    CheckEq(ControllerBits(mi.p1), 0x11, "adapter: P1 up+fire pins");
    CheckEq(ControllerBits(mi.p2), 0x0C, "adapter: P2 down+left pins");

    // End-to-end through the real bus scan: P1 bit2 low enables the 74148.
    BusFixture f;
    f.Init(MakeBytes(0x800, 0x00));
    bool key[64] = {};
    key[KEY_1] = true;
    f.input = BuildMachineInput(key, 64, {}, {});
    f.P1(0xF3); // P12=0 (keyboard enabled), other selects inactive
    f.P2(0x00); // row 0
    CheckEq(f.bus.PortRead(2), 0xCF,
            "adapter: bus keyboard scan encodes '1' (row0 bit1)");
    f.P1(0xFF); // P12=1 -> decoder disabled
    CheckEq(f.bus.PortRead(2), 0xFF, "adapter: keyboard disabled returns FF");
}

std::uint64_t StateHash(const o2em::ng::Machine& m) {
    std::uint64_t h = 1469598103934665603ull;
    const auto& s = m.CpuState();
    h = Fnv1aBytes(h, &s.a, sizeof(s.a));
    h = Fnv1aBytes(h, &s.pc, sizeof(s.pc));
    h = Fnv1aBytes(h, &s.psw, sizeof(s.psw));
    h = Fnv1aBytes(h, s.ram, sizeof(s.ram));
    h = Fnv1aBytes(h, &s.timer, sizeof(s.timer));
    h = Fnv1aBytes(h, &s.prescaler, sizeof(s.prescaler));
    h = Fnv1aBytes(h, &s.p1, sizeof(s.p1));
    h = Fnv1aBytes(h, &s.p2, sizeof(s.p2));
    const auto now = m.Now();
    h = Fnv1aBytes(h, &now, sizeof(now));
    return h;
}

void TestSaveRestoreDeterminism() {
    MachineFixture f(true);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    bios[0x020] = 0x23; bios[0x021] = 0xFB; // P12 low
    bios[0x022] = 0x39;
    bios[0x023] = 0x27;
    bios[0x024] = 0x3A;
    bios[0x025] = 0x08; // INS BUS
    bios[0x026] = 0xAF; // MOV R7,A
    bios[0x027] = 0x18; // INC R0
    PutJmp(bios, 0x028, 0x025);
    f.machine->LoadBiosBytes(std::move(bios), "save");
    f.machine->LoadCartridgeBytes(MakeBytes(0x1000, 0x11), "cart");
    f.machine->PowerOn();
    for (int i = 0; i < 40; ++i) {
        o2em::ng::InputTransition t;
        t.tick = 1000 + i * 37;
        t.target = o2em::ng::InputTarget::Controller1;
        t.value = static_cast<std::uint8_t>((i & 1) ? 0x00 : 0x10);
        f.machine->ScheduleInput(t);
    }
    f.machine->RunInstructions(5000);
    // Leave a future input transition pending so the scheduler state itself
    // must be restored, not just the device state.
    o2em::ng::InputTransition pending;
    pending.tick = f.machine->Now() + 10000;
    pending.target = o2em::ng::InputTarget::Controller1;
    pending.value = 0x10;
    Check(f.machine->ScheduleInput(pending), "save-state: pending event scheduled");
    std::vector<std::uint8_t> saved;
    Check(f.machine->SaveState(saved), "save-state: save");

    f.machine->MutableTrace().SetEnabled(true);
    f.machine->MutableTrace().SetFilter({0, INT64_MAX, 0, 0xFFF, 0xFFFF, 20000, true});
    f.machine->MutableTrace().Reset();
    f.machine->RunInstructions(3000);
    const std::uint64_t hash_a = StateHash(*f.machine);
    const auto records_a = f.machine->Trace().Records();
    const std::uint64_t now_a = f.machine->Now();

    Check(f.machine->LoadState(saved.data(), saved.size()),
          "save-state: restore");
    f.machine->MutableTrace().Reset();
    f.machine->RunInstructions(3000);
    const std::uint64_t hash_b = StateHash(*f.machine);
    const auto records_b = f.machine->Trace().Records();
    CheckEq(now_a, f.machine->Now(), "save-state: now replay");
    CheckEq(hash_a, hash_b, "save-state: full state replay");
    CheckEq(records_a.size(), records_b.size(), "save-state: trace size replay");
    bool same_trace = records_a.size() == records_b.size();
    for (std::size_t i = 0; same_trace && i < records_a.size(); ++i) {
        same_trace = records_a[i].tick == records_b[i].tick &&
                     records_a[i].pc == records_b[i].pc &&
                     records_a[i].op == records_b[i].op &&
                     records_a[i].address == records_b[i].address &&
                     records_a[i].data == records_b[i].data &&
                     records_a[i].p1 == records_b[i].p1 &&
                     records_a[i].p2 == records_b[i].p2;
    }
    Check(same_trace, "save-state: bus trace replay");
}

void TestFourInOneSaveState() {
    std::vector<std::uint8_t> bios;
    std::vector<std::uint8_t> cart;
    if (!LoadRealBios(bios) || !LoadRealCart("vp_40.bin", cart)) {
        ++g_skips;
        std::printf("SKIP: real BIOS/vp_40 missing for 4in1 save-state\n");
        return;
    }
    NullHost host;
    auto make = [&](std::unique_ptr<o2em::ng::Machine>& out) {
        o2em::ng::MachineConfig config;
        config.region = o2em::ng::Region::Pal;
        out = o2em::ng::Machine::Create(config, host);
        out->LoadBiosBytes(bios, "G7000.bin");
        out->LoadCartridgeBytes(cart, "vp_40.bin");
        out->PowerOn();
    };
    auto frame_hash = [](const o2em::ng::Machine& m) {
        const auto& fb = m.GetFramebuffer();
        std::uint64_t h = 1469598103934665603ull;
        for (std::uint8_t b : fb.pixels) {
            h ^= b;
            h *= 1099511628211ull;
        }
        return h;
    };
    std::unique_ptr<o2em::ng::Machine> a;
    make(a);
    for (int i = 0; i < 40; ++i) a->RunFrame();
    std::vector<std::uint8_t> saved;
    Check(a->SaveState(saved), "4in1 save: save");
    for (int i = 0; i < 20; ++i) a->RunFrame();
    const std::uint64_t ha = StateHash(*a);
    const std::uint64_t fa = frame_hash(*a);
    Check(a->LoadState(saved.data(), saved.size()), "4in1 save: restore");
    for (int i = 0; i < 20; ++i) a->RunFrame();
    CheckEq(StateHash(*a), ha, "4in1 save: state replay");
    CheckEq(frame_hash(*a), fa, "4in1 save: frame replay");

    std::unique_ptr<o2em::ng::Machine> b;
    make(b);
    for (int i = 0; i < 60; ++i) b->RunFrame();
    CheckEq(StateHash(*b), ha, "4in1 determinism: independent run state");
    CheckEq(frame_hash(*b), fa, "4in1 determinism: independent run frame");
}

void TestBusTraceFilters() {
    MachineFixture f;
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    PutJmp(bios, 0x000, 0x020);
    bios[0x020] = 0x23; bios[0x021] = 0xAF; // P16=0 P14=0
    bios[0x022] = 0x39;
    bios[0x023] = 0xB8; bios[0x024] = 0x10;
    bios[0x025] = 0x23; bios[0x026] = 0x5A;
    bios[0x027] = 0x90; // MOVX @R0,A
    bios[0x028] = 0x80; // MOVX A,@R0
    PutJmp(bios, 0x029, 0x020);
    f.machine->LoadBiosBytes(std::move(bios), "trace");
    f.machine->LoadCartridgeBytes(MakeBytes(0x800, 0x00), "cart");
    f.machine->PowerOn();
    auto& tr = f.machine->MutableTrace();
    tr.SetEnabled(true);
    tr.SetFilter({0, INT64_MAX, 0, 0xFFF, 0xFFFF, 8, true});
    tr.Reset();
    f.machine->RunInstructions(200);
    CheckEq(tr.Size(), 8, "trace: max record count");
    Check(tr.Truncated(), "trace: truncated flag");
    for (const auto& r : tr.Records()) {
        CheckEq(static_cast<std::uint64_t>(r.cpu_cycle),
                static_cast<std::uint64_t>(r.tick / o2em::ng::kTicksPerCpuCycle),
                "trace: cpu cycle field");
        Check(r.p1 <= 0xFF && r.p2 <= 0xFF, "trace: port fields");
    }

    tr.Reset();
    tr.SetFilter({0, INT64_MAX, 0x400, 0x4FF, 0xFFFF, 1000, false});
    f.machine->RunInstructions(200);
    bool pc_ok = true;
    bool no_fetch = true;
    for (const auto& r : tr.Records()) {
        pc_ok = pc_ok && r.pc >= 0x400 && r.pc <= 0x4FF;
        no_fetch = no_fetch && r.op != o2em::ng::BusOp::ProgramFetch;
    }
    Check(pc_ok, "trace: PC range filter");
    Check(no_fetch, "trace: program fetch filter");

    tr.Reset();
    tr.SetFilter({0, INT64_MAX, 0, 0xFFF, 0xFFFF, 1000, true});
    f.machine->RunInstructions(200);
    const std::size_t before = tr.Size();
    Check(before > 0, "trace: unfiltered records");
    tr.Reset();
    tr.SetFilter({0, 100, 0, 0xFFF, 0xFFFF, 1000, true});
    f.machine->RunInstructions(200);
    bool time_ok = true;
    for (const auto& r : tr.Records()) time_ok = time_ok && r.tick <= 100;
    Check(time_ok, "trace: time range filter");

    tr.Reset();
    tr.SetFilter({0, INT64_MAX, 0, 0xFFF,
                  static_cast<std::uint16_t>(1u << static_cast<unsigned>(
                      o2em::ng::BusOp::MovxRead)) |
                      static_cast<std::uint16_t>(1u << static_cast<unsigned>(
                          o2em::ng::BusOp::MovxWrite)),
                  1000, true});
    f.machine->RunInstructions(200);
    bool op_ok = true;
    for (const auto& r : tr.Records())
        op_ok = op_ok && (r.op == o2em::ng::BusOp::MovxRead ||
                          r.op == o2em::ng::BusOp::MovxWrite);
    Check(op_ok, "trace: operation mask filter");
    Check(tr.Size() > 0, "trace: MOVX records present");
}

void TestReplay() {
    auto run = [](std::vector<std::uint8_t>& reads) {
        BusFixture f;
        f.Init(MakeBytes(0x0800, 0x00));
        f.P1(0xAF);
        f.bus.DataWrite(0x10, 0x11);
        f.bus.DataWrite(0x20, 0x22);
        reads.push_back(f.bus.DataRead(0x10));
        f.P1(0xBF);
        f.bus.DataWrite(0x80, 0x33);
        f.bus.DataWrite(0x10, 0x44);
        f.P1(0xA7);
        f.bus.DataWrite(0xA0, 0x20);
        reads.push_back(f.bus.DataRead(0xA0));
        f.P2(0x04);
        f.P1(0xFB);
        reads.push_back(f.bus.BusRead());
        std::uint64_t h = 0;
        h = Fnv1aBytes(h, f.bus.ExternalRam(), 0x80);
        h = Fnv1aBytes(h, f.vdc.Registers(), 256);
        const std::uint8_t status = f.vdc.Status();
        const std::uint8_t irq = f.vdc.IrqLine() ? 1 : 0;
        h = Fnv1aBytes(h, &status, 1);
        h = Fnv1aBytes(h, &irq, 1);
        const std::uint8_t p1 = f.bus.P1();
        const std::uint8_t p2 = f.bus.P2();
        h = Fnv1aBytes(h, &p1, 1);
        h = Fnv1aBytes(h, &p2, 1);
        return h;
    };
    std::vector<std::uint8_t> a;
    std::vector<std::uint8_t> b;
    const std::uint64_t ha = run(a);
    const std::uint64_t hb = run(b);
    CheckEq(ha, hb, "replay: final transaction state");
    Check(a.size() == 3 && a == b, "replay: returned values");
    Check(a.size() == 3 && a[0] == 0x11 && a[1] == 0x20 && a[2] == 0xFF,
          "replay: decoded values");
}

// ---------------------------------------------------------------------------
// Real BIOS / cartridge execution

bool LoadRealBios(std::vector<std::uint8_t>& bios) {
    return LoadAsset("BIOS/G7000.bin", bios);
}

bool LoadRealCart(const std::string& name, std::vector<std::uint8_t>& cart) {
    const std::string path = LocalCartPath(name);
    if (path.empty()) return false;
    return ReadFile(path, cart);
}

void TestRealBiosExecution() {
    std::vector<std::uint8_t> bios;
    std::vector<std::uint8_t> cart;
    if (!LoadRealBios(bios) || !LoadRealCart("vp_01.bin", cart)) {
        ++g_skips;
        std::printf("SKIP: real BIOS/vp_01 not found\n");
        return;
    }

    auto run_once = [&](std::uint64_t instructions) {
        NullHost host;
        o2em::ng::MachineConfig config;
        config.region = o2em::ng::Region::Pal;
        auto m = o2em::ng::Machine::Create(config, host);
        Check(m->LoadBiosBytes(bios, "G7000.bin"), "BIOS: load");
        Check(m->LoadCartridgeBytes(cart, "vp_01.bin"), "BIOS: cart load");
        m->PowerOn();
        m->RunInstructions(1);
        const std::uint16_t pc1 = m->CpuState().pc;
        m->RunInstructions(1);
        const std::uint16_t pc2 = m->CpuState().pc;
        m->RunInstructions(instructions - 2);
        return std::make_tuple(pc1, pc2, StateHash(*m), m->CpuState().illegal_count,
                               m->Now(), m->MapperState() ? m->MapperState()->Bank() : 0);
    };
    auto a = run_once(100000);
    auto b = run_once(100000);
    CheckEq(std::get<0>(a), 0x400, "BIOS: reset jumps to cartridge");
    CheckEq(std::get<1>(a), 0x2C3, "BIOS: cartridge entry");
    CheckEq(std::get<2>(a), std::get<2>(b), "BIOS: deterministic state");
    CheckEq(std::get<4>(a), std::get<4>(b), "BIOS: deterministic time");
    CheckEq(std::get<3>(a), 0, "BIOS: no illegal opcodes");
    Check(std::get<5>(a) <= 3, "BIOS: valid mapper bank");

    // Capture compact checkpoints at 100/1000/10000 instructions by replay.
    NullHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    m->LoadBiosBytes(bios, "G7000.bin");
    m->LoadCartridgeBytes(cart, "vp_01.bin");
    m->PowerOn();
    m->RunInstructions(100);
    const std::uint16_t pc100 = m->CpuState().pc;
    const std::uint64_t h100 = StateHash(*m);
    m->RunInstructions(900);
    const std::uint16_t pc1000 = m->CpuState().pc;
    const std::uint64_t h1000 = StateHash(*m);
    m->RunInstructions(9000);
    const std::uint64_t h10000 = StateHash(*m);
    Check(pc100 < 0x1000 && pc1000 < 0x1000, "BIOS: checkpoints in 12-bit PC");
    Check(h100 != h1000 && h1000 != h10000, "BIOS: state progresses");
}

void TestRealCartExecution() {
    struct Case {
        const char* name;
        o2em::ng::CartridgeKind kind;
    };
    const Case cases[] = {
        {"vp_01.bin", o2em::ng::CartridgeKind::Normal2K},
        {"im_atlantis.bin", o2em::ng::CartridgeKind::Normal4K},
        {"Tutankham.bin", o2em::ng::CartridgeKind::Normal8K},
        {"new_ktaa.bin", o2em::ng::CartridgeKind::ThreeK12K},
        {"vp_60_16.bin", o2em::ng::CartridgeKind::Rally16K},
    };
    std::vector<std::uint8_t> bios;
    if (!LoadRealBios(bios)) {
        ++g_skips;
        std::printf("SKIP: real BIOS not found\n");
        return;
    }
    for (const auto& c : cases) {
        std::vector<std::uint8_t> cart;
        if (!LoadRealCart(c.name, cart)) {
            ++g_skips;
            std::printf("SKIP: %s not found\n", c.name);
            continue;
        }
        auto run_once = [&](std::uint64_t instructions, std::uint64_t& hash,
                            std::uint16_t& last_pc, std::uint64_t& illegal,
                            std::uint8_t& bank) {
            NullHost host;
            o2em::ng::MachineConfig config;
            config.region = o2em::ng::Region::Pal;
            auto m = o2em::ng::Machine::Create(config, host);
            m->LoadBiosBytes(bios, "G7000.bin");
            if (!m->LoadCartridgeBytes(cart, c.name)) {
                hash = 0;
                return;
            }
            Check(m->Cart().info.kind == c.kind, "cart: expected mapper kind");
            m->PowerOn();
            bool saw_cart = false;
            bool saw_movx = false;
            bool saw_port = false;
            m->MutableTrace().SetEnabled(true);
            m->MutableTrace().SetFilter({0, INT64_MAX, 0, 0xFFF, 0xFFFF, 20000, true});
            for (std::uint64_t i = 0; i < instructions; i += 50) {
                m->RunInstructions(50);
                const auto& s = m->CpuState();
                if (s.pc >= 0x400) saw_cart = true;
                if (s.last_pc >= 0x400) saw_cart = true;
            }
            for (const auto& r : m->Trace().Records()) {
                if (r.op == o2em::ng::BusOp::MovxRead ||
                    r.op == o2em::ng::BusOp::MovxWrite)
                    saw_movx = true;
                if (r.op == o2em::ng::BusOp::PortWrite) saw_port = true;
            }
            const auto& s = m->CpuState();
            last_pc = s.pc;
            illegal = s.illegal_count;
            bank = m->MapperState() ? m->MapperState()->Bank() : 0;
            hash = StateHash(*m);
            Check(saw_cart, "cart: cartridge code executed");
            Check(saw_movx, "cart: MOVX activity");
            Check(saw_port, "cart: port activity");
        };
        std::uint64_t ha = 0, hb = 0, ia = 0, ib = 0;
        std::uint16_t pa = 0, pb = 0;
        std::uint8_t ba = 0, bb = 0;
        run_once(200000, ha, pa, ia, ba);
        run_once(200000, hb, pb, ib, bb);
        CheckEq(ha, hb, "cart: deterministic execution");
        CheckEq(pa, pb, "cart: deterministic PC");
        CheckEq(ia, 0, "cart: no illegal opcodes run 1");
        CheckEq(ib, 0, "cart: no illegal opcodes run 2");
        CheckEq(ba, bb, "cart: deterministic mapper bank");
        (void)c.kind;
    }
}


// Real BIOS + vp_01 MAME 0.289 prefix differential. MAME's debugger trace was
// generated with -cart vp_01.bin; the first 4,261 traced PCs (starting at the
// cartridge entry 0x400) match NG exactly. MAME's first external IRQ occurs at
// trace index 4,261; that is the VDC VBL phase, not a CPU/bus difference.
void TestRealBiosMamePrefix() {
    std::vector<std::uint8_t> bios;
    std::vector<std::uint8_t> cart;
    if (!LoadRealBios(bios) || !LoadRealCart("vp_01.bin", cart)) {
        ++g_skips;
        std::printf("SKIP: real BIOS/vp_01 MAME prefix assets missing\n");
        return;
    }
    NullHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    Check(m->LoadBiosBytes(std::move(bios), "G7000.bin"),
          "MAME prefix: BIOS load");
    Check(m->LoadCartridgeBytes(std::move(cart), "vp_01.bin"),
          "MAME prefix: cart load");
    m->PowerOn();
    constexpr std::uint64_t kPrefix = 4261;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint64_t i = 0; i < kPrefix + 1; ++i) {
        const auto& s = m->CpuState();
        if (i != 0) {
            const std::uint16_t pc = s.pc;
            hash ^= static_cast<std::uint8_t>(pc & 0xFF);
            hash *= 1099511628211ull;
            hash ^= static_cast<std::uint8_t>((pc >> 8) & 0x0F);
            hash *= 1099511628211ull;
        }
        m->RunInstructions(1);
    }
    CheckEq(hash, 0x44B7F94C07942A55ull,
            "MAME prefix: real BIOS/cart PC sequence");
}

// ---------------------------------------------------------------------------
// MAME runtime differential fixture


void TestMameRuntimeBankedFixture() {
    std::vector<std::uint8_t> cart;
    if (!LoadAsset("tests/machine_ng/fixtures/synth_std_4k_banks.bin", cart)) {
        ++g_skips;
        std::printf("SKIP: MAME 4K bank fixture missing\n");
        return;
    }
    NullHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
    bios[0] = 0x84;
    bios[1] = 0x00;
    m->LoadBiosBytes(std::move(bios), "fixture-bios");
    Check(m->LoadCartridgeBytes(std::move(cart), "synth_std_4k_banks.bin"),
          "MAME 4K: cart load");
    m->PowerOn();
    constexpr std::uint64_t kInstructions = 15767;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint64_t i = 0; i < kInstructions + 1; ++i) {
        const auto& s = m->CpuState();
        if (i != 0) {
            const std::uint16_t pc = s.pc;
            hash ^= static_cast<std::uint8_t>(pc & 0xFF);
            hash *= 1099511628211ull;
            hash ^= static_cast<std::uint8_t>((pc >> 8) & 0x0F);
            hash *= 1099511628211ull;
        }
        m->RunInstructions(1);
    }
    CheckEq(hash, 0x95037606FA5FF93Dull, "MAME 4K: PC sequence hash");
    const auto& s = m->CpuState();
    CheckEq(s.pc, 0x0440, "MAME 4K: final PC");
    CheckEq(s.a, 0x33, "MAME 4K: final A");
    CheckEq(s.ram[5], 0x33, "MAME 4K: bank1 R5");
    CheckEq(s.ram[6], 0x44, "MAME 4K: bank0 R6");
    CheckEq(s.ram[7], 0x22, "MAME 4K: bank1 R7");
    CheckEq(s.p1, 0xFD, "MAME 4K: final P1");
    CheckEq(s.p2, 0xFF, "MAME 4K: final P2");
    CheckEq(s.illegal_count, 0, "MAME 4K: no illegal opcodes");
}

void TestMameRuntimeFixture() {
    std::vector<std::uint8_t> cart;
    if (!LoadAsset("tests/machine_ng/fixtures/synth_std_2k.bin", cart)) {
        ++g_skips;
        std::printf("SKIP: MAME fixture cart missing\n");
        return;
    }
    NullHost host;
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    auto m = o2em::ng::Machine::Create(config, host);
    std::vector<std::uint8_t> fixture_bios = MakeBytes(0x400, 0xFF);
    fixture_bios[0] = 0x84; // JMP 0x400, matching the G7000 BIOS entry
    fixture_bios[1] = 0x00;
    m->LoadBiosBytes(std::move(fixture_bios), "fixture-bios");
    Check(m->LoadCartridgeBytes(cart, "synth_std_2k.bin"), "MAME fixture: cart load");
    m->PowerOn();

    // MAME 0.289 trace began at the first cartridge fetch (0x400). NG first
    // executes the BIOS JMP at 0x000, so skip one PC.
    constexpr std::uint64_t kMameInstructions = 15779;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint64_t i = 0; i < kMameInstructions + 1; ++i) {
        const auto& s = m->CpuState();
        if (i != 0) {
            const std::uint16_t pc = s.pc;
            hash ^= static_cast<std::uint8_t>(pc & 0xFF);
            hash *= 1099511628211ull;
            hash ^= static_cast<std::uint8_t>((pc >> 8) & 0x0F);
            hash *= 1099511628211ull;
        }
        m->RunInstructions(1);
    }
    CheckEq(hash, 0xAEF10A408054B017ull, "MAME runtime: PC sequence hash");
    const auto& s = m->CpuState();
    CheckEq(s.pc, 0x045F, "MAME runtime: final PC");
    CheckEq(s.a, 0xAA, "MAME runtime: final A");
    CheckEq(s.sp, 0, "MAME runtime: final SP");
    CheckEq(s.ram[0], 0x00, "MAME runtime: R0");
    CheckEq(s.ram[1], 0x20, "MAME runtime: R1");
    CheckEq(s.ram[2], 0x00, "MAME runtime: R2");
    CheckEq(s.ram[3], 0x00, "MAME runtime: R3");
    CheckEq(s.ram[4], 0x00, "MAME runtime: R4");
    CheckEq(s.ram[5], 0x00, "MAME runtime: R5");
    CheckEq(s.ram[6], 0x00, "MAME runtime: R6");
    CheckEq(s.ram[7], 0x00, "MAME runtime: R7");
    CheckEq(s.p1, 0xB7, "MAME runtime: final P1");
    CheckEq(s.p2, 0xFF, "MAME runtime: final P2");
    CheckEq(s.timer, 0xF0, "MAME runtime: final timer");
    CheckEq(s.prescaler, 6, "MAME runtime: final prescaler");
    CheckEq(s.illegal_count, 0, "MAME runtime: no illegal opcodes");
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::strcmp(argv[1], "--trace") == 0) {
        // Diagnostic-only PC trace used while generating MAME fixtures.
        std::vector<std::uint8_t> cart;
        if (!ReadFile(argv[2], cart)) return 2;
        NullHost host;
        o2em::ng::MachineConfig config;
        config.region = o2em::ng::Region::Pal;
        auto m = o2em::ng::Machine::Create(config, host);
        std::vector<std::uint8_t> bios = MakeBytes(0x400, 0xFF);
        bios[0] = 0x84;
        bios[1] = 0x00;
        m->LoadBiosBytes(std::move(bios), "trace-bios");
        m->LoadCartridgeBytes(std::move(cart), "trace-cart");
        m->PowerOn();
        const int count = argc > 3 ? std::atoi(argv[3]) : 20000;
        for (int i = 0; i < count; ++i) {
            std::printf("PC=%03X\n", m->CpuState().pc);
            m->RunInstructions(1);
        }
        return 0;
    }

    std::printf("O2EM-NG NG-2 machine conformance\n");
    std::printf("core: src/core_ng/{cpu,scheduler,memory,video,machine}\n\n");

#define RUN(fn) do { std::printf("[TEST] %s\n", #fn); std::fflush(stdout); fn(); } while (0)
    RUN(TestTimebase);
    RUN(TestSchedulerOrdering);
    RUN(TestSchedulerSnapshot);
    RUN(TestCartridgeClassification);
    RUN(TestStandardMapper);
    RUN(TestEightKMapper);
    RUN(Test32KMapper);
    RUN(TestThreeKMapper);
    RUN(TestRallyMapper);
    RUN(TestFourInOneMapper);
    RUN(TestMapperSnapshot);
    RUN(TestProgramDecode);
    RUN(TestExternalRamAndOpenBus);
    RUN(TestP1P2Decode);
    RUN(TestSpecialHardwareBus);
    RUN(TestSpecialHardwareMachineLifecycle);
    RUN(TestMovxRouting);
    RUN(TestMovxCpuRouting);
    RUN(TestPalNtscProfiles);
    RUN(TestPowerOnReset);
    RUN(TestIrqIntegration);
    RUN(TestIrqHeldAcrossDisable);
    RUN(TestTimerIrqIntegration);
    RUN(TestT0T1Integration);
    RUN(TestInputScheduling);
    RUN(TestInputAdapter);
    RUN(TestSaveRestoreDeterminism);
    RUN(TestFourInOneSaveState);
    RUN(TestBusTraceFilters);
    RUN(TestReplay);
    RUN(TestRealBiosExecution);
    RUN(TestRealCartExecution);
    RUN(TestRealBiosMamePrefix);
    RUN(TestMameRuntimeFixture);
    RUN(TestMameRuntimeBankedFixture);

    std::printf("\nchecks: %d, failures: %d, skips: %d\n", g_checks, g_failures,
                g_skips);
    if (g_failures == 0)
        std::printf("RESULT: GREEN - NG-2 machine conformance passed.\n");
    else
        std::printf("RESULT: RED - NG-2 machine conformance failures.\n");
    return g_failures == 0 ? 0 : 1;
}
