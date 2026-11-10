// ============================================================================
// src/mcs48/g7400_freeze_trace.cpp
//
// See g7400_freeze_trace.h. Observational, bounded, opt-in.
// ============================================================================

#include "g7400_freeze_trace.h"
#include "legacy_observation.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../version.h"
#include "../../vmachine.h"
#include "../../cpu.h"
#include "../../audio.h"

namespace mcs48
{
namespace
{
constexpr std::uint32_t kInstrCap = 131072u; // ~0.5-1 s of 8048 instructions
constexpr std::uint32_t kVdcCap = 4096u;
constexpr std::uint32_t kVppCap = 8192u;
constexpr std::uint32_t kEventCap = 4096u;
constexpr std::uint32_t kA0Cap = 4096u;
constexpr std::uint32_t kVdc24Cap = 2048u;
constexpr std::uint32_t kWaiteCap = 256u;
constexpr std::uint32_t kRetCap = 256u;
constexpr unsigned kPre = 64u;
constexpr std::uint32_t kIrqWindow = 512u;
constexpr std::uint32_t kPersistentThreshold = 20000u;
constexpr std::uint32_t kRasterCap = 256u;        // bounded A4 wait samples
constexpr std::uint32_t kRasterNearVbl = 384u;    // "close to VBL" window (master cycles)
constexpr std::uint32_t kRasterNearStride = 8u;   // throttle steady A4 reads near VBL
constexpr std::uint32_t kCpuDumpMax = 8192u;   // lines written to the log
constexpr std::uint32_t kPollWindow = 32768u;  // for frequency statistics
constexpr std::uint32_t kLoopWindow = 4096u;   // for period detection
constexpr unsigned long kHelicopterCrc = 0xC4134DF8ul;

// Debug builds auto-arm the VP59 raster diagnostic without any environment
// variable. Release builds stay strictly env-gated (production behavior).
bool FreezeTraceAutoArmBuildEnabled()
{
#if defined(_DEBUG)
    return true;
#else
    return false;
#endif
}

bool FreezeTraceAutoArmForCrc(unsigned long crc)
{
    // The development video probe installs its own bounded observation
    // callbacks. Do not let the Debug auto-arm replace them.
    const char* videoProbe = std::getenv("O2EM_VIDEO_PROBE");
    if (videoProbe && *videoProbe)
        return false;
    return FreezeTraceAutoArmBuildEnabled() && crc == kHelicopterCrc;
}

struct Instr
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t pc;
    std::uint16_t frame;
    std::uint8_t op;
    std::uint8_t acc;
    std::uint8_t psw;
    std::uint8_t flags; // bit0 mstate,1 irq_ex,2 xirq_en,3 tirq_en,4 timer_on,5 count_on
    std::uint8_t regs[8]{}; // ACTIVE bank R0..R7 at Before() (intRAM[reg_pnt+0..7])
};

struct VdcRead
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t pc;
    std::uint16_t frame;
    std::uint8_t adr;
    std::uint8_t value;
    std::uint16_t aux;
    std::uint8_t mstate;
    std::uint8_t pad;
};

// One sampled $A4 read during the AAA/AAB/AAC raster wait.
struct RasterSample
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint32_t waitEntryMaster;
    std::uint16_t frame;
    std::uint16_t pc;
    std::uint8_t p1;
    std::uint8_t a0;
    std::uint8_t a4;
    std::uint8_t r5;
    std::int8_t delta;       // A4 - R5 (signed, clamped to int8)
    std::uint8_t flags;      // bit0 in_wait, bit1 first_after_entry, bit2 near_vbl, bit3 a4=FF clamp
    std::uint8_t missed;     // 1 if A4 > target R5
    std::uint32_t waitSeq;   // links to WAIT ENTRIES
};

struct VppAccess
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t pc;
    std::uint8_t write;
    std::uint8_t adr;
    std::uint8_t value;
    std::uint8_t p1;
    std::uint16_t pad;
};

struct EventRec
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t kind;
    std::uint16_t a;
    std::uint16_t b;
    std::uint8_t mstate;
    std::uint8_t pad;
};

// VDC address 0x24 activity only (read/write), bounded ring.
struct Vdc24Event
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t frame;
    std::uint16_t pc;
    std::uint8_t dir;       // 0 = read, 1 = write
    std::uint8_t requested; // write value requested (0 for read)
    std::uint8_t returned;  // read value returned (0 for write)
    std::uint8_t p1;
    std::uint8_t a0;
    std::uint8_t before;    // VDCwrite[0x24] before
    std::uint8_t after;     // VDCwrite[0x24] after
    std::uint8_t flags;     // bit0 write blocked, bit1 object-approx, bit2 mirror, bit3 other
    std::uint16_t pad;
};

// One instruction in the prehistory / IRQ window.
struct PreInstr
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t frame;
    std::uint16_t pc;
    std::uint8_t op;
    std::uint8_t acc;
    std::uint8_t psw;
    std::uint8_t irqFlags; // bit0 irq_ex, bit1 xirq_en, bit2 tirq_en, bit3 mstate
    std::uint8_t regs[8];
    std::uint8_t p1;
    std::uint8_t a0;
};

// A new entry into the generic AAA/AAB/AAC wait sequence (entry at any of the
// three PCs).
struct WaitEntry
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint32_t entrySeq;
    std::uint32_t instrsSinceVector;
    std::uint32_t lastVectorSeq;
    std::uint32_t lastVectorMaster;
    std::uint16_t frame;
    std::uint16_t prevPc;
    std::uint16_t entryPc;      // AAA / AAB / AAC
    std::uint16_t lastVectorFrame;
    std::uint8_t prevOp;
    std::uint8_t acc;
    std::uint8_t psw;
    std::uint8_t p1;
    std::uint8_t a0;
    std::uint8_t irqEx;
    std::uint8_t xirqEn;
    std::uint8_t mstate;
    std::uint8_t bank;
    std::uint8_t entryType; // 0 CALL, 1 JMP, 2 OTHER
    std::uint8_t haveVector;
    std::uint8_t preCount;
    std::uint8_t regs[8];
    PreInstr pre[64];
};

struct WaitDone
{
    std::uint32_t entrySeq;
    std::uint32_t master;
    std::uint32_t durationInstrs;
    std::uint16_t frame;
    std::uint16_t callerPc;
    std::uint16_t entryPc;
    std::uint8_t r0;
    std::uint8_t r5;
    std::uint8_t a0;
    std::uint8_t p1;
    std::uint8_t irqEx;
    std::uint8_t xirqEn;
    std::uint8_t mstate;
    std::uint8_t exitAcc;
};

struct RetEvent
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t frame;
    std::uint16_t pc;
    std::uint16_t nextPc;      // observed next PC (0xFFFF = unknown)
    std::uint16_t retTarget;   // observational stack-derived return PC
    std::uint8_t op;
    std::uint8_t psw;
    std::uint8_t irqExBefore;
    std::uint8_t irqExAfter;
    std::uint8_t haveAfter;
    std::uint8_t sp;
    std::uint8_t stackHigh;    // first pulled byte (high PC nibble + PSW restore)
    std::uint8_t stackLow;     // second pulled byte (low PC)
    std::uint8_t retBank;      // bank bit decoded from stackHigh
    std::uint8_t stack[16];
};

struct A0Write
{
    std::uint32_t seq;
    std::uint32_t master;
    std::uint16_t frame;
    std::uint16_t pc;
    std::uint8_t prev;
    std::uint8_t next;
    std::uint8_t yBefore;
    std::uint8_t yAfter;
    std::uint8_t hclk;
    std::uint8_t mstate;
    std::uint8_t flags; // bit0 falling(1->0), bit1 clamped(raw>241), bit2 rising(0->1)
    std::uint8_t pad;
    std::uint16_t rawLine;
    std::uint16_t pad2;
};

// --- state ------------------------------------------------------------------
bool g_envEnabled = false;
bool g_armed = false;
bool g_written = false;
bool g_dumping = false;

std::string g_romPath;
unsigned long g_romCrc = 0;
std::string g_bios;
std::string g_region;
int g_vpp = 0;

std::vector<Instr> g_instr(kInstrCap);
std::uint32_t g_instrCount = 0;
std::uint64_t g_cycles = 0;

std::vector<VdcRead> g_vdc(kVdcCap);
std::uint32_t g_vdcCount = 0;
std::uint32_t g_vdcA1 = 0, g_vdcA2 = 0, g_vdcA4A5 = 0, g_vdcWrite = 0;

std::vector<VppAccess> g_vppAcc(kVppCap);
std::uint32_t g_vppCount = 0;
std::uint32_t g_vppReads = 0, g_vppWrites = 0;

std::vector<EventRec> g_events(kEventCap);
std::uint32_t g_eventCount = 0;
std::uint32_t g_vblCount = 0, g_irqReq = 0, g_extIrq = 0, g_frameEnd = 0;
std::uint32_t g_vectorEntries = 0;

std::vector<A0Write> g_a0(kA0Cap);
std::uint32_t g_a0Count = 0;
std::uint32_t g_a0Rising = 0, g_a0Falling = 0, g_a0FallGt241 = 0, g_a0FallYff = 0;
A0Write g_lastA0Fall{};
bool g_haveLastA0Fall = false;

std::vector<Vdc24Event> g_vdc24(kVdc24Cap);
std::uint32_t g_vdc24Count = 0;
std::uint32_t g_vdc24Reads = 0, g_vdc24Writes = 0;
std::uint32_t g_vdc24Req3E = 0, g_vdc24Stored3E = 0;
Vdc24Event g_lastVdc24Write{};
bool g_haveLastVdc24Write = false;
Vdc24Event g_lastVdc24Read{};
bool g_haveLastVdc24Read = false;

// Transition tracing for the AAA/AAB/AAC wait sequence.
std::vector<WaitEntry> g_waitRing(kWaiteCap);
std::uint32_t g_waitCount = 0;
std::uint32_t g_waitEntries = 0;
PreInstr g_pre[kPre];
std::uint32_t g_preCount = 0;
std::uint16_t g_prevPc = 0;
std::uint8_t g_prevOp = 0;
bool g_havePrevPc = false;
bool g_inWait = false;
bool g_haveCandidate = false;
WaitEntry g_candidate{};
std::uint32_t g_candidateInstrs = 0;
// First-ever and last (final) persistent R0=0x24 wait entry records.
WaitEntry g_firstPersistentWait{};
bool g_haveFirstPersistent = false;
WaitEntry g_lastPersistentWait{};
bool g_havePersistent = false;
std::uint32_t g_persistentCrossings = 0;
std::uint32_t g_persistentEntryPcCount[3] = {}; // index 0=AAA 1=AAB 2=AAC
WaitDone g_lastSuccessful24{};
bool g_haveSuccessful24 = false;
std::vector<RetEvent> g_retRing(kRetCap);
std::uint32_t g_retCount = 0;
bool g_retPending = false;
std::uint32_t g_retPendingIdx = 0;
std::uint32_t g_lastRetrSeq = 0;
std::uint32_t g_lastRetrMaster = 0;
std::uint16_t g_lastRetrFrame = 0, g_lastRetrPc = 0;
bool g_haveRetr = false;
std::uint32_t g_lastVectorSeq = 0;
std::uint32_t g_lastVectorMaster = 0;
std::uint16_t g_lastVectorFrame = 0;
bool g_haveVector = false;
std::uint32_t g_irqReqAfterPersistent = 0;
std::uint32_t g_irqVecAfterPersistent = 0;

