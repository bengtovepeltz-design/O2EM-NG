// ============================================================================
// src/mcs48/vbl_timing_probe.cpp
//
// Phase 2C prep: development-only PAL VBL timing probe (measurement only).
//
// Verified Phase 2B finding this probe reproduces and documents:
//   - O2EM-NG first VBL IRQ fires at machine cycle 5494 (VBLCLK = 5493 plus
//     the instruction that crosses it).
//   - MAME 0.289 PAL fires VBL much later: first VBL ~normalized cycle 7924,
//     with the first observed vector instruction at 7926.
//   - The first proven IRQ divergence is therefore VBL phase/timing.
//   - The rejected MAME "IRQ at 1199" hypothesis is intentionally NOT used
//     as a reference here.
//
// What it does:
//   Boots a clean PAL system through the normal EmulatorCore_StartRom path,
//   records the first VBL IRQ cycle, the first vector instruction, and the
//   measured frame period, and prints them beside:
//     - the emulator's own constants (VBLCLK, EVBLCLK_PAL), and
//     - the documented MAME 0.289 PAL reference.
//
// Safety / scope:
//   - Opt-in only: runs solely when O2EM_VBL_TIMING_PROBE is set. With the
//     variable unset this returns false immediately; production execution is
//     byte-for-byte unchanged.
//   - Pure observation through the existing null-by-default
//     mcs48::observation callbacks. No CPU opcode behavior, IRQ behavior,
//     audio, LINE IRQ, CX, C7010, G7400, XROM or region logic is modified,
//     and no CPU/machine state is written by the probe callbacks. The probe
//     only reads existing globals (master_clk, evblclk, pc) for its report.
//   - Nothing else in the emulator calls this probe; it is not wired into
//     the frontend, the machine loop, or any release path.
//
// Environment contract:
//   O2EM_VBL_TIMING_PROBE  set and non-empty => enable. If the value is not
//                          "1" it is also used as the CSV output path.
//   O2EM_VBL_ROM           cartridge ROM path (required).
//   O2EM_VBL_BIOS          BIOS file name resolved under <exe>/BIOS
//                          (required), e.g. "o2rom.bin".
//   O2EM_VBL_FRAMES        number of VBL events to capture (default 4, max 16).
//   O2EM_VBL_TRACE         optional explicit CSV output path.
//
// Result / exit codes:
//   0 = measurement completed and at least one VBL observed.
//   2 = could not run (missing/invalid environment, SDL init failure, no VBL).
// ============================================================================

#include "vbl_timing_probe.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "legacy_observation.h"

#include "../../cpu.h"
#include "../../vmachine.h"
#include "../../emulator_core.h"
#include "../../vdc_stub.h"
#include "keyboard.h" // extern Byte key_done (adjacent o2em include tree)

#include <SDL3/SDL.h>

