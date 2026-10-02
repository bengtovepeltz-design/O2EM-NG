// Development-only Phase 2B fixture. Explicit opt-in, fresh-process use only.
// No normal-path ROM checks, settings/database access, or hardware fixes.
#include "legacy_observation.h"
#include "trace_record.h"
#include "input_schedule.h"
#include "../../cpu.h"
#include "../../audio.h"
#include "../../vmachine.h"
#include "keyboard.h"
#include "../../o2em_keys.h"
#include "../../emulator_core.h"
#include "../../crc32.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace mcs48::observation {
namespace {
std::unique_ptr<TraceRing> ring;
InputSchedule schedule;
std::uint64_t ticks = 0, endTick = 2048;
unsigned instructionPc = 0, instructionOp = 0;
bool zeroDown = false, finished = false;
unsigned lastFlags = 0;
FILE* irqLog = nullptr;
unsigned irqSource = 0;

TraceEvent State(unsigned kind, unsigned address = 0, unsigned value = 0) {
    TraceEvent e{};
    e.machineCycles = ticks;
    e.kind = static_cast<TraceEventKind>(kind);
    e.pc = instructionPc; e.opcode = static_cast<std::uint8_t>(instructionOp);
    e.a = acc;
    // Assemble locally: calling make_psw_debug would mutate the live PSW.
    e.psw = static_cast<std::uint8_t>((cy << 7) | ac | f0 | bs | 8 | ((sp - 8) >> 1));
    e.timerValue = itimer; e.prescaler = static_cast<std::uint8_t>(master_count);
    e.timerMode = timer_on | (count_on << 1);
    e.irqFlags = (t_flag ? 1 : 0) | (xirq_en ? 2 : 0) | (tirq_en ? 4 : 0)
        | (xirq_pend ? 8 : 0) | (tirq_pend ? 16 : 0) | (irq_ex ? 32 : 0)
        | (f0 ? 64 : 0) | (f1 ? 128 : 0);
    e.regBank = reg_pnt; e.p1Latch = p1; e.p2Latch = p2;
    // Pure mirrors of this machine's pin providers, no side-effectful reads.
    e.t0 = 0; // current voice_stub get_voice_status() is identically zero
    e.t1 = ((h_clk > 16) || (master_clk > VBLCLK)) ? 1 : 0;
    for (unsigned i = 0; i < 8; ++i) e.registers[i] = intRAM[reg_pnt + i];
    e.payloadAddress = static_cast<std::uint16_t>(address);
    e.payloadValue = static_cast<std::uint8_t>(value);
    return e;
}
void Record(unsigned kind, unsigned address, unsigned value) {
    ring->Observe(State(kind, address, value));
    if (kind == 13) irqSource = address;
    if (irqLog && (kind == 13 || kind == 14 || kind == 15 || kind == 17 || (kind == 7 && address == 0))) {
        unsigned status = (VDCwrite[0xA0] & 2) | (master_clk > VBLCLK ? 8 : 0)
            | (h_clk < LINECNT - 7 ? 1 : 0) | (sound_IRQ ? 4 : 0);
        std::fprintf(irqLog, "%llu,%u,%u,%d,%d,%02X,%02X,%d,%u,%03X,%03X,%02X\n",
            static_cast<unsigned long long>(ticks),kind,irqSource,master_clk,h_clk,
            VDCwrite[0xA0],status,int_clk,irq_ex,pc,instructionPc,value);
    }
}
void Pre(unsigned location, unsigned opcode) {
    instructionPc = location; instructionOp = opcode;
    ScheduledInput inputEvent;
    while (schedule.PopDue(ticks, inputEvent)) {
        zeroDown = inputEvent.action == InputAction::KeyDown;
        Record(12, inputEvent.key, zeroDown ? 1 : 0);
    }
    const auto state = State(0);
    if (state.irqFlags != lastFlags) Record(7, 2, state.irqFlags);
    lastFlags = state.irqFlags;
    ring->Observe(state);
    if (ticks >= endTick) { finished = true; key_done = 1; }
}
void Tick(unsigned cycles) { ticks += cycles; }
bool StopNow() { return finished; }
bool Input(int key) { return key == KEY_0 && zeroDown; }
bool Identity(const char* path, std::uintmax_t size, unsigned long crc) {
    std::error_code ec;
    return std::filesystem::file_size(path, ec) == size && !ec && crc32_file(path) == crc;
}
bool Dump(const char* path) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "seq,cycle,kind,pc,op,a,psw,timer,prescaler,mode,irq,rb,p1,p2,t0,t1,address,value,r0,r1,r2,r3,r4,r5,r6,r7\n");
    for (std::size_t i = 0; i < ring->Count(); ++i) {
        const auto& e = ring->At(i);
        std::fprintf(f, "%llu,%llu,%u,%03X,%02X,%02X,%02X,%02X,%02X,%u,%u,%02X,%u,%02X,%02X,%u,%03X,%02X",
            static_cast<unsigned long long>(e.seq), static_cast<unsigned long long>(e.machineCycles),
            static_cast<unsigned>(e.kind), e.pc, e.opcode, e.a, e.psw, e.timerValue,
            e.prescaler, e.timerMode, e.irqFlags, e.regBank, e.p1Latch, e.p2Latch,
            e.t0, e.t1, e.payloadAddress, e.payloadValue);
        for (unsigned i = 0; i < 8; ++i) std::fprintf(f, ",%02X", e.registers[i]);
        std::fprintf(f, "\n");
    }
    const bool ok = !std::ferror(f);
    std::fclose(f); return ok;
}
}

bool RunFixtureIfRequested(int& result) {
    const char* output = std::getenv("O2EM_PHASE2B_TRACE");
    if (!output || !*output) return false;
    result = 2;
    const char* cart = std::getenv("O2EM_PHASE2B_ROM");
    const char* bios = std::getenv("O2EM_PHASE2B_BIOS");
    if (!cart || !bios || !Identity(cart, 4096, 0x69D21F8FUL)
        || !Identity(bios, 1024, 0x8016A315UL)) {
        std::fprintf(stderr, "Phase2B: fixture ROM/BIOS identity mismatch.\n"); return true;
    }
    if (const char* limit = std::getenv("O2EM_PHASE2B_CYCLES")) {
        endTick = std::strtoull(limit, nullptr, 10);
        if (!endTick || endTick > 120000) return true;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) return true;
    ring = std::make_unique<TraceRing>(8192);
    TraceRing::enabled = true;
    schedule.Add(72590, KEY_0, InputAction::KeyDown);
    schedule.Add(79849, KEY_0, InputAction::KeyUp);
    if (const char* path = std::getenv("O2EM_IRQ_PROBE")) irqLog = std::fopen(path, "wb");
    before = Pre; advance = Tick; event = Record; stop = StopNow; input = Input;
    const bool ok = EmulatorCore_StartRom(cart, RegionMode::PAL, bios, false);
    before = nullptr; advance = nullptr; event = nullptr; stop = nullptr; input = nullptr;
    if (irqLog) { std::fclose(irqLog); irqLog = nullptr; }
    const bool written = Dump(output);
    TraceRing::enabled = false;
    ring.reset(); SDL_Quit();
    result = ok && finished && written ? 0 : 2;
    return true;
}
}