// Post-vector capture windows.
std::vector<PreInstr> g_irqWindow(kIrqWindow);      // current, from last PC=003
std::uint32_t g_irqWindowCount = 0;
bool g_irqWindowActive = false;
std::uint16_t g_irqWindowVectorFrame = 0;
std::vector<PreInstr> g_candidateWindow(kIrqWindow); // snapshot at candidate entry
std::uint32_t g_candidateWindowCount = 0;
bool g_candidateHasWindow = false;
std::vector<PreInstr> g_finalWindow(kIrqWindow);      // preserved for final persistent
std::uint32_t g_finalWindowCount = 0;
bool g_haveFinalWindow = false;
std::uint16_t g_finalWindowVectorFrame = 0;
std::vector<PreInstr> g_successWindow(kIrqWindow);    // last successful IRQ path
std::uint32_t g_successWindowCount = 0;
bool g_haveSuccessWindow = false;
std::uint16_t g_successWindowVectorFrame = 0;
bool g_retrPendingForWindow = false;

// Raster-wait ($A4) target-miss diagnostics: bounded samples plus one retained
// miss record. Diagnostic only; no behavior change.
std::vector<RasterSample> g_rasterRing(kRasterCap);
std::uint32_t g_rasterCount = 0;
bool g_rasterEntryPending = false;
std::uint32_t g_rasterPendingWaitSeq = 0;
std::uint32_t g_rasterPendingEntryMaster = 0;
std::uint8_t g_rasterPendingTarget = 0;
std::uint32_t g_rasterWaitEntries = 0;
std::uint32_t g_rasterTargetMisses = 0;
std::uint32_t g_rasterExactHits = 0;
std::uint32_t g_rasterBehindTarget = 0;
bool g_rasterLastDeltaValid = false;
std::int16_t g_rasterLastDelta = 0;
std::uint32_t g_rasterNearStrideCounter = 0;
RasterSample g_lastRasterMiss{};
bool g_haveRasterMiss = false;
RasterSample g_rasterAtVbl{};
bool g_haveRasterAtVbl = false;
RasterSample g_rasterAtVector{};
bool g_haveRasterAtVector = false;
RasterSample g_lastRasterA4Context{};
bool g_haveLastRasterA4Context = false;

std::uint16_t g_lastA2PreClear = 0;

void ResetAll()
{
    g_instrCount = 0;
    g_cycles = 0;
    g_vdcCount = 0;
    g_vdcA1 = g_vdcA2 = g_vdcA4A5 = g_vdcWrite = 0;
    g_vppCount = 0;
    g_vppReads = g_vppWrites = 0;
    g_eventCount = 0;
    g_vblCount = g_irqReq = g_extIrq = g_frameEnd = 0;
    g_vectorEntries = 0;
    g_lastA2PreClear = 0;
    g_a0Count = 0;
    g_a0Rising = g_a0Falling = g_a0FallGt241 = g_a0FallYff = 0;
    g_lastA0Fall = A0Write{};
    g_haveLastA0Fall = false;
    g_vdc24Count = 0;
    g_vdc24Reads = g_vdc24Writes = g_vdc24Req3E = g_vdc24Stored3E = 0;
    g_lastVdc24Write = Vdc24Event{};
    g_haveLastVdc24Write = false;
    g_lastVdc24Read = Vdc24Event{};
    g_haveLastVdc24Read = false;
    g_waitCount = 0;
    g_waitEntries = 0;
    g_preCount = 0;
    g_prevPc = 0; g_prevOp = 0; g_havePrevPc = false;
    g_inWait = false;
    g_haveCandidate = false;
    g_candidate = WaitEntry{};
    g_candidateInstrs = 0;
    g_firstPersistentWait = WaitEntry{};
    g_haveFirstPersistent = false;
    g_lastPersistentWait = WaitEntry{};
    g_havePersistent = false;
    g_persistentCrossings = 0;
    g_persistentEntryPcCount[0] = g_persistentEntryPcCount[1] = g_persistentEntryPcCount[2] = 0;
    g_lastSuccessful24 = WaitDone{};
    g_haveSuccessful24 = false;
    g_retCount = 0;
    g_retPending = false;
    g_retPendingIdx = 0;
    g_lastRetrSeq = 0; g_lastRetrMaster = 0; g_lastRetrFrame = 0; g_lastRetrPc = 0;
    g_haveRetr = false;
    g_lastVectorSeq = 0; g_lastVectorMaster = 0; g_lastVectorFrame = 0;
    g_haveVector = false;
    g_irqReqAfterPersistent = 0;
    g_irqVecAfterPersistent = 0;
    g_irqWindowCount = 0;
    g_irqWindowActive = false;
    g_irqWindowVectorFrame = 0;
    g_candidateWindowCount = 0;
    g_candidateHasWindow = false;
    g_finalWindowCount = 0;
    g_haveFinalWindow = false;
    g_finalWindowVectorFrame = 0;
    g_successWindowCount = 0;
    g_haveSuccessWindow = false;
    g_successWindowVectorFrame = 0;
    g_retrPendingForWindow = false;
    g_rasterCount = 0;
    g_rasterEntryPending = false;
    g_rasterPendingWaitSeq = 0;
    g_rasterPendingEntryMaster = 0;
    g_rasterPendingTarget = 0;
    g_rasterWaitEntries = 0;
    g_rasterTargetMisses = 0;
    g_rasterExactHits = 0;
    g_rasterBehindTarget = 0;
    g_rasterLastDeltaValid = false;
    g_rasterLastDelta = 0;
    g_rasterNearStrideCounter = 0;
    g_lastRasterMiss = RasterSample{};
    g_haveRasterMiss = false;
    g_rasterAtVbl = RasterSample{};
    g_haveRasterAtVbl = false;
    g_rasterAtVector = RasterSample{};
    g_haveRasterAtVector = false;
    g_lastRasterA4Context = RasterSample{};
    g_haveLastRasterA4Context = false;
}

// --- cheap p1 classifiers (read-only) ---------------------------------------
bool IsVdcAccess() { return ((p1 & 0x08) == 0) && ((p1 & 0x40) == 0); }
bool IsVppRead() { return (p1 & 0x10) && ((p1 & 0x20) == 0); }
bool IsVppWrite() { return (p1 & 0x08) && (p1 & (0x10 | 0x40)) && ((p1 & 0x20) == 0); }

unsigned A1Aux()
{
    unsigned aux = 0;
    if (master_clk > VM_EffectiveVblClk()) aux |= 0x01;
    if (h_clk < (LINECNT - 7)) aux |= 0x02;
    if (sound_IRQ) aux |= 0x04;
    aux |= (static_cast<unsigned>(VDCwrite[0xA0]) << 8);
    return aux;
}

// --- recording (bounded ring buffers) ---------------------------------------
void RecInstr(unsigned pcArg, unsigned opArg)
{
    Instr& e = g_instr[g_instrCount % kInstrCap];
    e.seq = g_instrCount;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.pc = static_cast<std::uint16_t>(pcArg);
    e.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    e.op = static_cast<std::uint8_t>(opArg);
    e.acc = acc;
    e.psw = psw;
    e.flags = static_cast<std::uint8_t>(
        (mstate ? 0x01 : 0) | (irq_ex ? 0x02 : 0) | (xirq_en ? 0x04 : 0) |
        (tirq_en ? 0x08 : 0) | (timer_on ? 0x10 : 0) | (count_on ? 0x20 : 0));
    // Active register bank R0..R7 (observational read only).
    for (int i = 0; i < 8; ++i)
        e.regs[i] = intRAM[reg_pnt + i];
    ++g_instrCount;
    if (pcArg == 0x003)
        ++g_vectorEntries;
}

void RecVdc(unsigned adr, unsigned value, unsigned aux)
{
    VdcRead& e = g_vdc[g_vdcCount % kVdcCap];
    e.seq = g_vdcCount;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.pc = static_cast<std::uint16_t>(pc);
    e.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    e.adr = static_cast<std::uint8_t>(adr);
    e.value = static_cast<std::uint8_t>(value);
    e.aux = static_cast<std::uint16_t>(aux);
    e.mstate = static_cast<std::uint8_t>(mstate);
    ++g_vdcCount;
}

void RecVpp(bool write, unsigned adr, unsigned value)
{
    VppAccess& e = g_vppAcc[g_vppCount % kVppCap];
    e.seq = g_vppCount;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.pc = static_cast<std::uint16_t>(pc);
    e.write = write ? 1 : 0;
    e.adr = static_cast<std::uint8_t>(adr);
    e.value = static_cast<std::uint8_t>(value);
    e.p1 = p1;
    ++g_vppCount;
    if (write) ++g_vppWrites; else ++g_vppReads;
}

void RecEvent(unsigned kind, unsigned a, unsigned b)
{
    EventRec& e = g_events[g_eventCount % kEventCap];
    e.seq = g_eventCount;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.kind = static_cast<std::uint16_t>(kind);
    e.a = static_cast<std::uint16_t>(a);
    e.b = static_cast<std::uint16_t>(b);
    e.mstate = static_cast<std::uint8_t>(mstate);
    ++g_eventCount;
}

// --- observation callbacks --------------------------------------------------
void OnAdvance(unsigned cycles) { g_cycles += cycles; }

bool IsCallOp(unsigned op)
{
    switch (op) { case 0x14: case 0x34: case 0x54: case 0x74:
                  case 0x94: case 0xB4: case 0xD4: case 0xF4: return true; default: return false; }
}
bool IsJmpOp(unsigned op)
{
    switch (op) { case 0x04: case 0x24: case 0x44: case 0x64:
                  case 0x84: case 0xA4: case 0xC4: case 0xE4: return true; default: return false; }
}
bool IsLoopPc(unsigned pcv) { return pcv == 0xAAA || pcv == 0xAAB || pcv == 0xAAC; }

// --- raster-wait ($A4) target-miss instrumentation --------------------------
int RasterDeltaOf(unsigned a4, unsigned r5)
{
    return static_cast<int>(a4) - static_cast<int>(r5);
}

void PushRasterSample(const RasterSample& s)
{
    g_rasterRing[g_rasterCount % kRasterCap] = s;
    ++g_rasterCount;
}

// Called from the external-read observation for $A4, i.e. while the instruction
// that performed the read is still executing. g_prevPc is that instruction's PC
// (set at the end of TraceTransition).
void NoteRasterA4(unsigned value)
{
    const unsigned a4 = value & 0xFFu;
    const bool inWait = IsLoopPc(g_prevPc);
    const bool first = g_rasterEntryPending;
    const unsigned r5live = intRAM[reg_pnt + 5];
    const unsigned target = first ? static_cast<unsigned>(g_rasterPendingTarget) : r5live;
    const int vblClk = VM_EffectiveVblClk();
    const bool nearVbl = (master_clk >= (vblClk - static_cast<int>(kRasterNearVbl))) &&
                         (master_clk <= (vblClk + static_cast<int>(kRasterNearVbl)));

    RasterSample s{};
    s.seq = g_rasterCount;
    s.master = static_cast<std::uint32_t>(master_clk);
    s.waitEntryMaster = g_rasterPendingEntryMaster;
    s.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    s.pc = g_prevPc;
    s.p1 = p1;
    s.a0 = VDCwrite[0xA0];
    s.a4 = static_cast<std::uint8_t>(a4);
    s.r5 = static_cast<std::uint8_t>(target);
    const int d = RasterDeltaOf(a4, target);
    s.delta = static_cast<std::int8_t>(d < -127 ? -127 : (d > 127 ? 127 : d));
    s.flags = static_cast<std::uint8_t>(
        (inWait ? 0x01 : 0) | (first ? 0x02 : 0) | (nearVbl ? 0x04 : 0) |
        (a4 == 0xFFu ? 0x08 : 0));
    s.missed = (a4 > target) ? 1 : 0;
    s.waitSeq = g_rasterPendingWaitSeq;

    g_lastRasterA4Context = s;
    g_haveLastRasterA4Context = true;

    bool record = false;
    if (first)
    {
        record = true;
        if (a4 > target) { ++g_rasterTargetMisses; g_lastRasterMiss = s; g_haveRasterMiss = true; }
        else if (a4 == target) ++g_rasterExactHits;
        else ++g_rasterBehindTarget;
        g_rasterEntryPending = false;
        g_rasterLastDelta = static_cast<std::int16_t>(d);
        g_rasterLastDeltaValid = true;
    }
    else if (inWait)
    {
        // One row per delta change (line progression), plus a throttled sample
        // close to VBL so the FF clamp / spin state is visible. Bounded.
        if (!g_rasterLastDeltaValid || d != static_cast<int>(g_rasterLastDelta))
        {
            record = true;
            g_rasterLastDelta = static_cast<std::int16_t>(d);
            g_rasterLastDeltaValid = true;
        }
        else if (nearVbl)
        {
            if ((g_rasterNearStrideCounter++ % kRasterNearStride) == 0)
                record = true;
        }
    }

    if (record)
        PushRasterSample(s);
}

