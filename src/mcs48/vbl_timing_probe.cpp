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
#include <vector>

#include "legacy_observation.h"

#include "../../cpu.h"
#include "../../vmachine.h"
#include "../../emulator_core.h"
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

} // namespace mcs48