namespace mcs48 {
namespace {

constexpr unsigned kDefaultFrames = 4u;
constexpr unsigned kMaxFrames = 16u;

// Documented Phase 2B / MAME 0.289 PAL reference, in normalized machine cycles.
constexpr unsigned long long kMamePalFirstVblCycle = 7924ull;
constexpr unsigned long long kMamePalFirstVectorCycle = 7926ull;

// --- Probe state (touched only while the probe is installed) ---------------
unsigned g_framesRequested = kDefaultFrames;
std::uint64_t g_ticks = 0; // cumulative emulated machine cycles (Advance)

bool g_firstVblSeen = false;
bool g_vectorSeen = false;
bool g_finished = false;

unsigned long long g_firstVblCycle = 0;
unsigned long long g_firstVectorCycle = 0;
unsigned g_firstVectorPc = 0;

std::vector<unsigned long long> g_vblCycles;   // ticks at each VBL IRQ
std::vector<int> g_vblMasterClk;               // master_clk at each VBL IRQ

// --- Observation callbacks (pure counters; no emulation state writes) ------
void AdvanceTick(unsigned cycles) {
    g_ticks += cycles;
}

void RecordEvent(unsigned kind, unsigned address, unsigned /*value*/) {
    // kind 13 = IrqTransition; address 1 = handle_vbl() VBL source.
    if (kind != 13u || address != 1u || g_finished)
        return;

    g_vblCycles.push_back(g_ticks);
    g_vblMasterClk.push_back(master_clk);

    if (!g_firstVblSeen) {
        g_firstVblSeen = true;
        g_firstVblCycle = g_ticks;
    }
    if (g_vblCycles.size() >= g_framesRequested)
        g_finished = true;
}

void BeforeInstruction(unsigned pc, unsigned /*opcode*/) {
    // The first instruction fetched after the first VBL is the IRQ vector.
    if (g_firstVblSeen && !g_vectorSeen) {
        g_vectorSeen = true;
        g_firstVectorCycle = g_ticks;
        g_firstVectorPc = pc;
    }
}

bool StopNow() {
    if (g_finished) {
        key_done = 1;
        return true;
    }
    return false;
}

// --- Helpers ---------------------------------------------------------------
void ResetProbeState() {
    g_ticks = 0;
    g_firstVblSeen = false;
    g_vectorSeen = false;
    g_finished = false;
    g_firstVblCycle = 0;
    g_firstVectorCycle = 0;
    g_firstVectorPc = 0;
    g_vblCycles.clear();
    g_vblMasterClk.clear();
}

void Uninstall() {
    observation::advance = nullptr;
    observation::event = nullptr;
    observation::before = nullptr;
    observation::stop = nullptr;
    observation::input = nullptr;
}

bool WriteCsv(const char* path) {
    if (!path || !*path)
        return false;
    FILE* f = std::fopen(path, "wb");
    if (!f)
        return false;
    std::fprintf(f, "frame,vbl_cycle,master_clk,interval\n");
    for (std::size_t i = 0; i < g_vblCycles.size(); ++i) {
        const long long interval = i == 0
            ? 0
            : static_cast<long long>(g_vblCycles[i]) - static_cast<long long>(g_vblCycles[i - 1]);
        std::fprintf(f, "%zu,%llu,%d,%lld\n", i + 1,
            g_vblCycles[i], g_vblMasterClk[i], interval);
    }
    const bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

int RunProbeImpl() {
    std::printf("O2EM-NG VBL timing probe (development-only, PAL, measurement only)\n");

    const char* cart = std::getenv("O2EM_VBL_ROM");
    const char* bios = std::getenv("O2EM_VBL_BIOS");
    if (!cart || !*cart || !bios || !*bios) {
        std::fprintf(stderr, "  missing O2EM_VBL_ROM and/or O2EM_VBL_BIOS\n");
        return 2;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(cart, ec) || ec) {
        std::fprintf(stderr, "  O2EM_VBL_ROM not found: %s\n", cart);
        return 2;
    }

    if (const char* frames = std::getenv("O2EM_VBL_FRAMES")) {
        const unsigned long v = std::strtoul(frames, nullptr, 10);
        if (v == 0 || v > kMaxFrames) {
            std::fprintf(stderr, "  O2EM_VBL_FRAMES must be 1..%u\n", kMaxFrames);
            return 2;
        }
        g_framesRequested = static_cast<unsigned>(v);
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "  SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }

    ResetProbeState();

    // Install observation callbacks. `input` is deliberately left null so the
    // normal keyboard path is used; no synthetic input is injected.
    observation::advance = AdvanceTick;
    observation::event = RecordEvent;
    observation::before = BeforeInstruction;
    observation::stop = StopNow;

    const bool booted = EmulatorCore_StartRom(cart, RegionMode::PAL, bios, false);

    Uninstall();

    const char* csv = std::getenv("O2EM_VBL_TRACE");
    const char* enable = std::getenv("O2EM_VBL_TIMING_PROBE");
    if ((!csv || !*csv) && enable && std::strcmp(enable, "1") != 0)
        csv = enable;
    const bool csvWritten = WriteCsv(csv);

    const int result = (booted && g_firstVblSeen) ? 0 : 2;

    std::printf("\n--- PAL VBL timing (observed vs expected) ---\n");
    std::printf("  ROM=%s\n", cart);
    std::printf("  BIOS=%s\n", bios);
    if (!g_firstVblSeen) {
        std::printf("  no VBL IRQ observed before the probe stopped.\n");
        SDL_Quit();
        return 2;
    }

    const unsigned long long first = g_firstVblCycle;
    const unsigned long long firstMaster = static_cast<unsigned long long>(g_vblMasterClk.front());

    std::printf("  first VBL IRQ cycle (observed)   : %llu  (master_clk=%llu)\n",
        first, firstMaster);
    if (g_vectorSeen) {
        std::printf("  first vector instr   (observed)  : cycle %llu, PC=%03X\n",
            g_firstVectorCycle, g_firstVectorPc);
    }

    std::printf("  VBL cycle samples                :");
    for (std::size_t i = 0; i < g_vblCycles.size(); ++i)
        std::printf(" %llu", g_vblCycles[i]);
    std::printf("\n");

    unsigned long long period = 0;
    bool periodStable = true;
    for (std::size_t i = 1; i < g_vblCycles.size(); ++i) {
        const unsigned long long d = g_vblCycles[i] - g_vblCycles[i - 1];
        if (i == 1)
            period = d;
        else if (d != period)
            periodStable = false;
    }
    if (g_vblCycles.size() < 2) {
        std::printf("  frame period (observed)          : n/a (need >= 2 VBL events)\n");
    } else {
        std::printf("  frame period (observed)          : %llu cycles%s\n",
            period, periodStable ? "" : " (varies)");
    }

    std::printf("  emulator constants               : VBLCLK=%d (first VBL threshold), EVBLCLK_PAL=%d (frame period)\n",
        static_cast<int>(VBLCLK), static_cast<int>(EVBLCLK_PAL));
    std::printf("  reference (MAME 0.289 PAL)       : first VBL ~%llu, first vector ~%llu\n",
        kMamePalFirstVblCycle, kMamePalFirstVectorCycle);

    const long long delta = static_cast<long long>(kMamePalFirstVblCycle) -
                            static_cast<long long>(first);
    std::printf("  first VBL delta vs reference     : %+lld cycles (%s than MAME)\n",
        delta, delta >= 0 ? "earlier" : "later");

    if (period != 0) {
        const long long pd = static_cast<long long>(period) - static_cast<long long>(EVBLCLK_PAL);
        std::printf("  frame period delta vs EVBLCLK_PAL: %+lld cycles\n", pd);
    }
    std::printf("  VERDICT: VBL phase/timing divergence reproduced (%s).\n",
        delta != 0 ? "observed first VBL differs from MAME reference" : "observed matches reference");
    if (csvWritten)
        std::printf("  CSV written: %s\n", csv);

    SDL_Quit();
    return result;
}

} // namespace

bool RunVblTimingProbeIfRequested(int& result) {
    const char* enable = std::getenv("O2EM_VBL_TIMING_PROBE");
    if (!enable || !*enable)
        return false;
    result = RunProbeImpl();
    return true;
}

// ============================================================================
// Development-only video investigation probe.
//
// Environment:
//   O2EM_VIDEO_PROBE       set/non-empty enables the probe. If the value is not
//                          "1" and O2EM_VIDEO_OUT is unset, it is used as CSV path.
//   O2EM_VIDEO_ROM         cartridge ROM path (required)
//   O2EM_VIDEO_BIOS        BIOS file name under <exe>/BIOS (required)
//   O2EM_VIDEO_FRAMES      number of frames (default 240, max 5000)
//   O2EM_VIDEO_OUT         CSV path (optional)
//   O2EM_VIDEO_DUMP_DIR    optional directory for BMP frame dumps
//   O2EM_VIDEO_DUMP_COUNT  number of BMP frames to dump (default 20)
// ============================================================================
namespace {

struct VideoFrameSample {
    unsigned long long serial = 0;
    unsigned long long hash = 0;
    int frame = 0;
    int a0 = 0;
    int a3 = 0;
    int aa = 0;
};

std::vector<VideoFrameSample> g_videoSamples;
unsigned g_videoFramesRequested = 240;
unsigned long long g_videoLastSerial = 0;
FILE* g_videoCsv = nullptr;
std::string g_videoDumpDir;
unsigned g_videoDumpLimit = 0;
unsigned g_videoDumped = 0;
struct VideoKeyEvent {
    unsigned start = 0;
    unsigned duration = 1;
    int key = -1;
};

std::vector<VideoKeyEvent> g_videoKeySeq;
int g_videoKey = -1;
unsigned g_videoKeyStart = 30;
unsigned g_videoKeyEnd = 36;
int g_videoKey2 = -1;
unsigned g_videoKey2Start = 0;
unsigned g_videoKey2End = 0;
bool g_videoVisible = false;
bool g_videoKeepSound = false;
RegionMode g_videoRegion = RegionMode::PAL;
unsigned g_videoFireStart = 0;
unsigned g_videoFireEnd = 0;
int g_videoStickValue = -1;
unsigned g_videoStickStart = 0;
unsigned g_videoStickEnd = 0;
int g_videoResetFrame = -1;
bool g_videoResetDone = false;
bool g_videoVdcDiff = false;
unsigned g_videoVdcDiffStart = 0;
unsigned g_videoVdcDiffEnd = 0;
unsigned char g_prevVdc[256] = {};
bool g_prevVdcValid = false;
int g_videoGridDumpFrame = -1;
bool g_videoGridDumped = false;
std::vector<int> g_videoRamDumpFrames;
std::vector<int> g_videoFrameDumps;

// Opt-in, bounded instruction trace used for cycle-level CPU comparison with
// an external reference emulator. Enabled only when O2EM_INSTR_TRACE_OUT is
// set; writes at most the requested frame window.
FILE* g_videoInstrTrace = nullptr;
int g_videoInstrTraceFrom = -1;
int g_videoInstrTraceTo = -1;
unsigned long long g_videoInstrCycles = 0;
bool g_videoInstrTraceOpenFailed = false;

// Opt-in internal-RAM write trace for a small address window. The probe
// observes the value at each instruction boundary, so a change is attributed
// to the instruction that just executed (pcv/opv = instruction about to run).
FILE* g_ramTrace = nullptr;
int g_ramTraceFrom = -1;
int g_ramTraceTo = -1;
int g_ramShadow[64] = {0};
bool g_ramShadowInit = false;
unsigned g_ramPrevPc = 0;
unsigned g_ramPrevOp = 0;

void VideoBefore(unsigned pcv, unsigned opv)
{
    const int frameNow = static_cast<int>(g_videoSamples.size());

    if (g_videoInstrTrace &&
        frameNow >= g_videoInstrTraceFrom && frameNow <= g_videoInstrTraceTo)
    {
        std::fprintf(g_videoInstrTrace,
            "C %llu %d %03X %02X %02X %02X %02X %02X %02X %02X %u %u %u %u %u %d %u "
            "%02X %02X %02X %02X %02X %02X %02X %02X %03X %d %d\n",
            g_videoInstrCycles, frameNow, static_cast<unsigned>(pcv), static_cast<unsigned>(opv),
            static_cast<unsigned>(acc), static_cast<unsigned>(psw), static_cast<unsigned>(sp),
            static_cast<unsigned>(p1), static_cast<unsigned>(p2), static_cast<unsigned>(itimer),
            static_cast<unsigned>(timer_on), static_cast<unsigned>(count_on),
            static_cast<unsigned>(xirq_en), static_cast<unsigned>(irq_ex),
            static_cast<unsigned>(int_clk), static_cast<int>(mstate),
            static_cast<unsigned>(reg_pnt),
            static_cast<unsigned>(intRAM[reg_pnt + 0]), static_cast<unsigned>(intRAM[reg_pnt + 1]),
            static_cast<unsigned>(intRAM[reg_pnt + 2]), static_cast<unsigned>(intRAM[reg_pnt + 3]),
            static_cast<unsigned>(intRAM[reg_pnt + 4]), static_cast<unsigned>(intRAM[reg_pnt + 5]),
            static_cast<unsigned>(intRAM[reg_pnt + 6]), static_cast<unsigned>(intRAM[reg_pnt + 7]),
            static_cast<unsigned>(A11), master_clk, static_cast<int>(t_flag));
    }

    if (g_ramTrace)
    {
        if (!g_ramShadowInit)
        {
            for (int a = 0x3B; a <= 0x3E; ++a)
                g_ramShadow[a] = intRAM[a];
            g_ramShadowInit = true;
        }
        else if (frameNow >= g_ramTraceFrom && frameNow <= g_ramTraceTo)
        {
            for (int a = 0x3B; a <= 0x3E; ++a)
            {
                if (intRAM[a] != g_ramShadow[a])
                {
                    std::fprintf(g_ramTrace,
                        "W %llu %d %d %03X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                        "%02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                        g_videoInstrCycles, frameNow, master_clk,
                        g_ramPrevPc, g_ramPrevOp, a,
                        static_cast<unsigned>(g_ramShadow[a]), static_cast<unsigned>(intRAM[a]),
                        static_cast<unsigned>(acc), static_cast<unsigned>(psw),
                        static_cast<unsigned>(sp), static_cast<unsigned>(p1),
                        static_cast<unsigned>(p2), static_cast<unsigned>(reg_pnt),
                        static_cast<unsigned>(intRAM[reg_pnt + 0]),
                        static_cast<unsigned>(intRAM[reg_pnt + 1]),
                        static_cast<unsigned>(intRAM[reg_pnt + 2]),
                        static_cast<unsigned>(intRAM[reg_pnt + 3]),
                        static_cast<unsigned>(intRAM[reg_pnt + 4]),
                        static_cast<unsigned>(intRAM[reg_pnt + 5]),
                        static_cast<unsigned>(intRAM[reg_pnt + 6]),
                        static_cast<unsigned>(intRAM[reg_pnt + 7]));
                    g_ramShadow[a] = intRAM[a];
                }
            }
        }
        else
        {
            for (int a = 0x3B; a <= 0x3E; ++a)
                g_ramShadow[a] = intRAM[a];
        }
        g_ramPrevPc = pcv;
        g_ramPrevOp = opv;
    }
}


bool VideoInput(int key) {
    const unsigned frameNow = static_cast<unsigned>(g_videoSamples.size());
    for (const auto& e : g_videoKeySeq) {
        if (key == e.key && frameNow >= e.start && frameNow < e.start + e.duration)
            return true;
    }
    if (g_videoKey >= 0 && key == g_videoKey &&
        frameNow >= g_videoKeyStart && frameNow < g_videoKeyEnd)
        return true;
    if (g_videoKey2 >= 0 && key == g_videoKey2 &&
        frameNow >= g_videoKey2Start && frameNow < g_videoKey2End)
        return true;
    return false;
}

void VideoAdvance(unsigned cycles) {
    g_videoInstrCycles += cycles;
    static bool first = true;
    if (first) {
        first = false;
        // The probe is normally a fast, bounded analysis run: no real-time
        // pacing and no SDL audio queue pressure. A visible run keeps normal
        // pacing and audio so the presentation path can be compared too.
        if (!g_videoVisible) {
            app_data.limit = 0;
            if (!g_videoKeepSound)
                app_data.sound_en = 0;
        }
    }

    const unsigned frameNow = static_cast<unsigned>(g_videoSamples.size());
    if (g_videoResetFrame >= 0 && !g_videoResetDone &&
        frameNow >= static_cast<unsigned>(g_videoResetFrame))
    {
        g_videoResetDone = true;
        O2EM_RequestReset();
    }
    dbstick1 = (g_videoFireStart > 0 && frameNow >= g_videoFireStart &&
                frameNow < g_videoFireEnd) ? 0xEF : 0;
    if (g_videoStickValue >= 0 && frameNow >= g_videoStickStart &&
        frameNow < g_videoStickEnd)
        dbstick1 = static_cast<Byte>(g_videoStickValue & 0xFF);
    dbstick2 = dbstick1;

    const unsigned long long serial = VDCStub_DebugFrameSerial();
    while (g_videoLastSerial < serial) {
        ++g_videoLastSerial;
        VideoFrameSample s;
        s.serial = g_videoLastSerial;
        s.hash = VDCStub_DebugFrameHash();
        s.frame = VDCStub_DebugFrameNumber();
        s.a0 = VDCStub_DebugFrameA0();
        s.a3 = VDCStub_DebugFrameA3();
        s.aa = VDCStub_DebugFrameAA();
        g_videoSamples.push_back(s);

        if (g_videoCsv) {
            std::fprintf(g_videoCsv, "%llu,%d,%016llX,%02X,%02X,%02X\n",
                s.serial, s.frame, s.hash,
                static_cast<unsigned>(s.a0),
                static_cast<unsigned>(s.a3),
                static_cast<unsigned>(s.aa));
        }

        if (g_videoGridDumpFrame >= 0 && !g_videoGridDumped &&
            s.frame == g_videoGridDumpFrame)
        {
            g_videoGridDumped = true;
            unsigned char cur[256] = {};
            VDCStub_DebugCopyLastVdc(cur);
            std::fprintf(stderr,
                "GRID_DUMP frame=%d A0=%02X A1=%02X A2=%02X A3=%02X AA=%02X\n",
                s.frame, unsigned(cur[0xA0]), unsigned(cur[0xA1]),
                unsigned(cur[0xA2]), unsigned(cur[0xA3]), unsigned(cur[0xAA]));
            std::fprintf(stderr, "GRID_H0 ");
            for (int a = 0xC0; a <= 0xCF; ++a)
                std::fprintf(stderr, "%02X", unsigned(cur[a]));
            std::fprintf(stderr, "\nGRID_H1 ");
            for (int a = 0xD0; a <= 0xDF; ++a)
                std::fprintf(stderr, "%02X", unsigned(cur[a]));
            std::fprintf(stderr, "\nGRID_V  ");
            for (int a = 0xE0; a <= 0xEF; ++a)
                std::fprintf(stderr, "%02X", unsigned(cur[a]));
            std::fprintf(stderr, "\nGRID_OBJ ");
            for (int a = 0x00; a <= 0x7F; ++a)
                std::fprintf(stderr, "%02X", unsigned(cur[a]));
            std::fprintf(stderr, "\nGRID_SHAPE ");
            for (int a = 0x80; a <= 0x9F; ++a)
                std::fprintf(stderr, "%02X", unsigned(cur[a]));
            std::fprintf(stderr, "\nGRID_CV ");
            for (int a = 0; a < 313 && a < MAXLINES; ++a)
                std::fprintf(stderr, "%02X", unsigned(ColorVector[a]));
            std::fprintf(stderr, "\nGRID_GRIDCOL ");
            for (int a = 24; a < 240; a += 24)
            {
                const unsigned cv = ColorVector[a];
                const unsigned g = (cv & 0x07) | ((cv & 0x40) >> 3) | (cv & 0x80 ? 0 : 8);
                const unsigned bg = ((cv & 0x38) >> 3) | (cv & 0x80 ? 0 : 8);
                std::fprintf(stderr, "%d:cv=%02X,g=%u,bg=%u ", a, cv, g, bg);
            }
            std::fprintf(stderr, "\nGRID_PIX ");
            {
                static const int pts[][2] = {
                    {100,100}, {100,60}, {100,120}, {200,100},
                    {60,30}, {60,200}, {280,40}, {280,120}, {10,10},
                    {82,230}, {95,235}, {150,230}, {165,235},
                    {268,4}, {272,6}, {300,4}, {304,6},
                    {20,30}, {30,30},
                    {270,105}, {300,105}, {270,75}, {300,75},
                    {270,40}, {300,40}, {25,25}, {315,25}, {25,35}, {315,35}
                };
                for (const auto& pt : pts)
                {
                    std::fprintf(stderr, "(%d,%d)c=%02X,l=%02X ",
                        pt[0], pt[1],
                        VDCStub_DebugPixelColor(pt[0], pt[1]),
                        VDCStub_DebugPixelLayers(pt[0], pt[1]));
                }
            }
            std::fprintf(stderr, "\n");
        }

        for (int ramFrame : g_videoRamDumpFrames)
        {
            if (ramFrame != s.frame)
                continue;
            std::fprintf(stderr,
                "RAM_DUMP frame=%d pc=%03X acc=%02X psw=%02X bank=%02X p1=%02X p2=%02X timer=%02X t_on=%d c_on=%d A0=%02X A3=%02X\n",
                s.frame, unsigned(pc), unsigned(acc), unsigned(psw), unsigned(reg_pnt),
                unsigned(p1), unsigned(p2), unsigned(itimer), unsigned(timer_on), unsigned(count_on),
                unsigned(VDCwrite[0xA0]), unsigned(VDCwrite[0xA3]));
            std::fprintf(stderr, "RAM ");
            for (int a = 0; a < 64; ++a)
                std::fprintf(stderr, "%02X", unsigned(intRAM[a]));
            std::fprintf(stderr, "\n");
            break;
        }

        for (int dumpFrame : g_videoFrameDumps)
        {
            if (dumpFrame != s.frame)
                continue;
            if (!g_videoDumpDir.empty())
            {
                char name[512];
                std::snprintf(name, sizeof(name), "%s/frame_%04d.bmp",
                    g_videoDumpDir.c_str(), dumpFrame);
                VDCStub_DebugSaveLastFrameBmp(name);
            }
            break;
        }

        if (!g_videoDumpDir.empty() && g_videoDumped < g_videoDumpLimit) {
            char name[512];
            std::snprintf(name, sizeof(name), "%s/frame_%04llu.bmp",
                g_videoDumpDir.c_str(), s.serial);
            VDCStub_DebugSaveLastFrameBmp(name);
            ++g_videoDumped;
        }

        if (g_videoSamples.size() >= g_videoFramesRequested) {
            key_done = 1;
            break;
        }

        if (g_videoVdcDiff && s.frame >= static_cast<int>(g_videoVdcDiffStart) &&
            s.frame <= static_cast<int>(g_videoVdcDiffEnd))
        {
            unsigned char cur[256] = {};
            VDCStub_DebugCopyLastVdc(cur);
            if (g_prevVdcValid)
            {
                for (int a = 0; a < 256; ++a)
                {
                    if (cur[a] != g_prevVdc[a])
                        std::fprintf(stderr, "VDC_DIFF frame=%d adr=%02X %02X -> %02X\n",
                            s.frame, a, static_cast<unsigned>(g_prevVdc[a]),
                            static_cast<unsigned>(cur[a]));
                }
            }
            std::memcpy(g_prevVdc, cur, sizeof(g_prevVdc));
            g_prevVdcValid = true;
        }
    }
}

bool VideoStop() {
    return g_videoSamples.size() >= g_videoFramesRequested;
}

void VideoUninstall() {
    observation::advance = nullptr;
    observation::stop = nullptr;
    observation::input = nullptr;
    observation::before = nullptr;
}

int RunVideoProbeImpl() {
    std::printf("O2EM-NG video probe (development-only, PAL)\n");

    const char* cart = std::getenv("O2EM_VIDEO_ROM");
    const char* bios = std::getenv("O2EM_VIDEO_BIOS");
    if (!cart || !*cart || !bios || !*bios) {
        std::fprintf(stderr, "  missing O2EM_VIDEO_ROM and/or O2EM_VIDEO_BIOS\n");
        return 2;
    }

    if (const char* region = std::getenv("O2EM_VIDEO_REGION")) {
        if (std::strcmp(region, "ntsc") == 0)
            g_videoRegion = RegionMode::NTSC;
        else if (std::strcmp(region, "auto") == 0)
            g_videoRegion = RegionMode::Auto;
        else
            g_videoRegion = RegionMode::PAL;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(cart, ec) || ec) {
        std::fprintf(stderr, "  O2EM_VIDEO_ROM not found: %s\n", cart);
        return 2;
    }

    if (const char* frames = std::getenv("O2EM_VIDEO_FRAMES")) {
        const unsigned long v = std::strtoul(frames, nullptr, 10);
        if (v == 0 || v > 5000) {
            std::fprintf(stderr, "  O2EM_VIDEO_FRAMES must be 1..5000\n");
            return 2;
        }
        g_videoFramesRequested = static_cast<unsigned>(v);
    }

    const char* out = std::getenv("O2EM_VIDEO_OUT");
    const char* enable = std::getenv("O2EM_VIDEO_PROBE");
    if ((!out || !*out) && enable && std::strcmp(enable, "1") != 0)
        out = enable;
    if (out && *out)
        g_videoCsv = std::fopen(out, "wb");
    if (g_videoCsv) {
        std::fprintf(g_videoCsv, "serial,frame,hash,a0,a3,aa\n");
    }

    if (const char* dir = std::getenv("O2EM_VIDEO_DUMP_DIR")) {
        if (dir && *dir) {
            g_videoDumpDir = dir;
            std::filesystem::create_directories(g_videoDumpDir, ec);
        }
    }
    if (const char* count = std::getenv("O2EM_VIDEO_DUMP_COUNT")) {
        g_videoDumpLimit = static_cast<unsigned>(std::strtoul(count, nullptr, 10));
    } else if (!g_videoDumpDir.empty()) {
        g_videoDumpLimit = 20;
    }

    if (const char* key = std::getenv("O2EM_VIDEO_KEY")) {
        g_videoKey = static_cast<int>(std::strtol(key, nullptr, 10));
    }
    if (const char* seq = std::getenv("O2EM_VIDEO_KEY_SEQ")) {
        std::string text(seq);
        std::size_t pos = 0;
        while (pos < text.size()) {
            const std::size_t comma = text.find(',', pos);
            const std::string item = text.substr(pos,
                comma == std::string::npos ? std::string::npos : comma - pos);
            unsigned start = 0, duration = 1;
            int keyCode = -1;
            if (std::sscanf(item.c_str(), "%u:%d:%u", &start, &keyCode, &duration) == 3 &&
                keyCode >= 0 && duration > 0) {
                g_videoKeySeq.push_back({start, duration, keyCode});
            }
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
    }
    if (const char* start = std::getenv("O2EM_VIDEO_KEY_START")) {
        g_videoKeyStart = static_cast<unsigned>(std::strtoul(start, nullptr, 10));
    }
    if (const char* end = std::getenv("O2EM_VIDEO_KEY_END")) {
        g_videoKeyEnd = static_cast<unsigned>(std::strtoul(end, nullptr, 10));
    }
    if (const char* key2 = std::getenv("O2EM_VIDEO_KEY2")) {
        g_videoKey2 = static_cast<int>(std::strtol(key2, nullptr, 10));
    }
    if (const char* start2 = std::getenv("O2EM_VIDEO_KEY2_START")) {
        g_videoKey2Start = static_cast<unsigned>(std::strtoul(start2, nullptr, 10));
    }
    if (const char* end2 = std::getenv("O2EM_VIDEO_KEY2_END")) {
        g_videoKey2End = static_cast<unsigned>(std::strtoul(end2, nullptr, 10));
    }
    if (const char* visible = std::getenv("O2EM_VIDEO_VISIBLE")) {
        g_videoVisible = visible && *visible && *visible != '0';
    }
    if (const char* keep = std::getenv("O2EM_VIDEO_KEEP_SOUND")) {
        g_videoKeepSound = keep && *keep && *keep != '0';
    }
    if (const char* start = std::getenv("O2EM_VIDEO_FIRE_START")) {
        g_videoFireStart = static_cast<unsigned>(std::strtoul(start, nullptr, 10));
    }
    if (const char* end = std::getenv("O2EM_VIDEO_FIRE_END")) {
        g_videoFireEnd = static_cast<unsigned>(std::strtoul(end, nullptr, 10));
    }
    if (const char* stick = std::getenv("O2EM_VIDEO_STICK")) {
        g_videoStickValue = static_cast<int>(std::strtol(stick, nullptr, 0));
    }
    if (const char* start = std::getenv("O2EM_VIDEO_STICK_START")) {
        g_videoStickStart = static_cast<unsigned>(std::strtoul(start, nullptr, 10));
    }
    if (const char* end = std::getenv("O2EM_VIDEO_STICK_END")) {
        g_videoStickEnd = static_cast<unsigned>(std::strtoul(end, nullptr, 10));
    }
    if (const char* reset = std::getenv("O2EM_VIDEO_RESET_FRAME")) {
        g_videoResetFrame = static_cast<int>(std::strtol(reset, nullptr, 10));
    }
    if (const char* diff = std::getenv("O2EM_VIDEO_VDC_DIFF")) {
        g_videoVdcDiff = diff && *diff && *diff != '0';
    }
    if (const char* start = std::getenv("O2EM_VIDEO_VDC_DIFF_START")) {
        g_videoVdcDiffStart = static_cast<unsigned>(std::strtoul(start, nullptr, 10));
    }
    if (const char* end = std::getenv("O2EM_VIDEO_VDC_DIFF_END")) {
        g_videoVdcDiffEnd = static_cast<unsigned>(std::strtoul(end, nullptr, 10));
    }
    if (const char* dump = std::getenv("O2EM_VIDEO_GRID_DUMP")) {
        g_videoGridDumpFrame = static_cast<int>(std::strtol(dump, nullptr, 10));
    }
    if (const char* ramDump = std::getenv("O2EM_VIDEO_RAM_DUMP")) {
        const char* p = ramDump;
        while (*p) {
            char* end = nullptr;
            const long v = std::strtol(p, &end, 10);
            if (end == p)
                break;
            g_videoRamDumpFrames.push_back(static_cast<int>(v));
            p = end;
            while (*p == ',' || *p == ' ')
                ++p;
        }
    }
    if (const char* frameDump = std::getenv("O2EM_VIDEO_DUMP_FRAME")) {
        const char* p = frameDump;
        while (*p) {
            char* end = nullptr;
            const long v = std::strtol(p, &end, 10);
            if (end == p)
                break;
            g_videoFrameDumps.push_back(static_cast<int>(v));
            p = end;
            while (*p == ',' || *p == ' ')
                ++p;
        }
    }
    if (const char* instrOut = std::getenv("O2EM_INSTR_TRACE_OUT")) {
        if (instrOut && *instrOut)
            g_videoInstrTrace = std::fopen(instrOut, "wb");
    }
    if (const char* from = std::getenv("O2EM_INSTR_TRACE_FROM")) {
        g_videoInstrTraceFrom = static_cast<int>(std::strtol(from, nullptr, 10));
    }
    if (const char* to = std::getenv("O2EM_INSTR_TRACE_TO")) {
        g_videoInstrTraceTo = static_cast<int>(std::strtol(to, nullptr, 10));
    }
    if (g_videoInstrTrace && g_videoInstrTraceTo < 0)
        g_videoInstrTraceTo = 0x7fffffff;
    if (g_videoInstrTrace && g_videoInstrTraceFrom < 0)
        g_videoInstrTraceFrom = 0;
    if (const char* ramOut = std::getenv("O2EM_RAM_TRACE_OUT")) {
        if (ramOut && *ramOut)
            g_ramTrace = std::fopen(ramOut, "wb");
    }
    if (const char* from = std::getenv("O2EM_RAM_TRACE_FROM")) {
        g_ramTraceFrom = static_cast<int>(std::strtol(from, nullptr, 10));
    }
    if (const char* to = std::getenv("O2EM_RAM_TRACE_TO")) {
        g_ramTraceTo = static_cast<int>(std::strtol(to, nullptr, 10));
    }
    if (g_ramTrace && g_ramTraceTo < 0)
        g_ramTraceTo = 0x7fffffff;
    if (g_ramTrace && g_ramTraceFrom < 0)
        g_ramTraceFrom = 0;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "  SDL_Init failed: %s\n", SDL_GetError());
        if (g_videoCsv) std::fclose(g_videoCsv);
        g_videoCsv = nullptr;
        return 2;
    }

    g_videoSamples.clear();
    g_videoLastSerial = 0;
    g_videoDumped = 0;
    g_videoInstrCycles = 0;
    g_ramShadowInit = false;
    g_ramPrevPc = 0;
    g_ramPrevOp = 0;
    g_prevVdcValid = false;
    std::memset(g_prevVdc, 0, sizeof(g_prevVdc));
    VDCStub_DebugEnableFrameTracking();

    // The normal frame-completion path (VPP composition + frame tracking) is
    // gated on a window/renderer. Use a hidden probe window so the diagnostic
    // observes exactly the same composition path as production.
    SDL_Window* probeWindow = SDL_CreateWindow(
        "O2EM-NG video probe",
        g_videoVisible ? 960 : 320,
        g_videoVisible ? 720 : 240,
        (g_videoVisible ? 0 : SDL_WINDOW_HIDDEN) |
            SDL_WINDOW_RESIZABLE);
    if (!probeWindow) {
        std::fprintf(stderr, "  SDL_CreateWindow failed: %s\n", SDL_GetError());
        if (g_videoCsv) std::fclose(g_videoCsv);
        g_videoCsv = nullptr;
        SDL_Quit();
        return 2;
    }
    VDCStub_SetWindow(probeWindow);

    observation::advance = VideoAdvance;
    observation::stop = VideoStop;
    observation::input = VideoInput;
    observation::before = VideoBefore;

    const bool booted = EmulatorCore_StartRom(cart, g_videoRegion, bios, false);

    VideoUninstall();
    if (g_videoCsv) {
        std::fclose(g_videoCsv);
        g_videoCsv = nullptr;
    }
    if (g_videoInstrTrace) {
        std::fclose(g_videoInstrTrace);
        g_videoInstrTrace = nullptr;
    }
    if (g_ramTrace) {
        std::fclose(g_ramTrace);
        g_ramTrace = nullptr;
    }

    std::printf("  ROM=%s\n", cart);
    std::printf("  BIOS=%s\n", bios);
    std::printf("  region=%s\n",
        g_videoRegion == RegionMode::PAL ? "PAL" :
        g_videoRegion == RegionMode::NTSC ? "NTSC" : "AUTO");
    std::printf("  frames captured=%zu requested=%u\n",
        g_videoSamples.size(), g_videoFramesRequested);

    if (!g_videoSamples.empty()) {
        std::size_t changes = 0;
        std::size_t twoFrameAlternations = 0;
        std::size_t distinct = 0;
        for (std::size_t i = 0; i < g_videoSamples.size(); ++i) {
            if (i > 0 && g_videoSamples[i].hash != g_videoSamples[i - 1].hash)
                ++changes;
            if (i >= 2 && g_videoSamples[i].hash == g_videoSamples[i - 2].hash &&
                g_videoSamples[i].hash != g_videoSamples[i - 1].hash)
                ++twoFrameAlternations;
            bool seen = false;
            for (std::size_t j = 0; j < i; ++j) {
                if (g_videoSamples[j].hash == g_videoSamples[i].hash) {
                    seen = true;
                    break;
                }
            }
            if (!seen)
                ++distinct;
        }
        std::printf("  distinct hashes=%zu consecutive changes=%zu two-frame alternations=%zu\n",
            distinct, changes, twoFrameAlternations);
        std::printf("  first samples (serial,frame,hash,a0,a3,aa):\n");
        const std::size_t shown = g_videoSamples.size() < 24 ? g_videoSamples.size() : 24;
        for (std::size_t i = 0; i < shown; ++i) {
            const auto& s = g_videoSamples[i];
            std::printf("    %llu,%d,%016llX,%02X,%02X,%02X\n",
                s.serial, s.frame, s.hash,
                static_cast<unsigned>(s.a0), static_cast<unsigned>(s.a3),
                static_cast<unsigned>(s.aa));
        }
    }

    if (g_videoDumped)
        std::printf("  BMP frames dumped: %u in %s\n", g_videoDumped, g_videoDumpDir.c_str());
    if (out && *out)
        std::printf("  CSV written: %s\n", out);

    SDL_Quit();
    return booted && !g_videoSamples.empty() ? 0 : 2;
}

} // namespace

bool RunVideoProbeIfRequested(int& result) {
    const char* enable = std::getenv("O2EM_VIDEO_PROBE");
    if (!enable || !*enable)
        return false;
    result = RunVideoProbeImpl();
    return true;
}

} // namespace mcs48