void NoteRasterVbl()
{
    if (!g_haveLastRasterA4Context)
        return;
    RasterSample s = g_lastRasterA4Context;
    s.seq = g_rasterCount;
    s.master = static_cast<std::uint32_t>(master_clk);
    s.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    s.p1 = p1;
    s.a0 = VDCwrite[0xA0];
    s.flags |= 0x04; // near VBL
    g_rasterAtVbl = s;
    g_haveRasterAtVbl = true;
}

void NoteRasterVector()
{
    if (!g_haveLastRasterA4Context)
        return;
    RasterSample s = g_lastRasterA4Context;
    s.seq = g_rasterCount;
    s.master = static_cast<std::uint32_t>(master_clk);
    s.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    s.pc = 0x003;
    s.p1 = p1;
    s.a0 = VDCwrite[0xA0];
    g_rasterAtVector = s;
    g_haveRasterAtVector = true;
}

void TraceTransition(unsigned pcv, unsigned opv)
{
    // Resolve a pending RET/RETR: capture the observed next PC and post irq_ex.
    if (g_retPending)
    {
        RetEvent& re = g_retRing[g_retPendingIdx];
        re.irqExAfter = static_cast<std::uint8_t>(irq_ex);
        re.nextPc = static_cast<std::uint16_t>(pcv);
        re.haveAfter = 1;
        g_retPending = false;
    }

    // A RETR was observed; if the next observation shows irq_ex cleared while a
    // post-vector window is active, preserve it as the successful IRQ reference.
    if (g_retrPendingForWindow)
    {
        g_retrPendingForWindow = false;
        if (g_irqWindowActive && irq_ex == 0)
        {
            const std::uint32_t n = (g_irqWindowCount < kIrqWindow) ? g_irqWindowCount : kIrqWindow;
            for (std::uint32_t i = 0; i < n; ++i) g_successWindow[i] = g_irqWindow[i];
            g_successWindowCount = n;
            g_successWindowVectorFrame = g_irqWindowVectorFrame;
            g_haveSuccessWindow = true;
        }
    }

    // IRQ vector entry (PC=003): restart the post-vector capture window.
    if (pcv == 0x003)
    {
        g_lastVectorSeq = g_instrCount;
        g_lastVectorFrame = static_cast<std::uint16_t>(frame & 0xFFFF);
        g_lastVectorMaster = static_cast<std::uint32_t>(master_clk);
        g_haveVector = true;
        if (g_havePersistent) ++g_irqVecAfterPersistent;
        g_irqWindowActive = true;
        g_irqWindowCount = 0;
        g_irqWindowVectorFrame = static_cast<std::uint16_t>(frame & 0xFFFF);
        NoteRasterVector();
    }

    // RET / RETR.
    if (opv == 0x83u || opv == 0x93u)
    {
        RetEvent& re = g_retRing[g_retCount % kRetCap];
        re.seq = g_retCount;
        re.master = static_cast<std::uint32_t>(master_clk);
        re.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
        re.pc = static_cast<std::uint16_t>(pcv);
        re.nextPc = 0xFFFF;
        re.op = static_cast<std::uint8_t>(opv);
        re.psw = psw;
        re.irqExBefore = static_cast<std::uint8_t>(irq_ex);
        re.irqExAfter = 0;
        re.haveAfter = 0;
        re.sp = sp;
        // Observational return-target decode (no stack/CPU state change):
        //   pull(): sp--; if (sp<8) sp=23; return intRAM[sp]
        std::uint8_t s1 = static_cast<std::uint8_t>(sp - 1);
        if (s1 < 8) s1 = 23;
        std::uint8_t s2 = static_cast<std::uint8_t>(s1 - 1);
        if (s2 < 8) s2 = 23;
        re.stackHigh = intRAM[s1];
        re.stackLow = intRAM[s2];
        re.retTarget = static_cast<std::uint16_t>(((re.stackHigh & 0x0Fu) << 8) | re.stackLow);
        re.retBank = static_cast<std::uint8_t>((re.stackHigh & 0x10u) ? 1 : 0);
        for (int i = 0; i < 16; ++i) re.stack[i] = intRAM[8 + i];
        g_retPending = true;
        g_retPendingIdx = g_retCount % kRetCap;
        ++g_retCount;
        if (opv == 0x93u)
        {
            g_lastRetrSeq = g_retCount - 1;
            g_lastRetrMaster = static_cast<std::uint32_t>(master_clk);
            g_lastRetrFrame = static_cast<std::uint16_t>(frame & 0xFFFF);
            g_lastRetrPc = static_cast<std::uint16_t>(pcv);
            g_haveRetr = true;
            g_retrPendingForWindow = true;
        }
    }

    const bool curInSet = IsLoopPc(pcv);
    const bool prevInSet = (g_havePrevPc && IsLoopPc(g_prevPc));
    bool justEnteredWait = false;

    // New wait-sequence entry: current PC in {AAA,AAB,AAC}, previous not in it.
    // The entry may be at AAA, AAB or AAC.
    if (curInSet && !prevInSet)
    {
        WaitEntry& we = g_waitRing[g_waitCount % kWaiteCap];
        we.seq = g_waitCount;
        we.master = static_cast<std::uint32_t>(master_clk);
        we.entrySeq = g_instrCount;
        we.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
        we.prevPc = static_cast<std::uint16_t>(g_prevPc);
        we.entryPc = static_cast<std::uint16_t>(pcv);
        we.prevOp = static_cast<std::uint8_t>(g_prevOp);
        we.acc = acc;
        we.psw = psw;
        we.p1 = p1;
        we.a0 = VDCwrite[0xA0];
        we.irqEx = static_cast<std::uint8_t>(irq_ex);
        we.xirqEn = static_cast<std::uint8_t>(xirq_en);
        we.mstate = static_cast<std::uint8_t>(mstate);
        we.bank = static_cast<std::uint8_t>((psw & 0x10) ? 1 : 0);
        we.entryType = IsCallOp(g_prevOp) ? 0 : (IsJmpOp(g_prevOp) ? 1 : 2);
        we.haveVector = g_haveVector ? 1 : 0;
        we.lastVectorSeq = g_lastVectorSeq;
        we.lastVectorFrame = g_lastVectorFrame;
        we.lastVectorMaster = g_lastVectorMaster;
        we.instrsSinceVector = g_haveVector ? (g_instrCount - g_lastVectorSeq) : 0;
        for (int i = 0; i < 8; ++i) we.regs[i] = intRAM[reg_pnt + i];
        const std::uint32_t pcnt = (g_preCount < kPre) ? g_preCount : kPre;
        we.preCount = static_cast<std::uint8_t>(pcnt);
        for (std::uint32_t i = 0; i < pcnt; ++i)
            we.pre[i] = g_pre[(g_preCount - pcnt + i) % kPre];
        ++g_waitEntries;
        ++g_waitCount;
        if (we.regs[0] == 0xA4u)
        {
            // This is an $A4 polling wait: arm first-read capture for the next
            // A4 read (the MOVX in this entry iteration or the next AAA).
            ++g_rasterWaitEntries;
            g_rasterEntryPending = true;
            g_rasterPendingWaitSeq = we.seq;
            g_rasterPendingEntryMaster = we.master;
            g_rasterPendingTarget = we.regs[5];
            g_rasterLastDeltaValid = false;
            g_rasterNearStrideCounter = 0;
        }
        if (we.regs[0] == 0x24u)
        {
            g_candidate = we;
            g_haveCandidate = true;
            g_candidateInstrs = 0;
            g_candidateHasWindow = false;
            g_candidateWindowCount = 0;
        }
        else
        {
            g_haveCandidate = false;
            g_candidateHasWindow = false;
        }
        g_inWait = true;
        justEnteredWait = true;
    }

    // Candidate tracking: count only while execution stays inside the wait set.
    if (g_inWait && g_haveCandidate && curInSet)
    {
        ++g_candidateInstrs;
        if (g_candidateInstrs == kPersistentThreshold)
        {
            if (!g_haveFirstPersistent)
            {
                g_firstPersistentWait = g_candidate;
                g_haveFirstPersistent = true;
            }
            g_lastPersistentWait = g_candidate;
            g_havePersistent = true;
            g_irqReqAfterPersistent = 0;
            g_irqVecAfterPersistent = 0;
            ++g_persistentCrossings;
            const unsigned idx = (g_candidate.entryPc == 0xAAA) ? 0u
                : (g_candidate.entryPc == 0xAAB) ? 1u : 2u;
            if (idx < 3) ++g_persistentEntryPcCount[idx];
            // Preserve the post-vector window captured at this wait's entry.
            if (g_candidateHasWindow)
            {
                const std::uint32_t n = (g_candidateWindowCount < kIrqWindow)
                    ? g_candidateWindowCount : kIrqWindow;
                for (std::uint32_t i = 0; i < n; ++i) g_finalWindow[i] = g_candidateWindow[i];
                g_finalWindowCount = n;
                g_finalWindowVectorFrame = g_irqWindowVectorFrame;
                g_haveFinalWindow = true;
            }
        }
    }

    // Leaving the wait set. A normal JNZ fall-through out of AAC is the only
    // successful completion; anything else (e.g. an accepted IRQ) just ends the
    // candidate without being recorded as a successful wait.
    if (g_inWait && !curInSet)
    {
        if (prevInSet && g_prevPc == 0xAACu && g_haveCandidate && g_candidate.regs[0] == 0x24u)
        {
            WaitDone wd{};
            wd.entrySeq = g_candidate.entrySeq;
            wd.master = g_candidate.master;
            wd.frame = g_candidate.frame;
            wd.callerPc = g_candidate.prevPc;
            wd.entryPc = g_candidate.entryPc;
            wd.r0 = g_candidate.regs[0];
            wd.r5 = g_candidate.regs[5];
            wd.a0 = g_candidate.a0;
            wd.p1 = g_candidate.p1;
            wd.irqEx = g_candidate.irqEx;
            wd.xirqEn = g_candidate.xirqEn;
            wd.mstate = g_candidate.mstate;
            wd.exitAcc = acc;
            wd.durationInstrs = g_instrCount - g_candidate.entrySeq;
            g_lastSuccessful24 = wd;
            g_haveSuccessful24 = true;
        }
        g_rasterEntryPending = false;
        g_rasterLastDeltaValid = false;
        g_inWait = false;
        g_haveCandidate = false;
    }

    // Roll prehistory (kPre instructions).
    PreInstr& p = g_pre[g_preCount % kPre];
    p.seq = g_instrCount;
    p.master = static_cast<std::uint32_t>(master_clk);
    p.pc = static_cast<std::uint16_t>(pcv);
    p.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    p.op = static_cast<std::uint8_t>(opv);
    p.acc = acc;
    p.psw = psw;
    p.irqFlags = static_cast<std::uint8_t>(
        (irq_ex ? 0x01 : 0) | (xirq_en ? 0x02 : 0) | (tirq_en ? 0x04 : 0) | (mstate ? 0x08 : 0));
    for (int i = 0; i < 8; ++i) p.regs[i] = intRAM[reg_pnt + i];
    p.p1 = p1;
    p.a0 = VDCwrite[0xA0];
    ++g_preCount;

    // Append to the active post-vector window (bounded).
    if (g_irqWindowActive && g_irqWindowCount < kIrqWindow)
    {
        g_irqWindow[g_irqWindowCount] = p;
        ++g_irqWindowCount;
    }

    // Snapshot the post-vector window at a qualifying wait entry, after the
    // entry instruction itself has been appended.
    if (justEnteredWait && g_haveCandidate && !g_candidateHasWindow && g_irqWindowActive)
    {
        const std::uint32_t n = (g_irqWindowCount < kIrqWindow) ? g_irqWindowCount : kIrqWindow;
        for (std::uint32_t i = 0; i < n; ++i) g_candidateWindow[i] = g_irqWindow[i];
        g_candidateWindowCount = n;
        g_candidateHasWindow = true;
    }

    g_prevPc = static_cast<std::uint16_t>(pcv);
    g_prevOp = static_cast<std::uint8_t>(opv);
    g_havePrevPc = true;
}

void OnBefore(unsigned pcArg, unsigned opArg)
{
    if (!g_armed)
        return;
    TraceTransition(pcArg, opArg);
    RecInstr(pcArg, opArg);
}

void OnEvent(unsigned kind, unsigned address, unsigned value)
{
    if (!g_armed)
        return;
    switch (kind)
    {
    case 13: // IRQ transition
        if (address == 1u) { ++g_vblCount; RecEvent(1, address, value); NoteRasterVbl(); }
        else { ++g_irqReq; RecEvent(2, address, value); }
        if (g_havePersistent) ++g_irqReqAfterPersistent;
        break;
    case 14: // ext_IRQ()
        ++g_extIrq; RecEvent(3, address, value);
        if (g_havePersistent) ++g_irqReqAfterPersistent;
        break;
    case 15: // VDC A1 status read snapshot
        if (address == 0xA1u) { ++g_vdcA1; RecVdc(0xA1, value, A1Aux()); }
        break;
    case 2: // external read
        if (IsVdcAccess())
        {
            // VDC A1 is already captured via kind 15; do not double count it.
            if (address == 0xA2u) { ++g_vdcA2; RecVdc(0xA2, value, g_lastA2PreClear); }
            else if (address == 0xA4u || address == 0xA5u)
            {
                ++g_vdcA4A5;
                RecVdc(address, value, 0);
                if (address == 0xA4u)
                    NoteRasterA4(value);
            }
        }
        else if (IsVppRead())
        {
            RecVpp(false, address, value);
        }
        break;
    case 3: // external write (observed before routing)
        if (IsVppWrite())
            RecVpp(true, address, value);
        break;
    case 9: // VDC write
        ++g_vdcWrite;
        break;
    default:
        break;
    }
}

void Install()
{
    observation::before = OnBefore;
    observation::advance = OnAdvance;
    observation::event = OnEvent;
}

void Uninstall()
{
    observation::before = nullptr;
    observation::advance = nullptr;
    observation::event = nullptr;
}

// ---- log writing -----------------------------------------------------------
void WriteEventLine(FILE* f, const EventRec& e)
{
    const char* name = "?";
    switch (e.kind)
    {
    case 1: name = "VBL"; break;
    case 2: name = "IRQ_REQ"; break;
    case 3: name = "EXT_IRQ"; break;
    case 4: name = "IRQ_EX"; break;
    case 5: name = "FRAME_END"; break;
    default: name = "EVENT"; break;
    }
    std::fprintf(f, "  seq=%u master=%u mstate=%u kind=%s a=0x%02X b=%u\n",
        e.seq, e.master, e.mstate, name, e.a, e.b);
}

void WritePrehistory(FILE* f, const char* title, const PreInstr* pre, unsigned n)
{
    std::fprintf(f, "%s\n", title);
    std::fprintf(f, "    seq frame master pc  op  acc psw ifl p1 A0 r0r1r2r3r4r5r6r7\n");
    if (n > kPre) n = kPre;
    for (unsigned i = 0; i < n; ++i)
    {
        const PreInstr& p = pre[i];
        std::fprintf(f, "    %u %u %u %03X %02X %02X %02X %02X %02X %02X "
            "%02X%02X%02X%02X%02X%02X%02X%02X\n",
            p.seq, p.frame, p.master, p.pc, p.op, p.acc, p.psw, p.irqFlags, p.p1, p.a0,
            p.regs[0], p.regs[1], p.regs[2], p.regs[3],
            p.regs[4], p.regs[5], p.regs[6], p.regs[7]);
    }
}

void WriteWindow(FILE* f, const char* title, const std::vector<PreInstr>& w,
    std::uint32_t n, std::uint32_t from)
{
    std::fprintf(f, "%s (%u..%u of %u)\n", title, from, n, n);
    std::fprintf(f, "    seq frame master pc  op  acc psw ifl p1 A0 r0r1r2r3r4r5r6r7\n");
    if (n > kIrqWindow) n = kIrqWindow;
    for (std::uint32_t i = from; i < n; ++i)
    {
        const PreInstr& p = w[i];
        std::fprintf(f, "    %u %u %u %03X %02X %02X %02X %02X %02X %02X "
            "%02X%02X%02X%02X%02X%02X%02X%02X\n",
            p.seq, p.frame, p.master, p.pc, p.op, p.acc, p.psw, p.irqFlags, p.p1, p.a0,
            p.regs[0], p.regs[1], p.regs[2], p.regs[3],
            p.regs[4], p.regs[5], p.regs[6], p.regs[7]);
    }
}

void WriteLog()
{
    FILE* f = std::fopen("g7400_freeze_trace.log", "wb");
    if (!f)
    {
        std::fprintf(stderr, "O2EM-NG: could not create g7400_freeze_trace.log\n");
        return;
    }

    const unsigned long cpuDump = (std::min)(kCpuDumpMax, g_instrCount);

    std::fprintf(f, "O2EM-NG G7400 freeze trace\n");
    std::fprintf(f, "==========================\n");
    std::fprintf(f, "version        : %s\n", O2emVersion::kAppVersion);
    std::fprintf(f, "rom            : %s\n", g_romPath.c_str());
    std::fprintf(f, "rom_crc32      : %08lX\n", g_romCrc);
    std::fprintf(f, "bios           : %s\n", g_bios.c_str());
    std::fprintf(f, "region         : %s\n", g_region.c_str());
    std::fprintf(f, "vpp_enabled    : %d\n", g_vpp);
    std::fprintf(f, "instr_ring_cap : %u\n", kInstrCap);
    std::fprintf(f, "vdc_ring_cap   : %u\n", kVdcCap);
    std::fprintf(f, "vpp_ring_cap   : %u\n", kVppCap);
    std::fprintf(f, "event_ring_cap : %u\n", kEventCap);

    std::fprintf(f, "\nSUMMARY\n-------\n");
    std::fprintf(f, "  frames_executed        : %d\n", frame);
    std::fprintf(f, "  instructions_observed  : %u\n", g_instrCount);
    std::fprintf(f, "  cycles_observed        : %llu\n", static_cast<unsigned long long>(g_cycles));
    std::fprintf(f, "  debug_auto_arm_build   : %d\n", FreezeTraceAutoArmBuildEnabled() ? 1 : 0);
    std::fprintf(f, "  debug_auto_arm_vp59    : %d\n", FreezeTraceAutoArmForCrc(kHelicopterCrc) ? 1 : 0);
    std::fprintf(f, "  vector_entries(PC=003) : %u\n", g_vectorEntries);
    std::fprintf(f, "  vbl_count              : %u\n", g_vblCount);
    std::fprintf(f, "  irq_request_events     : %u\n", g_irqReq);
    std::fprintf(f, "  ext_irq_events         : %u\n", g_extIrq);
    std::fprintf(f, "  frame_end_events       : %u\n", g_frameEnd);
    std::fprintf(f, "  vdc_writes             : %u\n", g_vdcWrite);
    std::fprintf(f, "  vdc_A1_reads           : %u\n", g_vdcA1);
    std::fprintf(f, "  vdc_A2_reads           : %u\n", g_vdcA2);
    std::fprintf(f, "  vdc_A4_A5_reads        : %u\n", g_vdcA4A5);
    std::fprintf(f, "  vpp_reads              : %u\n", g_vppReads);
    std::fprintf(f, "  vpp_writes             : %u\n", g_vppWrites);
    std::fprintf(f, "  a0_writes              : %u\n", g_a0Count);
    std::fprintf(f, "  a0_bit1_rising         : %u\n", g_a0Rising);
    std::fprintf(f, "  a0_bit1_falling        : %u\n", g_a0Falling);
    std::fprintf(f, "  a0_fall_raw_line_gt241 : %u\n", g_a0FallGt241);
    std::fprintf(f, "  a0_fall_y_equals_FF    : %u\n", g_a0FallYff);
    if (g_haveLastA0Fall)
    {
        const A0Write& e = g_lastA0Fall;
        std::fprintf(f, "  LAST_A0_BIT1_FALL seq=%u frame=%u pc=%03X master=%u h_clk=%u mstate=%u "
            "old_A0=%02X new_A0=%02X raw_line=%u y_before=%02X y_after=%02X clamped=%u\n",
            e.seq, e.frame, e.pc, e.master, e.hclk, e.mstate,
            e.prev, e.next, e.rawLine, e.yBefore, e.yAfter, (e.flags >> 1) & 1);
    }
    else
    {
        std::fprintf(f, "  LAST_A0_BIT1_FALL      : none observed\n");
    }

    std::fprintf(f, "  vdc24_writes           : %u\n", g_vdc24Writes);
    std::fprintf(f, "  vdc24_reads            : %u\n", g_vdc24Reads);
    std::fprintf(f, "  vdc24_0x3E_requested   : %u\n", g_vdc24Req3E);
    std::fprintf(f, "  vdc24_0x3E_stored      : %u\n", g_vdc24Stored3E);
    if (g_haveLastVdc24Write)
    {
        const Vdc24Event& e = g_lastVdc24Write;
        std::fprintf(f, "  LAST_VDC24_WRITE seq=%u frame=%u pc=%03X requested=%02X before=%02X after=%02X "
            "blocked=%u p1=%02X A0=%02X\n", e.seq, e.frame, e.pc, e.requested, e.before, e.after,
            e.flags & 1, e.p1, e.a0);
    }
    else
    {
        std::fprintf(f, "  LAST_VDC24_WRITE       : none observed\n");
    }
    if (g_haveLastVdc24Read)
    {
        const Vdc24Event& e = g_lastVdc24Read;
        const char* br = (e.flags & 0x02) ? "OBJECT_APPROX" : ((e.flags & 0x04) ? "MIRROR" : "OTHER");
        std::fprintf(f, "  LAST_VDC24_READ seq=%u frame=%u pc=%03X returned=%02X mirror=%02X branch=%s "
            "p1=%02X A0=%02X\n", e.seq, e.frame, e.pc, e.returned, e.before, br, e.p1, e.a0);
    }
    else
    {
        std::fprintf(f, "  LAST_VDC24_READ        : none observed\n");
    }

    // Polling analysis over the retained instruction ring.
    const std::uint32_t retained = (std::min)(g_instrCount, kInstrCap);
    const std::uint32_t window = (std::min)(retained, kPollWindow);
    const std::uint32_t start = g_instrCount - window;
    std::vector<std::uint16_t> pcs;
    pcs.reserve(window);
    for (std::uint32_t s = start; s < g_instrCount; ++s)
        pcs.push_back(g_instr[s % kInstrCap].pc);

    const std::uint32_t loopW = (std::min)(window, kLoopWindow);
    bool loopFound = false;
    unsigned period = 0;
    for (unsigned p = 1; p <= 64 && !loopFound; ++p)
    {
        if (loopW <= p) break;
        bool ok = true;
        for (std::uint32_t i = loopW - p; i < loopW; ++i)
            if (pcs[i] != pcs[i - p]) { ok = false; break; }
        if (ok) { loopFound = true; period = p; }
    }

    // Frequency over the analysis window.
    std::vector<std::uint16_t> uniq = pcs;
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    std::uint16_t topPc = 0; unsigned topPcCount = 0;
    for (std::uint16_t pcv : uniq)
    {
        const unsigned c = static_cast<unsigned>(std::count(pcs.begin(), pcs.end(), pcv));
        if (c > topPcCount) { topPcCount = c; topPc = pcv; }
    }

    std::fprintf(f, "  analysis_window        : %u instructions\n", window);
    std::fprintf(f, "  distinct_pcs_in_window : %zu\n", uniq.size());
    std::fprintf(f, "  most_frequent_pc       : 0x%03X (%u of %u)\n", topPc, topPcCount, window);
    if (loopFound)
    {
        std::fprintf(f, "  loop_detected          : YES period=%u pcs=", period);
        const unsigned shown = (std::min)(period, 64u);
        for (unsigned i = 0; i < shown; ++i)
            std::fprintf(f, "%s%03X", i ? "," : "", pcs[loopW - period + i]);
        std::fprintf(f, "\n");
    }
    else
    {
        std::fprintf(f, "  loop_detected          : no simple period <=64 in last %u instructions\n", loopW);
    }

    // Trailing persistent polling loop, strictly within the BOUNDED analysis
    // window [start, g_instrCount). The scan must never walk before the
    // physically retained/analysed records.
    std::uint32_t loopStart = g_instrCount;
    while (loopStart > start)
    {
        const Instr& e = g_instr[(loopStart - 1) % kInstrCap];
        if (e.pc == 0xAAA || e.pc == 0xAAB || e.pc == 0xAAC)
            --loopStart;
        else
            break;
    }
    const bool haveLoop = (loopStart < g_instrCount);
    if (haveLoop)
    {
        const Instr& e = g_instr[loopStart % kInstrCap];
        std::fprintf(f, "  persistent_loop_start  : seq=%u frame=%u pc=%03X master=%u\n",
            e.seq, e.frame, e.pc, e.master);

        // Register summary over the bounded window only. One linear pass with
        // fixed 256-entry frequency arrays (no allocations, no nested search).
        // R0 at the AAA row = MOVX address; R5 at the AAA row = XRL target.
        std::uint32_t r0Counts[256] = {};
        std::uint32_t r5Counts[256] = {};
        std::uint32_t rows = 0;
        bool first = true;
        std::uint8_t r0First = 0, r5First = 0;
        bool r0Const = true, r5Const = true;
        for (std::uint32_t k = loopStart; k < g_instrCount; ++k)
        {
            const Instr& r = g_instr[k % kInstrCap];
            if (r.pc != 0xAAA)
                continue;
            ++rows;
            ++r0Counts[r.regs[0]];
            ++r5Counts[r.regs[5]];
            if (first) { r0First = r.regs[0]; r5First = r.regs[5]; first = false; }
            else
            {
                if (r.regs[0] != r0First) r0Const = false;
                if (r.regs[5] != r5First) r5Const = false;
            }
        }
        unsigned distinctR0 = 0, distinctR5 = 0, m0 = 0, m5 = 0;
        std::uint8_t v0 = 0, v5 = 0;
        for (int i = 0; i < 256; ++i)
        {
            if (r0Counts[i]) { ++distinctR0; if (r0Counts[i] > m0) { m0 = r0Counts[i]; v0 = static_cast<std::uint8_t>(i); } }
            if (r5Counts[i]) { ++distinctR5; if (r5Counts[i] > m5) { m5 = r5Counts[i]; v5 = static_cast<std::uint8_t>(i); } }
        }
        std::fprintf(f, "  loop_AAA_rows          : %u\n", rows);
        if (rows)
        {
            std::fprintf(f, "  R0_constant_during_loop: %s\n", r0Const ? "YES" : "NO");
            std::fprintf(f, "  R5_constant_during_loop: %s\n", r5Const ? "YES" : "NO");
            if (r0Const) std::fprintf(f, "  MOVX_address           : 0x%02X%s\n",
                r0First, r0First == 0xA4 ? "  (VDC A4)" : "");
            else std::fprintf(f, "  most_common_R0         : 0x%02X (%u of %u)\n", v0, m0, rows);
            if (r5Const) std::fprintf(f, "  comparison_target_R5   : 0x%02X\n", r5First);
            else std::fprintf(f, "  most_common_R5         : 0x%02X (%u of %u)\n", v5, m5, rows);
            std::fprintf(f, "  distinct_R0            :");
            { unsigned n = 0; for (int i = 0; i < 256 && n < 12; ++i) if (r0Counts[i]) { std::fprintf(f, " %02X", i); ++n; }
              if (distinctR0 > 12) std::fprintf(f, " ...(%u)", distinctR0); }
            std::fprintf(f, "\n  distinct_R5            :");
            { unsigned n = 0; for (int i = 0; i < 256 && n < 12; ++i) if (r5Counts[i]) { std::fprintf(f, " %02X", i); ++n; }
              if (distinctR5 > 12) std::fprintf(f, " ...(%u)", distinctR5); }
            std::fprintf(f, "\n");
        }
    }
    if (g_haveLastA0Fall)
    {
        std::fprintf(f, "  last_A0_fall           : seq=%u frame=%u master=%u\n",
            g_lastA0Fall.seq, g_lastA0Fall.frame, g_lastA0Fall.master);
        if (haveLoop)
        {
            const Instr& e = g_instr[loopStart % kInstrCap];
            std::fprintf(f, "  loop_start_minus_fall  : frames=%d instrs=%d\n",
                static_cast<int>(e.frame) - static_cast<int>(g_lastA0Fall.frame),
                static_cast<int>(e.seq) - static_cast<int>(g_lastA0Fall.seq));
        }
    }

    // ---- transition trace (first-entry / IRQ-context) -----------------------
    static const char* kEntryName[3] = { "CALL", "JMP", "OTHER" };

    std::fprintf(f, "\nWAIT TRANSITION SUMMARY\n-----------------------\n");
    std::fprintf(f, "  wait_entries_total     : %u\n", g_waitEntries);
    std::fprintf(f, "  wait_entries_retained  : %u\n", (std::min)(g_waitCount, kWaiteCap));
    std::fprintf(f, "  wait_entry_ring_cap    : %u\n", kWaiteCap);
    std::fprintf(f, "  ret_event_ring_cap     : %u\n", kRetCap);
    std::fprintf(f, "  prehistory_per_entry   : %u instructions\n", kPre);
    std::fprintf(f, "  persistent_threshold   : %u instructions\n", kPersistentThreshold);
    std::fprintf(f, "  pal_vertical_timing    : %s\n", VM_PalVerticalTimingActive() ? "ON" : "OFF");
    std::fprintf(f, "  pal_cycles_per_line    : 25.333 (456/18)\n");
    std::fprintf(f, "  pal_frame_cycles       : 7929.333 (313*456/18; integer 7929/7930)\n");
    std::fprintf(f, "  pal_vbl_cycle          : %d\n", VM_PalVblCycle());
    std::fprintf(f, "  pal_vertical_lines     : 313\n");
    std::fprintf(f, "  raster_wait_entries    : %u\n", g_rasterWaitEntries);
    std::fprintf(f, "  raster_wait_target_misses : %u\n", g_rasterTargetMisses);
    std::fprintf(f, "  raster_wait_exact_hits : %u\n", g_rasterExactHits);
    std::fprintf(f, "  raster_wait_behind_target : %u\n", g_rasterBehindTarget);
    if (g_haveRasterMiss)
    {
        const RasterSample& s = g_lastRasterMiss;
        std::fprintf(f, "  RASTER_WAIT_MISS frame=%u wait_entry_master=%u target_R5=%02X first_A4=%02X delta=%+d missed_target=%s wait_seq=%u\n",
            s.frame, s.waitEntryMaster, s.r5, s.a4, static_cast<int>(s.delta),
            (s.a4 > s.r5) ? "yes" : "no", s.waitSeq);
    }
    else
    {
        std::fprintf(f, "  RASTER_WAIT_MISS       : none observed\n");
    }
    if (g_haveRasterAtVbl)
    {
        const RasterSample& s = g_rasterAtVbl;
        std::fprintf(f, "  RASTER_AT_VBL frame=%u master=%u pc=%03X p1=%02X A4=%02X R5=%02X delta=%+d in_wait=%u\n",
            s.frame, s.master, s.pc, s.p1, s.a4, s.r5, static_cast<int>(s.delta),
            (s.flags & 0x01u) ? 1u : 0u);
    }
    if (g_haveRasterAtVector)
    {
        const RasterSample& s = g_rasterAtVector;
        std::fprintf(f, "  RASTER_AT_VECTOR frame=%u master=%u pc=%03X p1=%02X A4=%02X R5=%02X delta=%+d in_wait=%u\n",
            s.frame, s.master, s.pc, s.p1, s.a4, s.r5, static_cast<int>(s.delta),
            (s.flags & 0x01u) ? 1u : 0u);
    }
    if (g_haveVector)
        std::fprintf(f, "  last_irq_vector        : seq=%u frame=%u master=%u\n",
            g_lastVectorSeq, g_lastVectorFrame, g_lastVectorMaster);
    else
        std::fprintf(f, "  last_irq_vector        : none observed\n");
    if (g_haveRetr)
        std::fprintf(f, "  last_RETR              : seq=%u frame=%u pc=%03X master=%u\n",
            g_lastRetrSeq, g_lastRetrFrame, g_lastRetrPc, g_lastRetrMaster);
    else
        std::fprintf(f, "  last_RETR              : none observed\n");

    const std::uint32_t retN = (std::min)(g_retCount, kRetCap);
    bool haveRetIrq1 = false;
    RetEvent lastRetIrq1{};
    for (std::uint32_t i = 0; i < retN; ++i)
    {
        const RetEvent& re = g_retRing[(g_retCount - retN + i) % kRetCap];
        if (re.op == 0x83u && re.irqExBefore) { lastRetIrq1 = re; haveRetIrq1 = true; }
    }
    if (haveRetIrq1)
        std::fprintf(f, "  last_RET_with_irq_ex_1 : seq=%u frame=%u pc=%03X master=%u irq_ex_after=%s\n",
            lastRetIrq1.seq, lastRetIrq1.frame, lastRetIrq1.pc, lastRetIrq1.master,
            lastRetIrq1.haveAfter ? (lastRetIrq1.irqExAfter ? "1" : "0") : "?");
    else
        std::fprintf(f, "  last_RET_with_irq_ex_1 : none observed\n");

    std::fprintf(f, "  persistent_by_entry_pc : AAA=%u AAB=%u AAC=%u\n",
        g_persistentEntryPcCount[0], g_persistentEntryPcCount[1], g_persistentEntryPcCount[2]);

    if (g_havePersistent)
    {
        const WaitEntry& w = g_lastPersistentWait;
        std::fprintf(f, "\nPERSISTENT_WAIT_ENTRY (final, entry at %03X; %u crossing(s))\n",
            w.entryPc, g_persistentCrossings);
        std::fprintf(f, "  seq=%u frame=%u master=%u entrySeq=%u prevPC=%03X prevOp=%02X entryType=%s\n",
            w.seq, w.frame, w.master, w.entrySeq, w.prevPc, w.prevOp,
            kEntryName[w.entryType < 3 ? w.entryType : 2]);
        std::fprintf(f, "  R0=%02X R5=%02X ACC=%02X PSW=%02X bank=%u mstate=%u p1=%02X A0=%02X\n",
            w.regs[0], w.regs[5], w.acc, w.psw, w.bank, w.mstate, w.p1, w.a0);
        std::fprintf(f, "  irq_ex=%u xirq_en=%u instrs_since_vector=%u\n",
            w.irqEx, w.xirqEn, w.instrsSinceVector);
        if (w.haveVector)
            std::fprintf(f, "  last_vector_before     : seq=%u frame=%u master=%u\n",
                w.lastVectorSeq, w.lastVectorFrame, w.lastVectorMaster);
        else
            std::fprintf(f, "  last_vector_before     : none observed\n");
        std::fprintf(f, "  frames_from_entry_to_dump : %d\n",
            static_cast<int>(frame) - static_cast<int>(w.frame));
        std::fprintf(f, "  irq_requests_after_entry  : %u\n", g_irqReqAfterPersistent);
        std::fprintf(f, "  irq_vectors_after_entry   : %u\n", g_irqVecAfterPersistent);
        if (g_irqReqAfterPersistent > 0 && g_irqVecAfterPersistent == 0)
            std::fprintf(f, "  NOTE: IRQ requests > 0 but IRQ vectors = 0 after persistent entry\n");

        WritePrehistory(f, "  PREHISTORY (last instructions before final entry, oldest first)",
            w.pre, w.preCount);

        if (g_haveFirstPersistent &&
            (g_firstPersistentWait.entrySeq != w.entrySeq ||
             g_firstPersistentWait.entryPc != w.entryPc))
        {
            const WaitEntry& wf = g_firstPersistentWait;
            std::fprintf(f, "\nFIRST_PERSISTENT_WAIT_ENTRY (first ever, entry at %03X)\n", wf.entryPc);
            std::fprintf(f, "  seq=%u frame=%u master=%u prevPC=%03X prevOp=%02X R0=%02X R5=%02X "
                "irq_ex=%u A0=%02X p1=%02X\n",
                wf.seq, wf.frame, wf.master, wf.prevPc, wf.prevOp, wf.regs[0], wf.regs[5],
                wf.irqEx, wf.a0, wf.p1);
            WritePrehistory(f, "  PREHISTORY (first persistent entry)",
                wf.pre, wf.preCount);
        }
    }
    else
    {
        std::fprintf(f, "\nPERSISTENT_WAIT_ENTRY : none observed "
            "(no R0=0x24 wait exceeded %u instructions)\n", kPersistentThreshold);
    }

    std::fprintf(f, "\nFINAL IRQ WINDOW (post-vector PC=003 stream up to the final wait entry)\n");
    if (g_haveFinalWindow)
    {
        std::fprintf(f, "  vector_frame=%u rows=%u\n",
            g_finalWindowVectorFrame, g_finalWindowCount);
        WriteWindow(f, "  FINAL_IRQ_PATH", g_finalWindow, g_finalWindowCount, 0);
    }
    else
    {
        std::fprintf(f, "  none preserved\n");
    }

    std::fprintf(f, "\nSUCCESS IRQ WINDOW (last IRQ path that completed with RETR irq_ex->0)\n");
    if (g_haveSuccessWindow)
    {
        std::fprintf(f, "  vector_frame=%u rows=%u\n",
            g_successWindowVectorFrame, g_successWindowCount);
        WriteWindow(f, "  SUCCESS_IRQ_PATH", g_successWindow, g_successWindowCount, 0);
    }
    else
    {
        std::fprintf(f, "  none preserved\n");
    }

    std::fprintf(f, "\nFINAL vs SUCCESS IRQ PATH DIVERGENCE (tail)\n");
    if (g_haveFinalWindow && g_haveSuccessWindow)
    {
        const std::uint32_t n = (std::min)(g_finalWindowCount, g_successWindowCount);
        std::uint32_t d = 0;
        while (d < n &&
            g_finalWindow[d].pc == g_successWindow[d].pc &&
            g_finalWindow[d].op == g_successWindow[d].op)
            ++d;
        std::fprintf(f, "  first_divergence_index : %u\n", d);
        if (d < n)
            std::fprintf(f, "  divergence: success PC=%03X op=%02X  final PC=%03X op=%02X\n",
                g_successWindow[d].pc, g_successWindow[d].op,
                g_finalWindow[d].pc, g_finalWindow[d].op);
        else
            std::fprintf(f, "  (common prefix; one path shorter)\n");
        const std::uint32_t tailFrom = (d > 16) ? (d - 16) : 0;
        WriteWindow(f, "  SUCCESS tail", g_successWindow, g_successWindowCount, tailFrom);
        WriteWindow(f, "  FINAL tail", g_finalWindow, g_finalWindowCount, tailFrom);
    }
    else
    {
        std::fprintf(f, "  (need both a final and a successful post-vector window)\n");
    }

    std::fprintf(f, "\nLAST SUCCESSFUL R0=0x24 WAIT\n");
    if (g_haveSuccessful24)
    {
        const WaitDone& d = g_lastSuccessful24;
        std::fprintf(f, "  caller=%03X R0=%02X R5=%02X A0=%02X P1=%02X irq_ex=%u xirq_en=%u mstate=%u\n",
            d.callerPc, d.r0, d.r5, d.a0, d.p1, d.irqEx, d.xirqEn, d.mstate);
        std::fprintf(f, "  frame=%u master=%u exit_acc=%02X duration_instrs=%u\n",
            d.frame, d.master, d.exitAcc, d.durationInstrs);
    }
    else
    {
        std::fprintf(f, "  none observed\n");
    }

    const std::uint32_t waitN = (std::min)(g_waitCount, kWaiteCap);
    std::fprintf(f, "\nWAIT ENTRIES (last %u, oldest first; entryType CALL/JMP/OTHER)\n", waitN);
    std::fprintf(f, "  seq frame master entrySeq ePC prevPC prevOp typ R0 R5 acc psw p1 A0 irqex xirq mst since_vec\n");
    for (std::uint32_t i = 0; i < waitN; ++i)
    {
        const WaitEntry& w = g_waitRing[(g_waitCount - waitN + i) % kWaiteCap];
        std::fprintf(f, "  %u %u %u %u %03X %03X %02X %s %02X %02X %02X %02X %02X %02X %u %u %u %u\n",
            w.seq, w.frame, w.master, w.entrySeq, w.entryPc, w.prevPc, w.prevOp,
            kEntryName[w.entryType < 3 ? w.entryType : 2],
            w.regs[0], w.regs[5], w.acc, w.psw, w.p1, w.a0,
            w.irqEx, w.xirqEn, w.mstate, w.instrsSinceVector);
    }

    std::fprintf(f, "\nRET / RETR EVENTS (last %u, oldest first)\n", retN);
    std::fprintf(f, "  seq frame master pc  op psw sp sp_hi sp_lo retTgt bank nextPC irqex_before irqex_after\n");
    for (std::uint32_t i = 0; i < retN; ++i)
    {
        const RetEvent& re = g_retRing[(g_retCount - retN + i) % kRetCap];
        char nextBuf[8];
        if (re.haveAfter && re.nextPc != 0xFFFF)
            std::snprintf(nextBuf, sizeof(nextBuf), "%03X", re.nextPc);
        else
            std::snprintf(nextBuf, sizeof(nextBuf), "----");
        std::fprintf(f, "  %u %u %u %03X %02X %02X %u %02X %02X %03X %u %s %u %s\n",
            re.seq, re.frame, re.master, re.pc, re.op, re.psw, re.sp,
            re.stackHigh, re.stackLow, re.retTarget, re.retBank, nextBuf,
            re.irqExBefore,
            re.haveAfter ? (re.irqExAfter ? "1" : "0") : "?");
    }

    std::fprintf(f, "\nSUCCESSFUL vs FROZEN R0=0x24 WAIT (raw)\n");
    if (g_haveSuccessful24 && g_havePersistent)
    {
        const WaitDone& d = g_lastSuccessful24;
        const WaitEntry& w = g_lastPersistentWait;
        std::fprintf(f, "  field        successful            frozen\n");
        std::fprintf(f, "  entryPC      %03X                   %03X\n", d.entryPc, w.entryPc);
        std::fprintf(f, "  caller       %03X                   %03X\n", d.callerPc, w.prevPc);
        std::fprintf(f, "  R5           %02X                    %02X\n", d.r5, w.regs[5]);
        std::fprintf(f, "  A0           %02X                    %02X\n", d.a0, w.a0);
        std::fprintf(f, "  P1           %02X                    %02X\n", d.p1, w.p1);
        std::fprintf(f, "  irq_ex       %u                     %u\n", d.irqEx, w.irqEx);
        std::fprintf(f, "  xirq_en      %u                     %u\n", d.xirqEn, w.xirqEn);
        std::fprintf(f, "  mstate       %u                     %u\n", d.mstate, w.mstate);
        std::fprintf(f, "  frame        %u                    %u\n", d.frame, w.frame);
        std::fprintf(f, "  duration     %u                    still running\n", d.durationInstrs);
    }
    else
    {
        std::fprintf(f, "  (need both a successful and a persistent R0=0x24 wait)\n");
    }

    std::fprintf(f, "\nRECENT CPU HISTORY (last %lu instructions, oldest first)\n", cpuDump);
    std::fprintf(f, "  seq frame master mstate pc  op  acc psw flg r0r1r2r3r4r5r6r7\n");
    const std::uint32_t first = g_instrCount - cpuDump;
    for (std::uint32_t s = first; s < g_instrCount; ++s)
    {
        const Instr& e = g_instr[s % kInstrCap];
        std::fprintf(f, "  %u %u %u %u %03X %02X %02X %02X %02X "
            "%02X%02X%02X%02X%02X%02X%02X%02X\n",
            e.seq, e.frame, e.master, e.flags & 1, e.pc, e.op, e.acc, e.psw, e.flags,
            e.regs[0], e.regs[1], e.regs[2], e.regs[3],
            e.regs[4], e.regs[5], e.regs[6], e.regs[7]);
    }

    // Last complete AAA/AAB/AAC iterations of the persistent loop, from a
    // single bounded pass over [start, g_instrCount) keeping only the last 24.
    // Values come straight from the recorded before-instruction rows:
    //   AAA row acc = before MOVX, AAB row acc = after MOVX,
    //   AAC row acc = after XRL, next row PC = JNZ decision.
    {
        struct Iter
        {
            std::uint32_t frame;
            std::uint32_t master;
            std::uint8_t r0, r5, accA, accB, accC;
            bool taken;
            std::uint16_t nextPc;
        };
        constexpr std::size_t kMaxIter = 24;
        Iter ring[kMaxIter];
        std::size_t ringCount = 0, ringHead = 0;
        for (std::uint32_t k = start; k + 3 < g_instrCount; ++k)
        {
            if (g_instr[k % kInstrCap].pc == 0xAAA &&
                g_instr[(k + 1) % kInstrCap].pc == 0xAAB &&
                g_instr[(k + 2) % kInstrCap].pc == 0xAAC)
            {
                const Instr& a = g_instr[k % kInstrCap];
                const Instr& b = g_instr[(k + 1) % kInstrCap];
                const Instr& c = g_instr[(k + 2) % kInstrCap];
                const Instr& nx = g_instr[(k + 3) % kInstrCap];
                Iter& it = ring[ringHead];
                it.frame = a.frame; it.master = a.master;
                it.r0 = a.regs[0]; it.r5 = a.regs[5];
                it.accA = a.acc; it.accB = b.acc; it.accC = c.acc;
                it.taken = (nx.pc == 0xAAA); it.nextPc = static_cast<std::uint16_t>(nx.pc);
                ringHead = (ringHead + 1) % kMaxIter;
                if (ringCount < kMaxIter) ++ringCount;
            }
        }
        std::fprintf(f, "\nLOOP ITERATIONS (last %zu complete AAA/AAB/AAC, oldest first)\n", ringCount);
        std::fprintf(f, "  frame masterAAA R0 R5 acc_beforeMOVX acc_afterMOVX acc_afterXRL JNZ_taken nextPC\n");
        const std::size_t beginIdx = (ringCount < kMaxIter) ? 0 : ringHead;
        for (std::size_t i = 0; i < ringCount; ++i)
        {
            const Iter& it = ring[(beginIdx + i) % kMaxIter];
            std::fprintf(f, "  %u %u %02X %02X %02X %02X %02X %s %03X\n",
                it.frame, it.master, it.r0, it.r5, it.accA, it.accB, it.accC,
                it.taken ? "yes" : "no", it.nextPc);
        }
    }

    std::fprintf(f, "\nVDC READ HISTORY (%u entries, oldest first; aux A1=flags/A0, A2=coltab[1]|[2]<<8)\n", g_vdcCount);
    std::fprintf(f, "  seq frame master mstate pc  adr val aux\n");
    const std::uint32_t vdcN = (std::min)(g_vdcCount, kVdcCap);
    for (std::uint32_t i = 0; i < vdcN; ++i)
    {
        const VdcRead& e = g_vdc[(g_vdcCount - vdcN + i) % kVdcCap];
        std::fprintf(f, "  %u %u %u %u %03X %02X %02X %04X\n",
            e.seq, e.frame, e.master, e.mstate, e.pc, e.adr, e.value, e.aux);
    }

    std::fprintf(f, "\nRASTER WAIT SAMPLES (%u retained, oldest first; delta=A4-R5; first=first read after wait entry; ff=A4 FF clamp)\n",
        (std::min)(g_rasterCount, kRasterCap));
    std::fprintf(f, "  frame master pc  p1 A0 A4 R5 delta in_wait first nearvbl ff waitseq\n");
    const std::uint32_t rasterN = (std::min)(g_rasterCount, kRasterCap);
    for (std::uint32_t i = 0; i < rasterN; ++i)
    {
        const RasterSample& s = g_rasterRing[(g_rasterCount - rasterN + i) % kRasterCap];
        std::fprintf(f, "  %u %u %03X %02X %02X %02X %02X %+d %u %u %u %u %u\n",
            s.frame, s.master, s.pc, s.p1, s.a0, s.a4, s.r5,
            static_cast<int>(s.delta),
            (s.flags & 0x01u) ? 1u : 0u,
            (s.flags & 0x02u) ? 1u : 0u,
            (s.flags & 0x04u) ? 1u : 0u,
            (s.flags & 0x08u) ? 1u : 0u,
            s.waitSeq);
    }

    std::fprintf(f, "\nVPP ACCESS HISTORY (%u entries, oldest first)\n", g_vppCount);
    std::fprintf(f, "  seq master pc  wr adr val p1\n");
    const std::uint32_t vppN = (std::min)(g_vppCount, kVppCap);
    for (std::uint32_t i = 0; i < vppN; ++i)
    {
        const VppAccess& e = g_vppAcc[(g_vppCount - vppN + i) % kVppCap];
        std::fprintf(f, "  %u %u %03X %u %02X %02X %02X\n",
            e.seq, e.master, e.pc, e.write, e.adr, e.value, e.p1);
    }

    std::fprintf(f, "\nA0 WRITE HISTORY (%u entries, oldest first; flg: F=fall(1->0) R=rise(0->1) C=raw_line>241)\n", g_a0Count);
    std::fprintf(f, "  seq frame pc  master hclk mst prev next yb ya line flg\n");
    const std::uint32_t a0N = (std::min)(g_a0Count, kA0Cap);
    for (std::uint32_t i = 0; i < a0N; ++i)
    {
        const A0Write& e = g_a0[(g_a0Count - a0N + i) % kA0Cap];
        char fl[4] = { (e.flags & 1) ? 'F' : '-', (e.flags & 4) ? 'R' : '-', (e.flags & 2) ? 'C' : '-', 0 };
        std::fprintf(f, "  %u %u %03X %u %u %u %02X %02X %02X %02X %u %s\n",
            e.seq, e.frame, e.pc, e.master, e.hclk, e.mstate,
            e.prev, e.next, e.yBefore, e.yAfter, e.rawLine, fl);
    }
    std::fprintf(f, "\nA0 BIT1 FALLING EDGES (1->0)\n");
    for (std::uint32_t i = 0; i < a0N; ++i)
    {
        const A0Write& e = g_a0[(g_a0Count - a0N + i) % kA0Cap];
        if (!(e.flags & 1))
            continue;
        std::fprintf(f, "  A0_BIT1_FALL seq=%u frame=%u pc=%03X master=%u h_clk=%u mstate=%u "
            "old_A0=%02X new_A0=%02X raw_line=%u y_before=%02X y_after=%02X clamped=%u\n",
            e.seq, e.frame, e.pc, e.master, e.hclk, e.mstate,
            e.prev, e.next, e.rawLine, e.yBefore, e.yAfter, (e.flags >> 1) & 1);
    }

    std::fprintf(f, "\nVDC 0x24 EVENTS (%u entries, oldest first; dir R/W; br OBJ=object-approx MIR=mirror OTH=other WBLK=write-blocked WOK=write-ok)\n", g_vdc24Count);
    std::fprintf(f, "  seq frame pc  master dir req ret p1 a0 before after flg\n");
    const std::uint32_t v24N = (std::min)(g_vdc24Count, kVdc24Cap);
    for (std::uint32_t i = 0; i < v24N; ++i)
    {
        const Vdc24Event& e = g_vdc24[(g_vdc24Count - v24N + i) % kVdc24Cap];
        const char* br = e.dir
            ? ((e.flags & 1) ? "WBLK" : "WOK")
            : ((e.flags & 0x02) ? "OBJ" : ((e.flags & 0x04) ? "MIR" : "OTH"));
        std::fprintf(f, "  %u %u %03X %u %c %02X %02X %02X %02X %02X %02X %s\n",
            e.seq, e.frame, e.pc, e.master, e.dir ? 'W' : 'R',
            e.requested, e.returned, e.p1, e.a0, e.before, e.after, br);
    }

    std::fprintf(f, "\nEVENT HISTORY (%u entries, oldest first)\n", g_eventCount);
    const std::uint32_t evN = (std::min)(g_eventCount, kEventCap);
    for (std::uint32_t i = 0; i < evN; ++i)
        WriteEventLine(f, g_events[(g_eventCount - evN + i) % kEventCap]);

    std::fflush(f);
    std::fclose(f);
}
} // namespace

bool G7400FreezeTrace_EnvEnabled() { return g_envEnabled; }

void G7400FreezeTrace_Configure(const char* romPath, unsigned long romCrc,
    const char* biosFile, const char* regionText, int vpp)
{
    const char* env = std::getenv("O2EM_G7400_FREEZE_TRACE");
    const bool envOn = (env && *env);
    const bool autoOn = FreezeTraceAutoArmForCrc(romCrc);
    g_envEnabled = envOn || autoOn;
    if (!g_envEnabled)
        return;

    if (romCrc != kHelicopterCrc)
    {
        g_envEnabled = false; // trace stays disabled for this cartridge
        std::printf("O2EM-NG: G7400 freeze trace requested but cartridge CRC %08lX "
            "is not Helicopter Rescue+ (%08lX); trace stays disabled.\n",
            romCrc, kHelicopterCrc);
        std::fflush(stdout);
        return;
    }

    g_romPath = romPath ? romPath : "";
    g_romCrc = romCrc;
    g_bios = biosFile ? biosFile : "";
    g_region = regionText ? regionText : "";
    g_vpp = vpp;
    g_written = false;
    ResetAll();
    Install();
    g_armed = true;

    std::printf("O2EM-NG: G7400 freeze trace ARMED for Helicopter Rescue+ "
        "(CRC C4134DF8) via %s. Press F9 after the freeze to dump; a log is also "
        "written on normal exit. Output: g7400_freeze_trace.log\n",
        (autoOn && !envOn) ? "Debug auto-enable (VP59 CRC, no environment variable)"
                           : "O2EM_G7400_FREEZE_TRACE");
    std::fflush(stdout);
}

void G7400FreezeTrace_RequestDump()
{
    if (!g_armed || g_written || g_dumping)
        return;
    g_dumping = true;
    WriteLog();
    g_dumping = false;
    g_written = true;
    std::printf("O2EM-NG: G7400 freeze trace written to g7400_freeze_trace.log\n");
    std::fflush(stdout);
}

void G7400FreezeTrace_Shutdown()
{
    if (g_armed && !g_written && !g_dumping)
    {
        g_dumping = true;
        WriteLog();
        g_dumping = false;
        g_written = true;
        std::printf("O2EM-NG: G7400 freeze trace written on exit to g7400_freeze_trace.log\n");
        std::fflush(stdout);
    }
    g_armed = false;
    Uninstall();
}

void G7400FreezeTrace_NoteA2PreClear()
{
    if (!g_armed)
        return;
    // Snapshot the sprite-collision accumulators BEFORE clear_collision().
    g_lastA2PreClear = static_cast<std::uint16_t>(
        static_cast<unsigned>(coltab[1]) | (static_cast<unsigned>(coltab[2]) << 8));
}

void G7400FreezeTrace_NoteFrameEnd()
{
    if (!g_armed)
        return;
    ++g_frameEnd;
    RecEvent(5, 0, 0);
}

void G7400FreezeTrace_NoteA0Write(unsigned oldA0, unsigned newA0,
    unsigned yBefore, unsigned yAfter)
{
    if (!g_armed)
        return;

    const bool oldBit1 = (oldA0 & 0x02u) != 0;
    const bool newBit1 = (newA0 & 0x02u) != 0;
    const bool falling = oldBit1 && !newBit1;
    const bool rising = !oldBit1 && newBit1;
    const unsigned rawLine = static_cast<unsigned>(master_clk) / 22u;
    const bool clamped = rawLine > 241u;

    A0Write& e = g_a0[g_a0Count % kA0Cap];
    e.seq = g_a0Count;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    e.pc = static_cast<std::uint16_t>(pc);
    e.prev = static_cast<std::uint8_t>(oldA0);
    e.next = static_cast<std::uint8_t>(newA0);
    e.yBefore = static_cast<std::uint8_t>(yBefore);
    e.yAfter = static_cast<std::uint8_t>(yAfter);
    e.hclk = static_cast<std::uint8_t>(h_clk & 0xFF);
    e.mstate = static_cast<std::uint8_t>(mstate);
    e.flags = static_cast<std::uint8_t>((falling ? 0x01 : 0) | (clamped ? 0x02 : 0) | (rising ? 0x04 : 0));
    e.rawLine = static_cast<std::uint16_t>(rawLine);
    ++g_a0Count;

    if (rising)
        ++g_a0Rising;
    if (falling)
    {
        ++g_a0Falling;
        if (clamped) ++g_a0FallGt241;
        if ((yAfter & 0xFFu) == 0xFFu) ++g_a0FallYff;
        g_lastA0Fall = e;
        g_haveLastA0Fall = true;
    }
}

void G7400FreezeTrace_NoteVdc24Read(unsigned returned, unsigned branch)
{
    if (!g_armed)
        return;
    Vdc24Event& e = g_vdc24[g_vdc24Count % kVdc24Cap];
    e.seq = g_vdc24Count;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    e.pc = static_cast<std::uint16_t>(pc);
    e.dir = 0;
    e.requested = 0;
    e.returned = static_cast<std::uint8_t>(returned);
    e.p1 = p1;
    e.a0 = VDCwrite[0xA0];
    e.before = VDCwrite[0x24];
    e.after = VDCwrite[0x24];
    e.flags = (branch == 1u) ? 0x02 : ((branch == 0u) ? 0x04 : 0x08);
    ++g_vdc24Count; ++g_vdc24Reads;
    g_lastVdc24Read = e;
    g_haveLastVdc24Read = true;
}

void G7400FreezeTrace_NoteVdc24Write(unsigned requested, unsigned before,
    unsigned after, bool blocked)
{
    if (!g_armed)
        return;
    Vdc24Event& e = g_vdc24[g_vdc24Count % kVdc24Cap];
    e.seq = g_vdc24Count;
    e.master = static_cast<std::uint32_t>(master_clk);
    e.frame = static_cast<std::uint16_t>(frame & 0xFFFF);
    e.pc = static_cast<std::uint16_t>(pc);
    e.dir = 1;
    e.requested = static_cast<std::uint8_t>(requested);
    e.returned = 0;
    e.p1 = p1;
    e.a0 = VDCwrite[0xA0];
    e.before = static_cast<std::uint8_t>(before);
    e.after = static_cast<std::uint8_t>(after);
    e.flags = blocked ? 0x01 : 0x00;
    ++g_vdc24Count; ++g_vdc24Writes;
    if ((requested & 0xFFu) == 0x3Eu) ++g_vdc24Req3E;
    if (!blocked && (after & 0xFFu) == 0x3Eu) ++g_vdc24Stored3E;
    g_lastVdc24Write = e;
    g_haveLastVdc24Write = true;
}

// Synthetic, opt-in dump-robustness self-test (no emulation, no gameplay).
// Fills the bounded CPU ring with the worst case (a full ring of AAA/AAB/AAC
// rows behind a long lifetime count) and invokes the same WriteLog() the F9
// dump uses. Enabled only by O2EM_G7400_FREEZE_SELFTEST.
bool RunFreezeTraceSelfTestIfRequested(int& result)
{
    const char* env = std::getenv("O2EM_G7400_FREEZE_SELFTEST");
    if (!env || !*env)
        return false;

    const std::uint32_t simulated = 7286507u; // lifetime instruction count
    g_envEnabled = true;
    g_armed = true;
    g_written = false;
    g_dumping = false;
    g_romPath = "SELFTEST";
    g_romCrc = kHelicopterCrc;
    g_bios = "G7400.bin";
    g_region = "PAL";
    g_vpp = 1;
    ResetAll();
    g_instrCount = simulated;
    frame = 1800;

    const std::uint32_t retainedStart = simulated > kInstrCap ? simulated - kInstrCap : 0;
    for (std::uint32_t k = retainedStart; k < simulated; ++k)
    {
        Instr& e = g_instr[k % kInstrCap];
        e.seq = k;
        e.master = static_cast<std::uint32_t>(k % 7259u);
        e.frame = static_cast<std::uint16_t>(k / 4000u);
        e.psw = 0x1D;
        e.flags = 0x02 | 0x04;
        for (int i = 0; i < 8; ++i)
            e.regs[i] = 0;
        e.regs[0] = 0xA4;
        e.regs[5] = 0x3E;
        const std::uint32_t off = k - retainedStart;
        if (off < 1000)
        {
            e.pc = 0x100; e.op = 0x00; e.acc = 0x11; // non-loop prefix
        }
        else
        {
            switch (off % 3)
            {
            case 0: e.pc = 0xAAA; e.op = 0x80; e.acc = 0xC1; break;
            case 1: e.pc = 0xAAB; e.op = 0xDD; e.acc = 0xFF; break;
            default: e.pc = 0xAAC; e.op = 0x96; e.acc = 0xC1; break;
            }
        }
    }

    // Exercise the bounded VDC 0x24 section (synthetic).
    VDCwrite[0x24] = 0x3E;
    G7400FreezeTrace_NoteVdc24Write(0x3E, 0x3E, 0x3E, false); // stored
    G7400FreezeTrace_NoteVdc24Write(0x3E, 0x3E, 0x3E, true);  // blocked attempt
    G7400FreezeTrace_NoteVdc24Read(0xFF, 1);                   // object-approx read

    // Exercise the transition tracer synthetically: one successful R0=0x24
    // wait, one persistent R0=0x24 wait, a RET with irq_ex=1 and a RETR that
    // clears it (post-state inferred from the following observation).
    {
        const std::uint8_t savedBank = reg_pnt;
        const std::uint8_t savedAcc = acc;
        const std::uint8_t savedPsw = psw;
        const std::uint8_t savedSp = sp;
        const std::uint8_t savedP1 = p1;
        const std::uint8_t savedIrqEx = irq_ex;
        const std::uint8_t savedXirq = xirq_en;
        const std::uint8_t savedMstate = mstate;
        const std::uint8_t savedA0 = VDCwrite[0xA0];

        reg_pnt = 24;
        intRAM[24 + 0] = 0x24;
        intRAM[24 + 5] = 0x3E;
        acc = 0x00; psw = 0x1D; sp = 12;
        irq_ex = 0; xirq_en = 1; mstate = 0;
        VDCwrite[0xA0] = 0x02;
        p1 = 0xE6;

        // Post-vector window: vector at PC=003 then a short ISR-like path.
        irq_ex = 1;
        TraceTransition(0x003, 0x00);
        TraceTransition(0x020, 0x00);
        TraceTransition(0x030, 0x00);

        // A) successful wait: JMP into AAA, read == R5 so XRL==0 and JNZ falls.
        TraceTransition(0x600, 0x04);   // JMP (immediate predecessor)
        TraceTransition(0xAAA, 0x80);
        TraceTransition(0xAAB, 0xDD);
        TraceTransition(0xAAC, 0x96);
        acc = 0x00;
        TraceTransition(0x610, 0x00);   // exit

        const unsigned spc[3] = { 0xAAA, 0xAAB, 0xAAC };
        const unsigned sop[3] = { 0x80, 0xDD, 0x96 };

        // B) persistent entry at AAA (predecessor CALL).
        TraceTransition(0x620, 0x14);
        for (std::uint32_t i = 0; i < kPersistentThreshold + 16u; ++i)
            TraceTransition(spc[(0u + i) % 3u], sop[(0u + i) % 3u]);

        // C) close, then persistent entry at AAB.
        TraceTransition(0x630, 0x00);
        TraceTransition(0x640, 0x14);
        for (std::uint32_t i = 0; i < kPersistentThreshold + 16u; ++i)
            TraceTransition(spc[(1u + i) % 3u], sop[(1u + i) % 3u]);

        // D) close, then persistent entry at AAC.
        TraceTransition(0x650, 0x00);
        TraceTransition(0x660, 0x14);
        for (std::uint32_t i = 0; i < kPersistentThreshold + 16u; ++i)
            TraceTransition(spc[(2u + i) % 3u], sop[(2u + i) % 3u]);

        // E) RET while irq_ex=1; irq_ex is unchanged by RET.
        irq_ex = 1;
        TraceTransition(0x700, 0x83);
        TraceTransition(0x701, 0x00);   // observes post-RET irq_ex
        // F) RETR clears irq_ex.
        irq_ex = 1;
        TraceTransition(0x702, 0x93);
        irq_ex = 0;
        TraceTransition(0x703, 0x00);   // observes post-RETR irq_ex

        reg_pnt = savedBank; acc = savedAcc; psw = savedPsw; sp = savedSp;
        p1 = savedP1; irq_ex = savedIrqEx; xirq_en = savedXirq;
        mstate = savedMstate; VDCwrite[0xA0] = savedA0;
    }

    // Raster-wait classification self-test (pure): +1 = target already passed
    // (miss), 0 = exact hit, -1 = beam still behind target.
    {
        const int miss = RasterDeltaOf(0x3F, 0x3E);
        const int hit = RasterDeltaOf(0x3E, 0x3E);
        const int behind = RasterDeltaOf(0x3D, 0x3E);
        std::printf("O2EM-NG: raster-wait self-test: miss=%+d hit=%+d behind=%+d (expect +1/0/-1)\n",
            miss, hit, behind);
        std::fflush(stdout);
    }

    // Debug-only auto-arm decision self-check.
    {
        std::printf("O2EM-NG: freeze-trace auto-arm: debug-build=%d vp59=%d other=%d\n",
            FreezeTraceAutoArmBuildEnabled() ? 1 : 0,
            FreezeTraceAutoArmForCrc(kHelicopterCrc) ? 1 : 0,
            FreezeTraceAutoArmForCrc(0x12345678ul) ? 1 : 0);
        std::fflush(stdout);
    }

    const auto t0 = std::chrono::steady_clock::now();
    WriteLog();    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::printf("O2EM-NG: freeze-trace SELF-TEST wrote g7400_freeze_trace.log in %.1f ms "
        "(simulated %u lifetime instructions, ring cap %u)\n", ms, simulated, kInstrCap);
    std::fflush(stdout);

    // Direct no-env activation checks through the full Configure path.
    G7400FreezeTrace_Configure("SELFTEST", kHelicopterCrc, "G7400.bin", "PAL", 1);
    std::printf("O2EM-NG: freeze-trace auto-arm configure (VP59, no env): enabled=%d\n",
        G7400FreezeTrace_EnvEnabled() ? 1 : 0);
    G7400FreezeTrace_Configure("SELFTEST", 0x12345678ul, "G7400.bin", "PAL", 1);
    std::printf("O2EM-NG: freeze-trace auto-arm configure (other CRC, no env): enabled=%d\n",
        G7400FreezeTrace_EnvEnabled() ? 1 : 0);
    std::fflush(stdout);

    // PAL vertical timing self-check: legacy A4 mapping vs the reference-derived
    // PAL mapping at the proven baseline master (1386).
    {
        const unsigned int legacyA4 = VM_A4BeamValue(1386);
        const unsigned int palA4 = VM_PalA4BeamValue(1386);
        std::printf("O2EM-NG: PAL vertical timing self-test: active=%d "
            "A4(1386) legacy=%02X pal=%02X vbl=%d frame=%d\n",
            VM_PalVerticalTimingActive(), legacyA4, palA4,
            VM_PalVblCycle(), VM_PalFrameBase());
        std::fflush(stdout);
    }

    g_armed = false;
    result = 0;
    return true;
}
} // namespace mcs48
