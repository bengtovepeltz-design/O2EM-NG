#include "c7010.h"
#include "cpu.h"

#include <array>
#include <cstdio>
#include <cstring>

#define C7010_TRACE_PRINT(...) do { if constexpr (O2EM_C7010_TRACE != 0) std::printf(__VA_ARGS__); } while (0)

namespace
{
    constexpr unsigned long kChessCartCrc = 0x77066338UL;
    constexpr std::size_t kFirmwareSize = 0x2000;
    constexpr std::size_t kRamSize = 0x0800;

    bool gEnabled = false;
    bool gFirmwareLoaded = false;
    bool gCpuReset = true;
    bool gExecutionNeedsReset = true;
    unsigned long gInterleaveCredit = 0;
    Byte gP1 = 0xFF;

    // Two one-byte latches:
    // 8048 -> NSC800 and NSC800 -> 8048.
    Byte gToNsc800 = 0x00;
    Byte gTo8048 = 0x00;

    // Patch 0030J: bounded latch/bus trace. This does not alter hardware
    // behaviour; it records the exact 8048 <-> NSC800 exchange so the
    // ENTER/move-commit boundary can be diagnosed without guessing.
    unsigned long gBusTraceSequence = 0;
    unsigned short gNscPcForTrace = 0;
    unsigned long gNscIoTraceSequence = 0;
    unsigned int gNscIoTraceLines = 0;
    constexpr unsigned int kNscIoTraceLimit = 1200;
    unsigned int gBusTraceLines = 0;
    unsigned long gToNscWriteGeneration = 0;
    unsigned long gToNscReadGeneration = ~0UL;
    unsigned long gTo8048WriteGeneration = 0;
    unsigned long gTo8048ReadGeneration = ~0UL;
    constexpr unsigned int kBusTraceLineLimit = 600;

    // 0030Q: independent ENTER-triggered diagnostics; never change latch state.
    unsigned int gMoveSlices = 0, gMoveLines = 0;
    unsigned long gMovePolls[2] = {}, gMoveReads[2] = {}, gMoveWrites[2] = {};
    unsigned long gMoveSeen[2] = {~0UL, ~0UL};
    void MoveEvent(unsigned int direction, bool write, unsigned int selector, Byte value)
    {
        if (!gMoveSlices) return;
        const unsigned long generation = direction ? gTo8048WriteGeneration : gToNscWriteGeneration;
        if (write) ++gMoveWrites[direction];
        else {
            ++gMoveReads[direction];
            if (gMoveSeen[direction] == generation) { ++gMovePolls[direction]; return; }
            gMoveSeen[direction] = generation;
        }
        if (gMoveLines >= 2000) return;
        ++gMoveLines;
        C7010_TRACE_PRINT("O2EM-NG: C7010 0030Q COMM slice=%u %s %s sel=%02X value=%02X gen=%lu 8048PC=%03X NSCPC=%04X P1=%02X in=%02X out=%02X\n",
            300u-gMoveSlices, direction ? "NSC->8048" : "8048->NSC",
            write ? "WRITE" : "READ", selector, value, generation,
            static_cast<unsigned int>(lastpc), gNscPcForTrace, gP1, gToNsc800, gTo8048);
        if (gMoveLines == 2000)
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030Q detail limit reached; slice summaries continue.\n");
    }

    // 0030K raw 8048 MOVX trace lives outside the C7010 routing decision.
    // This is deliberately separate from the latch trace above so we can see
    // why an access was NOT claimed by C7010.
    unsigned int gRaw8048TraceLines = 0;
    unsigned long gRaw8048TraceSequence = 0;
    constexpr unsigned int kRaw8048TraceLimit = 800;

    // Patch 0030N: focused service-manual latch-control trace.
    // Keep the previously working P10+P14 interface qualification and record
    // only accesses that are actually accepted by the C7010 interface, plus
    // the NSC800 side of the two hardware latches.
    unsigned long gLatchCtrlSequence = 0;
    unsigned int gLatchCtrlLines = 0;
    constexpr unsigned int kLatchCtrlLimit = 500;

    void TraceLatchCtrl(const char* actor, const char* action, unsigned int selector, Byte value)
    {
    if constexpr (!O2EM_C7010_TRACE) return;
        if (gLatchCtrlLines >= kLatchCtrlLimit)
            return;

        ++gLatchCtrlSequence;
        ++gLatchCtrlLines;
        const unsigned int p10 = (gP1 >> 0) & 1u;
        const unsigned int p11 = (gP1 >> 1) & 1u;
        const unsigned int p14 = (gP1 >> 4) & 1u;
        C7010_TRACE_PRINT(
            "O2EM-NG: C7010 LATCH #%lu %-6s %-5s sel=%02X data=%02X P1=%02X P10=%u P11=%u P14=%u NSCPC=%04X in=%02X out=%02X\n",
            gLatchCtrlSequence, actor, action, selector & 0xffu, value, gP1,
            p10, p11, p14, gNscPcForTrace, gToNsc800, gTo8048);

        if (gLatchCtrlLines == kLatchCtrlLimit)
            C7010_TRACE_PRINT("O2EM-NG: C7010 LATCH trace limit reached (%u lines).\n", kLatchCtrlLimit);
    }

    void TraceBus(const char* actor, const char* action, unsigned int selector, Byte value)
    {
        // Patch 0030L-R2: generic BUS trace disabled so focused NSCIO/PC112x output is not drowned.
        return;
        if (gBusTraceLines >= kBusTraceLineLimit)
            return;

        ++gBusTraceSequence;
        ++gBusTraceLines;
        C7010_TRACE_PRINT("O2EM-NG: C7010 BUS #%lu %-6s %-5s sel=%02X data=%02X P1=%02X NSCPC=%04X\n",
            gBusTraceSequence, actor, action, selector & 0xFFu, value, gP1, gNscPcForTrace);

        if (gBusTraceLines == kBusTraceLineLimit)
            C7010_TRACE_PRINT("O2EM-NG: C7010 BUS trace limit reached (%u lines); restart/reset to capture a fresh transaction window.\n",
                kBusTraceLineLimit);
    }

    std::array<Byte, kFirmwareSize> gFirmware{};
    std::array<Byte, kRamSize> gRam{};
    // 0030T: compact input history is independent of the old startup trace limits.
    unsigned int gInputTraceLines = 0, gDecisionTraceLines = 0, gValidationLines = 0;
    void TraceInput(const char* action, Byte value)
    {
    if constexpr (!O2EM_C7010_TRACE) return;
        if (value == 0xFF || gInputTraceLines >= 512) return;
        ++gInputTraceLines;
        C7010_TRACE_PRINT("O2EM-NG: C7010 0030T INPUT %s value=%02X 8048PC=%03X NSCPC=%04X P1=%02X FF58=%02X FF59=%02X FF5A=%02X\n",
            action, value, static_cast<unsigned int>(lastpc), gNscPcForTrace, gP1,
            gRam[0x758], gRam[0x759], gRam[0x75A]);
        if (gInputTraceLines == 512)
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030T INPUT limit reached; reset for a new capture.\n");
    }

    bool InterfaceSelected()
    {
        // C7010 uses P10 and P14 low to select the module interface.
        return (gP1 & 0x11) == 0;
    }
}

// 0030V: three sampled frames, independently bounded from startup logging.
static unsigned int gVideoTraceLines = 0;
static unsigned int gBoardTraceLines = 0;
void C7010_TraceBoardWrite(unsigned int address, Byte oldValue, Byte value)
{
    if constexpr (!O2EM_C7010_TRACE) return;
    if (!gEnabled || !gMoveSlices || address >= 0x5A || oldValue == value || gBoardTraceLines >= 1024) return;
    ++gBoardTraceLines;
    C7010_TRACE_PRINT("O2EM-NG: C7010 0030Z EXTRAM slice=%u PC=%03X adr=%02X old=%02X new=%02X\n",
        300u-gMoveSlices, static_cast<unsigned int>(lastpc), address, oldValue, value);
    if (gBoardTraceLines == 1024) C7010_TRACE_PRINT("O2EM-NG: C7010 0030Z RAM trace limit reached.\n");
}
static unsigned int gRasterSamples = 0;
bool C7010_TakeRasterSample()
{
    if constexpr (!O2EM_C7010_TRACE) return false;
    if (!gEnabled || gMoveSlices != 270 || gRasterSamples >= 1600) return false;
    ++gRasterSamples;
    return true;
}
bool C7010_TraceVideoFrame()
{
    if constexpr (!O2EM_C7010_TRACE) return false;
    return gEnabled && (gMoveSlices == 299 || gMoveSlices == 270 || gMoveSlices == 180)
        && gVideoTraceLines < 400;
}
void C7010_TraceVdcWrite(unsigned int address, Byte value, bool blocked, int clock)
{
    if constexpr (!O2EM_C7010_TRACE) return;
    if (!C7010_TraceVideoFrame()) return;
    if (address != 0xA0 && (address >= 0x80 || (address & 3) != 0)) return;
    ++gVideoTraceLines;
    C7010_TRACE_PRINT("O2EM-NG: C7010 0030V VDC slice=%u clk=%d PC=%03X adr=%02X data=%02X blocked=%u\n",
        300-gMoveSlices, clock, static_cast<unsigned int>(lastpc), address, value, blocked ? 1u : 0u);
    if (gVideoTraceLines == 400) C7010_TRACE_PRINT("O2EM-NG: C7010 0030V video detail limit reached.\n");
}
void C7010_ArmMoveTrace()
{
    if constexpr (!O2EM_C7010_TRACE) return;
    if (!gEnabled) return;
    gVideoTraceLines = 0;
    gBoardTraceLines = 0;
    gRasterSamples = 0;
    gMoveSlices = 300;
    gMoveLines = 0;
    for (unsigned int d=0; d<2; ++d) {
        gMoveSeen[d] = ~0UL;
        gMovePolls[d] = gMoveReads[d] = gMoveWrites[d] = 0;
    }
    C7010_TRACE_PRINT("O2EM-NG: C7010 0030Q COMM BEGIN - 300 slices; in=%02X out=%02X reset=%u firmware=%u\n",
        gToNsc800, gTo8048, gCpuReset ? 1u : 0u, gFirmwareLoaded ? 1u : 0u);
}

bool C7010_IsEnabled()
{
    return gEnabled;
}

bool C7010_ConfigureForCartridge(unsigned long cartridgeCrc)
{
    gEnabled = cartridgeCrc == kChessCartCrc;

    if (gEnabled)
    {
        std::printf("O2EM-NG: C7010 0030AD beta baseline (0030AC fixes retained, trace=%d)\n", O2EM_C7010_TRACE);
        std::printf("O2EM-NG: C7010 Chess cartridge detected (CRC 77066338)\n");
        std::printf("O2EM-NG: C7010 external NSC800 module enabled\n");
    }

    return gEnabled;
}

bool C7010_LoadFirmware(const char* path)
{
    gFirmwareLoaded = false;

    if (!gEnabled || !path)
        return false;

    FILE* file = std::fopen(path, "rb");
    if (!file)
    {
        std::printf("O2EM-NG: C7010 firmware not found: %s\n", path);
        return false;
    }

    const std::size_t count = std::fread(gFirmware.data(), 1, gFirmware.size(), file);
    std::fclose(file);

    if (count != gFirmware.size())
    {
        std::printf("O2EM-NG: C7010 firmware has wrong size (%zu, expected 8192)\n", count);
        return false;
    }

    gFirmwareLoaded = true;
    std::printf("O2EM-NG: C7010 NSC800 firmware loaded: %s\n", path);
    return true;
}

void C7010_Reset()
{
    if (!gEnabled)
        return;

    std::fill(gRam.begin(), gRam.end(), 0);
    gInterleaveCredit = 0;
    gInputTraceLines = gDecisionTraceLines = gValidationLines = 0;
    gMoveSlices = 0;
    gToNsc800 = 0;
    gTo8048 = 0;
    gP1 = 0xFF;
    gCpuReset = true;
    gExecutionNeedsReset = true;
    gBusTraceSequence = 0;
    gBusTraceLines = 0;
    gNscPcForTrace = 0;
    gNscIoTraceSequence = 0;
    gNscIoTraceLines = 0;
    gToNscWriteGeneration = 0;
    gToNscReadGeneration = ~0UL;
    gTo8048WriteGeneration = 0;
    gTo8048ReadGeneration = ~0UL;
    gRaw8048TraceLines = 0;
    gRaw8048TraceSequence = 0;
    gLatchCtrlSequence = 0;
    gLatchCtrlLines = 0;

    C7010_TRACE_PRINT("O2EM-NG: C7010 0030N restored P10+P14 select; latch-control trace ENABLED\n");
    C7010_TRACE_PRINT("O2EM-NG: C7010 reset (firmware=%s)\n",
        gFirmwareLoaded ? "ready" : "missing");
}

void C7010_WriteP1(Byte value)
{
    if (!gEnabled)
        return;

    const bool oldReset = gCpuReset;
    const Byte oldP1 = gP1;
    gP1 = value;

    // Log only meaningful C7010 control-line transitions (P10/P11/P14).
    if ((oldP1 & 0x13) != (value & 0x13))
        TraceBus("8048", "P1", value & 0x13, value);

    // C7010 P11 controls the external processor reset.
    // Low = reset asserted, high = processor released.
    gCpuReset = (value & 0x02) == 0;

    if (oldReset != gCpuReset)
    {
        C7010_TRACE_PRINT("O2EM-NG: C7010 NSC800 %s\n",
            gCpuReset ? "RESET asserted" : "RESET released");
        if (gCpuReset)
            gExecutionNeedsReset = true;
    }
}

void C7010_Trace8048ExternalAccess(bool isWrite, ADDRESS address, Byte value)
{
    if constexpr (!O2EM_C7010_TRACE) return;
    if (!gEnabled || gRaw8048TraceLines >= kRaw8048TraceLimit)
        return;

    ++gRaw8048TraceSequence;
    ++gRaw8048TraceLines;

    const bool selected = InterfaceSelected();
    const bool readWindow = (address & 0xA0) == 0xA0;
    const bool writeWindow = (address & 0x80) != 0 && (gP1 & 0x40) == 0;

    C7010_TRACE_PRINT(
        "O2EM-NG: C7010 RAW8048 #%lu %s adr=%02X data=%02X P1=%02X SEL=%u RW=%u WW=%u NSCPC=%04X\n",
        gRaw8048TraceSequence, isWrite ? "WRITE" : "READ ",
        static_cast<unsigned int>(address) & 0xFFu, value, gP1,
        selected ? 1u : 0u, readWindow ? 1u : 0u, writeWindow ? 1u : 0u,
        gNscPcForTrace);

    if (gRaw8048TraceLines == kRaw8048TraceLimit)
        C7010_TRACE_PRINT("O2EM-NG: C7010 RAW8048 trace limit reached (%u lines).\n",
            kRaw8048TraceLimit);
}

bool C7010_ExternalRead(ADDRESS address, Byte& value)
{
    if (!gEnabled || !InterfaceSelected())
        return false;

    // C7010 only drives the 8048 external data bus for the documented
    // A7/A5-selected read window. Do not swallow unrelated VDC/VPP reads.
    if ((address & 0xA0) != 0xA0)
        return false;

    // The 8048 reads the byte returned by the external processor.
    // Keep this intentionally conservative for 0030A: only intercept the
    // C7010-selected external bus, leaving normal VDC/VPP accesses untouched.
    (void)address;
    value = gTo8048;
    if (gTo8048ReadGeneration != gTo8048WriteGeneration)
        TraceInput("8048-READ", value);
    MoveEvent(1, false, static_cast<unsigned int>(address), value);
    TraceLatchCtrl("8048", "READ", static_cast<unsigned int>(address), value);
    // A real latch may be polled many times. Trace the first observation of
    // each newly written byte rather than flooding the console with polls.
    if (gTo8048ReadGeneration != gTo8048WriteGeneration)
    {
        TraceBus("8048", "READ", static_cast<unsigned int>(address), value);
        gTo8048ReadGeneration = gTo8048WriteGeneration;
    }
    return true;
}

bool C7010_ExternalWrite(ADDRESS address, Byte value)
{
    // 0030R: P16 high blocks cartridge writes, not VDC writes.
    if (!gEnabled || !InterfaceSelected() || (gP1 & 0x40))
        return false;

    // Hardware uses the cartridge address bus as part of the latch select.
    // For the foundation patch we only accept the documented high-half write.
    if ((address & 0x80) == 0)
        return false;

    gToNsc800 = value;
    ++gToNscWriteGeneration;
    TraceInput("8048-WRITE", value);
    MoveEvent(0, true, static_cast<unsigned int>(address), value);
    TraceLatchCtrl("8048", "WRITE", static_cast<unsigned int>(address), value);
    TraceBus("8048", "WRITE", static_cast<unsigned int>(address), value);
    return true;
}

Byte C7010_NSC800_ReadMemory(unsigned short address)
{
    if (address < 0x2000)
        return gFirmware[address];

    // 2 KiB RAM is mirrored through the upper 8 KiB region.
    if (address >= 0xE000)
        return gRam[address & 0x07FF];

    return 0xFF;
}

void C7010_NSC800_WriteMemory(unsigned short address, Byte value)
{
    if (address >= 0xE000)
        gRam[address & 0x07FF] = value;
}

Byte C7010_NSC800_In(Byte port)
{
    const Byte value = gToNsc800;
    if (gToNscReadGeneration != gToNscWriteGeneration)
        TraceInput("NSC-READ", value);
    MoveEvent(0, false, port, value);
    TraceLatchCtrl("NSC800", "READ", static_cast<unsigned int>(port), value);

    // Patch 0030L: dedicated NSC800 I/O trace. Unlike the generic bus trace,
    // this records repeated polls too. Chess currently sits around 1124-112B,
    // so we need to see whether it is really polling IN forever or reaches OUT.
    if (gNscIoTraceLines < kNscIoTraceLimit)
    {
        ++gNscIoTraceSequence;
        ++gNscIoTraceLines;
        C7010_TRACE_PRINT(
            "O2EM-NG: C7010 NSCIO #%lu IN  PC=%04X port=%02X value=%02X toNSCgen=%lu/%lu to8048=%02X outgen=%lu/%lu\n",
            gNscIoTraceSequence, gNscPcForTrace, port, value,
            gToNscReadGeneration, gToNscWriteGeneration, gTo8048,
            gTo8048ReadGeneration, gTo8048WriteGeneration);
        if (gNscIoTraceLines == kNscIoTraceLimit)
            C7010_TRACE_PRINT("O2EM-NG: C7010 NSCIO trace limit reached (%u lines).\n",
                kNscIoTraceLimit);
    }

    if (gToNscReadGeneration != gToNscWriteGeneration)
    {
        TraceBus("NSC800", "READ", static_cast<unsigned int>(port), value);
        gToNscReadGeneration = gToNscWriteGeneration;
    }
    return value;
}

void C7010_NSC800_Out(Byte port, Byte value)
{
    TraceLatchCtrl("NSC800", "WRITE", static_cast<unsigned int>(port), value);
    if (gNscIoTraceLines < kNscIoTraceLimit)
    {
        ++gNscIoTraceSequence;
        ++gNscIoTraceLines;
        C7010_TRACE_PRINT(
            "O2EM-NG: C7010 NSCIO #%lu OUT PC=%04X port=%02X value=%02X old8048=%02X outgen=%lu/%lu\n",
            gNscIoTraceSequence, gNscPcForTrace, port, value, gTo8048,
            gTo8048ReadGeneration, gTo8048WriteGeneration);
    }

    gTo8048 = value;
    ++gTo8048WriteGeneration;
    TraceInput("NSC-WRITE", value);
    MoveEvent(1, true, port, value);
    TraceBus("NSC800", "WRITE", static_cast<unsigned int>(port), value);
}

// ---------------------------------------------------------------------------
// Patch 0030B - NSC800/Z80 startup executor (diagnostic first stage)
// ---------------------------------------------------------------------------
namespace
{
    struct Nsc800DiagState
    {
        unsigned short pc = 0;
        unsigned short sp = 0xFFFF;
        unsigned short hl = 0;
        unsigned short ix = 0;
        unsigned short iy = 0;
        Byte a = 0, b = 0, c = 0, d = 0, e = 0;
        Byte f = 0;
        Byte a2 = 0, f2 = 0, b2 = 0, c2 = 0, d2 = 0, e2 = 0, h2 = 0, l2 = 0;
        Byte i = 0, r = 0;
        Byte interruptMode = 0;
        bool iff1 = false;
        bool iff2 = false;
        unsigned long instructions = 0;
        unsigned short djnzPc = 0xFFFF;
        unsigned short djnzTarget = 0xFFFF;
        unsigned int djnzIterations = 0;
        bool initialized = false;
        bool stopped = false;
    };

    Nsc800DiagState gNsc;

    Byte NscFetch8() { return C7010_NSC800_ReadMemory(gNsc.pc++); }
    unsigned short NscFetch16()
    {
        const Byte lo = NscFetch8();
        const Byte hi = NscFetch8();
        return static_cast<unsigned short>(lo | (static_cast<unsigned short>(hi) << 8));
    }
    void NscPush16(unsigned short v)
    {
        C7010_NSC800_WriteMemory(--gNsc.sp, static_cast<Byte>(v >> 8));
        C7010_NSC800_WriteMemory(--gNsc.sp, static_cast<Byte>(v));
    }
    unsigned short NscPop16()
    {
        const Byte lo = C7010_NSC800_ReadMemory(gNsc.sp++);
        const Byte hi = C7010_NSC800_ReadMemory(gNsc.sp++);
        return static_cast<unsigned short>(lo | (static_cast<unsigned short>(hi) << 8));
    }

    Byte NscInc8(Byte value)
    {
        // Z80/NSC800 INC r flags:
        // S, Z, H, P/V, N and undocumented 5/3 are updated; C is preserved.
        const Byte result = static_cast<Byte>(value + 1);
        Byte flags = static_cast<Byte>(gNsc.f & 0x01); // preserve carry

        flags |= static_cast<Byte>(result & 0xA8);      // S, Y(bit5), X(bit3)
        if (result == 0)
            flags |= 0x40;                             // Z
        if ((value & 0x0F) == 0x0F)
            flags |= 0x10;                             // H
        if (value == 0x7F)
            flags |= 0x04;                             // P/V (signed overflow)
        // N is cleared by INC.

        gNsc.f = flags;
        return result;
    }

    Byte NscDec8(Byte value)
    {
        // Z80/NSC800 DEC r flags:
        // S, Z, H, P/V, N and undocumented 5/3 are updated; C is preserved.
        const Byte result = static_cast<Byte>(value - 1);
        Byte flags = static_cast<Byte>((gNsc.f & 0x01) | 0x02); // preserve C, set N

        flags |= static_cast<Byte>(result & 0xA8);              // S, Y(bit5), X(bit3)
        if (result == 0)
            flags |= 0x40;                                     // Z
        if ((value & 0x0F) == 0x00)
            flags |= 0x10;                                     // H (borrow from bit 4)
        if (value == 0x80)
            flags |= 0x04;                                     // P/V (signed overflow)

        gNsc.f = flags;
        return result;
    }


    void NscAndA()
    {
        // Z80/NSC800 AND A: result is A itself.
        // S, Z, P/V and undocumented 5/3 follow the result; H=1, N=0, C=0.
        const Byte result = gNsc.a;
        Byte flags = static_cast<Byte>((result & 0xA8) | 0x10); // S,Y,X + H
        if (result == 0)
            flags |= 0x40; // Z

        // P/V is parity for logical operations: set for even parity.
        Byte parity = result;
        parity ^= static_cast<Byte>(parity >> 4);
        parity ^= static_cast<Byte>(parity >> 2);
        parity ^= static_cast<Byte>(parity >> 1);
        if ((parity & 1) == 0)
            flags |= 0x04;

        gNsc.f = flags;
    }

    unsigned short NscGetBC()
    {
        return static_cast<unsigned short>((static_cast<unsigned short>(gNsc.b) << 8) | gNsc.c);
    }

    bool NscEvenParity(Byte value)
    {
        value ^= static_cast<Byte>(value >> 4);
        value ^= static_cast<Byte>(value >> 2);
        value ^= static_cast<Byte>(value >> 1);
        return (value & 1) == 0;
    }

    void NscSetLogicFlags(Byte result, bool halfCarry)
    {
        Byte flags = static_cast<Byte>(result & 0xA8);
        if (result == 0) flags |= 0x40;
        if (halfCarry) flags |= 0x10;
        if (NscEvenParity(result)) flags |= 0x04;
        gNsc.f = flags;
    }

    Byte NscAdd8(Byte lhs, Byte rhs, Byte carryIn)
    {
        const unsigned int sum = static_cast<unsigned int>(lhs) + rhs + carryIn;
        const Byte result = static_cast<Byte>(sum);
        Byte flags = static_cast<Byte>(result & 0xA8);
        if (result == 0) flags |= 0x40;
        if (((lhs & 0x0F) + (rhs & 0x0F) + carryIn) > 0x0F) flags |= 0x10;
        if (((~(lhs ^ rhs) & (lhs ^ result)) & 0x80) != 0) flags |= 0x04;
        if (sum > 0xFF) flags |= 0x01;
        gNsc.f = flags;
        return result;
    }

    Byte NscSub8(Byte lhs, Byte rhs, Byte carryIn)
    {
        const int diff = static_cast<int>(lhs) - static_cast<int>(rhs) - carryIn;
        const Byte result = static_cast<Byte>(diff);
        Byte flags = static_cast<Byte>((result & 0xA8) | 0x02);
        if (result == 0) flags |= 0x40;
        if ((lhs & 0x0F) < ((rhs & 0x0F) + carryIn)) flags |= 0x10;
        if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80) != 0) flags |= 0x04;
        if (diff < 0) flags |= 0x01;
        gNsc.f = flags;
        return result;
    }

    Byte NscReadReg(unsigned int code)
    {
        switch (code & 7u)
        {
        case 0: return gNsc.b;
        case 1: return gNsc.c;
        case 2: return gNsc.d;
        case 3: return gNsc.e;
        case 4: return static_cast<Byte>(gNsc.hl >> 8);
        case 5: return static_cast<Byte>(gNsc.hl);
        case 6: return C7010_NSC800_ReadMemory(gNsc.hl);
        default:return gNsc.a;
        }
    }

    void NscWriteReg(unsigned int code, Byte value)
    {
        switch (code & 7u)
        {
        case 0: gNsc.b = value; break;
        case 1: gNsc.c = value; break;
        case 2: gNsc.d = value; break;
        case 3: gNsc.e = value; break;
        case 4: gNsc.hl = static_cast<unsigned short>((gNsc.hl & 0x00FFu) | (static_cast<unsigned short>(value) << 8)); break;
        case 5: gNsc.hl = static_cast<unsigned short>((gNsc.hl & 0xFF00u) | value); break;
        case 6: C7010_NSC800_WriteMemory(gNsc.hl, value); break;
        default:gNsc.a = value; break;
        }
    }

    bool NscExecuteRegisterMatrix(Byte op)
    {
        if (op >= 0x40 && op <= 0x7F)
        {
            if (op == 0x76)
                return false; // HALT stays in the explicit switch below.
            NscWriteReg((op >> 3) & 7u, NscReadReg(op & 7u));
            return true;
        }

        if (op < 0x80 || op > 0xBF)
            return false;

        const Byte value = NscReadReg(op & 7u);
        switch ((op >> 3) & 7u)
        {
        case 0: gNsc.a = NscAdd8(gNsc.a, value, 0); break;                    // ADD A,r
        case 1: gNsc.a = NscAdd8(gNsc.a, value, (gNsc.f & 1) ? 1 : 0); break; // ADC A,r
        case 2: gNsc.a = NscSub8(gNsc.a, value, 0); break;                    // SUB r
        case 3: gNsc.a = NscSub8(gNsc.a, value, (gNsc.f & 1) ? 1 : 0); break; // SBC A,r
        case 4: gNsc.a = static_cast<Byte>(gNsc.a & value); NscSetLogicFlags(gNsc.a, true); break;
        case 5: gNsc.a = static_cast<Byte>(gNsc.a ^ value); NscSetLogicFlags(gNsc.a, false); break;
        case 6: gNsc.a = static_cast<Byte>(gNsc.a | value); NscSetLogicFlags(gNsc.a, false); break;
        case 7: (void)NscSub8(gNsc.a, value, 0); break;                        // CP r
        }
        return true;
    }

    void NscSetBC(unsigned short value)
    {
        gNsc.b = static_cast<Byte>(value >> 8);
        gNsc.c = static_cast<Byte>(value);
    }

    unsigned short NscGetDE()
    {
        return static_cast<unsigned short>((static_cast<unsigned short>(gNsc.d) << 8) | gNsc.e);
    }

    void NscSetDE(unsigned short value)
    {
        gNsc.d = static_cast<Byte>(value >> 8);
        gNsc.e = static_cast<Byte>(value);
    }

    bool NscCondition(unsigned int code)
    {
        switch (code & 7u)
        {
        case 0: return (gNsc.f & 0x40) == 0; // NZ
        case 1: return (gNsc.f & 0x40) != 0; // Z
        case 2: return (gNsc.f & 0x01) == 0; // NC
        case 3: return (gNsc.f & 0x01) != 0; // C
        case 4: return (gNsc.f & 0x04) == 0; // PO
        case 5: return (gNsc.f & 0x04) != 0; // PE
        case 6: return (gNsc.f & 0x80) == 0; // P
        default:return (gNsc.f & 0x80) != 0; // M
        }
    }

    void NscConditionalRet(unsigned int cc)
    {
        if (NscCondition(cc))
            gNsc.pc = NscPop16();
    }

    void NscConditionalCall(unsigned int cc)
    {
        const unsigned short target = NscFetch16();
        if (NscCondition(cc))
        {
            NscPush16(gNsc.pc);
            gNsc.pc = target;
        }
    }

    void NscConditionalJump(unsigned int cc)
    {
        const unsigned short target = NscFetch16();
        if (NscCondition(cc))
            gNsc.pc = target;
    }

    void NscRst(unsigned short vector)
    {
        NscPush16(gNsc.pc);
        gNsc.pc = vector;
    }

    unsigned short NscAdd16(unsigned short lhs, unsigned short rhs)
    {
        const unsigned int sum = static_cast<unsigned int>(lhs) + rhs;
        Byte flags = static_cast<Byte>(gNsc.f & (0x80 | 0x40 | 0x04)); // preserve S/Z/PV
        const unsigned short result = static_cast<unsigned short>(sum);
        flags |= static_cast<Byte>((result >> 8) & 0x28);
        if (((lhs & 0x0FFFu) + (rhs & 0x0FFFu)) > 0x0FFFu) flags |= 0x10;
        if (sum > 0xFFFFu) flags |= 0x01;
        gNsc.f = flags;
        return result;
    }

    bool NscExecuteCB(unsigned short)
    {
        const Byte op2 = NscFetch8();
        const unsigned int group = op2 >> 6;
        const unsigned int bit = (op2 >> 3) & 7u;
        const unsigned int reg = op2 & 7u;
        Byte value = NscReadReg(reg);

        if (group == 0)
        {
            Byte result = value;
            Byte carry = 0;
            switch (bit)
            {
            case 0: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | carry); break; // RLC
            case 1: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (carry << 7)); break; // RRC
            case 2: { const Byte oldC = static_cast<Byte>(gNsc.f & 1); carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | oldC); break; } // RL
            case 3: { const Byte oldC = static_cast<Byte>(gNsc.f & 1); carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (oldC << 7)); break; } // RR
            case 4: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>(value << 1); break; // SLA
            case 5: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (value & 0x80)); break; // SRA
            case 6: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | 1); break; // SLL (undocumented Z80, harmless for NSC800 compatibility)
            case 7: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>(value >> 1); break; // SRL
            }
            NscWriteReg(reg, result);
            Byte flags = static_cast<Byte>(result & 0xA8);
            if (result == 0) flags |= 0x40;
            if (NscEvenParity(result)) flags |= 0x04;
            flags |= carry;
            gNsc.f = flags;
            return true;
        }

        if (group == 1) // BIT b,r
        {
            const bool set = (value & static_cast<Byte>(1u << bit)) != 0;
            Byte flags = static_cast<Byte>((gNsc.f & 0x01) | 0x10); // preserve C, set H, clear N
            if (!set) flags |= static_cast<Byte>(0x40 | 0x04);      // Z and P/V
            if (bit == 7 && set) flags |= 0x80;
            flags |= static_cast<Byte>(value & 0x28);
            gNsc.f = flags;
            return true;
        }

        if (group == 2) // RES b,r
            value = static_cast<Byte>(value & ~static_cast<Byte>(1u << bit));
        else            // SET b,r
            value = static_cast<Byte>(value | static_cast<Byte>(1u << bit));

        NscWriteReg(reg, value);
        return true;
    }

    bool NscExecuteED(unsigned short prefixPc);

    unsigned short NscIndexedAddress(unsigned short index, signed char displacement)
    {
        return static_cast<unsigned short>(index + displacement);
    }

    Byte NscReadIndexedReg(unsigned int code, unsigned short index, bool memoryUsesDisplacement, signed char displacement)
    {
        switch (code & 7u)
        {
        case 0: return gNsc.b;
        case 1: return gNsc.c;
        case 2: return gNsc.d;
        case 3: return gNsc.e;
        case 4: return static_cast<Byte>(index >> 8);      // IXH/IYH
        case 5: return static_cast<Byte>(index);           // IXL/IYL
        case 6:
            return C7010_NSC800_ReadMemory(memoryUsesDisplacement
                ? NscIndexedAddress(index, displacement)
                : index);
        default: return gNsc.a;
        }
    }

    void NscWriteIndexedReg(unsigned int code, Byte value, unsigned short& index,
                            bool memoryUsesDisplacement, signed char displacement)
    {
        switch (code & 7u)
        {
        case 0: gNsc.b = value; break;
        case 1: gNsc.c = value; break;
        case 2: gNsc.d = value; break;
        case 3: gNsc.e = value; break;
        case 4: index = static_cast<unsigned short>((index & 0x00FFu) | (static_cast<unsigned short>(value) << 8)); break;
        case 5: index = static_cast<unsigned short>((index & 0xFF00u) | value); break;
        case 6:
            C7010_NSC800_WriteMemory(memoryUsesDisplacement
                ? NscIndexedAddress(index, displacement)
                : index, value);
            break;
        default: gNsc.a = value; break;
        }
    }

    bool NscExecuteIndexedCB(unsigned short prefixPc, unsigned short& index, const char* name)
    {
        const signed char displacement = static_cast<signed char>(NscFetch8());
        const Byte op = NscFetch8();

        const unsigned short address = NscIndexedAddress(index, displacement);
        if (gNsc.instructions < 192)
            C7010_TRACE_PRINT("O2EM-NG: NSC800 %sCB PC=%04X d=%d OP=%02X EA=%04X\n",
                name, prefixPc, static_cast<int>(displacement), op, address);
        Byte value = C7010_NSC800_ReadMemory(address);
        const unsigned int group = op >> 6;
        const unsigned int bit = (op >> 3) & 7u;
        const unsigned int reg = op & 7u;

        if (group == 0)
        {
            Byte result = value;
            Byte carry = 0;
            switch (bit)
            {
            case 0: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | carry); break; // RLC
            case 1: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (carry << 7)); break; // RRC
            case 2: { const Byte oldC = static_cast<Byte>(gNsc.f & 1); carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | oldC); break; } // RL
            case 3: { const Byte oldC = static_cast<Byte>(gNsc.f & 1); carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (oldC << 7)); break; } // RR
            case 4: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>(value << 1); break; // SLA
            case 5: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>((value >> 1) | (value & 0x80)); break; // SRA
            case 6: carry = static_cast<Byte>(value >> 7); result = static_cast<Byte>((value << 1) | 1); break; // SLL
            case 7: carry = static_cast<Byte>(value & 1); result = static_cast<Byte>(value >> 1); break; // SRL
            }
            C7010_NSC800_WriteMemory(address, result);
            if (reg != 6)
                NscWriteReg(reg, result);
            Byte flags = static_cast<Byte>(result & 0xA8);
            if (result == 0) flags |= 0x40;
            if (NscEvenParity(result)) flags |= 0x04;
            flags |= carry;
            gNsc.f = flags;
            return true;
        }

        if (group == 1) // BIT b,(IX/IY+d)
        {
            const bool set = (value & static_cast<Byte>(1u << bit)) != 0;
            Byte flags = static_cast<Byte>((gNsc.f & 0x01) | 0x10); // preserve C, H=1, N=0
            if (!set) flags |= static_cast<Byte>(0x40 | 0x04);
            if (bit == 7 && set) flags |= 0x80;
            // Indexed BIT gets undocumented 5/3 from the effective-address high byte.
            flags |= static_cast<Byte>((address >> 8) & 0x28);
            gNsc.f = flags;
            return true;
        }

        const Byte result = (group == 2)
            ? static_cast<Byte>(value & ~static_cast<Byte>(1u << bit))
            : static_cast<Byte>(value | static_cast<Byte>(1u << bit));
        C7010_NSC800_WriteMemory(address, result);
        if (reg != 6)
            NscWriteReg(reg, result);
        return true;
    }

    bool NscExecuteIndexed(unsigned short prefixPc, unsigned short& index, const char* name)
    {
        const Byte op = NscFetch8();


        if (gNsc.instructions < 192)
            C7010_TRACE_PRINT("O2EM-NG: NSC800 %s-prefix PC=%04X OP=%s%02X %s=%04X\n",
                name, prefixPc, name, op, name + 1, index);

        switch (op)
        {
        case 0x09: index = NscAdd16(index, NscGetBC()); return true;
        case 0x19: index = NscAdd16(index, NscGetDE()); return true;
        case 0x21: index = NscFetch16(); return true;
        case 0x22:
        {
            const unsigned short address = NscFetch16();
            C7010_NSC800_WriteMemory(address, static_cast<Byte>(index));
            C7010_NSC800_WriteMemory(static_cast<unsigned short>(address + 1), static_cast<Byte>(index >> 8));
            return true;
        }
        case 0x23: ++index; return true;
        case 0x24:
        {
            const Byte hi = NscInc8(static_cast<Byte>(index >> 8));
            index = static_cast<unsigned short>((static_cast<unsigned short>(hi) << 8) | (index & 0x00FFu));
            return true;
        }
        case 0x25:
        {
            const Byte hi = NscDec8(static_cast<Byte>(index >> 8));
            index = static_cast<unsigned short>((static_cast<unsigned short>(hi) << 8) | (index & 0x00FFu));
            return true;
        }
        case 0x26:
        {
            const Byte hi = NscFetch8();
            index = static_cast<unsigned short>((static_cast<unsigned short>(hi) << 8) | (index & 0x00FFu));
            return true;
        }
        case 0x29: index = NscAdd16(index, index); return true;
        case 0x2A:
        {
            const unsigned short address = NscFetch16();
            index = static_cast<unsigned short>(C7010_NSC800_ReadMemory(address) |
                (static_cast<unsigned short>(C7010_NSC800_ReadMemory(static_cast<unsigned short>(address + 1))) << 8));
            return true;
        }
        case 0x2B: --index; return true;
        case 0x2C:
        {
            const Byte lo = NscInc8(static_cast<Byte>(index));
            index = static_cast<unsigned short>((index & 0xFF00u) | lo);
            return true;
        }
        case 0x2D:
        {
            const Byte lo = NscDec8(static_cast<Byte>(index));
            index = static_cast<unsigned short>((index & 0xFF00u) | lo);
            return true;
        }
        case 0x2E:
        {
            const Byte lo = NscFetch8();
            index = static_cast<unsigned short>((index & 0xFF00u) | lo);
            return true;
        }
        case 0x34:
        {
            const signed char d = static_cast<signed char>(NscFetch8());
            const unsigned short address = NscIndexedAddress(index, d);
            const Byte value = C7010_NSC800_ReadMemory(address);
            C7010_NSC800_WriteMemory(address, NscInc8(value));
            return true;
        }
        case 0x35:
        {
            const signed char d = static_cast<signed char>(NscFetch8());
            const unsigned short address = NscIndexedAddress(index, d);
            const Byte value = C7010_NSC800_ReadMemory(address);
            C7010_NSC800_WriteMemory(address, NscDec8(value));
            return true;
        }
        case 0x36:
        {
            const signed char d = static_cast<signed char>(NscFetch8());
            const Byte value = NscFetch8();
            C7010_NSC800_WriteMemory(NscIndexedAddress(index, d), value);
            return true;
        }
        case 0x39: index = NscAdd16(index, gNsc.sp); return true;
        case 0xCB: return NscExecuteIndexedCB(prefixPc, index, name);
        case 0xE1: index = NscPop16(); return true;
        case 0xE3:
        {
            const unsigned short stackValue = NscPop16();
            NscPush16(index);
            index = stackValue;
            return true;
        }
        case 0xE5: NscPush16(index); return true;
        case 0xE9: gNsc.pc = index; return true;
        case 0xF9: gNsc.sp = index; return true;
        case 0xDD: // repeated DD/FD prefixes: last prefix wins
            return NscExecuteIndexed(prefixPc, gNsc.ix, "DD");
        case 0xFD:
            return NscExecuteIndexed(prefixPc, gNsc.iy, "FD");
        default:
            break;
        }

        // LD r,r' and ALU r groups: H/L become IXH/IXL (or IYH/IYL), and
        // any (HL) operand becomes (IX/IY+d), consuming one displacement byte.
        if ((op >= 0x40 && op <= 0x7F && op != 0x76) || (op >= 0x80 && op <= 0xBF))
        {
            const unsigned int dst = (op >> 3) & 7u;
            const unsigned int src = op & 7u;
            const bool memoryOperand = (dst == 6u || src == 6u);
            const signed char displacement = memoryOperand ? static_cast<signed char>(NscFetch8()) : 0;

            if (op >= 0x40 && op <= 0x7F)
            {
                const Byte value = NscReadIndexedReg(src, index, memoryOperand, displacement);
                NscWriteIndexedReg(dst, value, index, memoryOperand, displacement);
                return true;
            }

            const Byte value = NscReadIndexedReg(src, index, memoryOperand, displacement);
            switch ((op >> 3) & 7u)
            {
            case 0: gNsc.a = NscAdd8(gNsc.a, value, 0); break;
            case 1: gNsc.a = NscAdd8(gNsc.a, value, (gNsc.f & 1) ? 1 : 0); break;
            case 2: gNsc.a = NscSub8(gNsc.a, value, 0); break;
            case 3: gNsc.a = NscSub8(gNsc.a, value, (gNsc.f & 1) ? 1 : 0); break;
            case 4: gNsc.a = static_cast<Byte>(gNsc.a & value); NscSetLogicFlags(gNsc.a, true); break;
            case 5: gNsc.a = static_cast<Byte>(gNsc.a ^ value); NscSetLogicFlags(gNsc.a, false); break;
            case 6: gNsc.a = static_cast<Byte>(gNsc.a | value); NscSetLogicFlags(gNsc.a, false); break;
            case 7: (void)NscSub8(gNsc.a, value, 0); break;
            }
            return true;
        }

        // For opcodes where DD/FD has no architectural effect, the Z80 treats
        // the prefix as ignored. Handle the small set Chess is already known to
        // use around control flow directly rather than stopping the core.
        switch (op)
        {
        case 0x00: return true; // NOP
        case 0x18: { const auto d=static_cast<signed char>(NscFetch8()); gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); return true; } // 0030AB
        case 0x20: { const signed char d=static_cast<signed char>(NscFetch8()); if (!(gNsc.f&0x40)) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); return true; }
        case 0x28: { const signed char d=static_cast<signed char>(NscFetch8()); if (gNsc.f&0x40) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); return true; }
        case 0x30: { const signed char d=static_cast<signed char>(NscFetch8()); if (!(gNsc.f&1)) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); return true; }
        case 0x38: { const signed char d=static_cast<signed char>(NscFetch8()); if (gNsc.f&1) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); return true; }
        case 0xC3: gNsc.pc=NscFetch16(); return true;
        case 0xC9: gNsc.pc=NscPop16(); return true;
        case 0xCD: { const unsigned short a=NscFetch16(); NscPush16(gNsc.pc); gNsc.pc=a; return true; }
        case 0xD3: { const Byte p=NscFetch8(); C7010_NSC800_Out(p,gNsc.a); return true; }
        case 0xDB: { const Byte p=NscFetch8(); gNsc.a=C7010_NSC800_In(p); return true; }
        case 0xED: return NscExecuteED(prefixPc);
        case 0xF3: gNsc.iff1=false; gNsc.iff2=false; return true;
        case 0xFB: gNsc.iff1=true; gNsc.iff2=true; return true;
        default:
            std::printf("O2EM-NG: NSC800 core stopped: unsupported %s%02X at PC=%04X (%s=%04X)\n",
                name, op, prefixPc, name + 1, index);
            gNsc.stopped = true;
            return false;
        }
    }

    bool NscExecuteED(unsigned short prefixPc)
    {
        const Byte op2 = NscFetch8();

        if (gNsc.instructions < 96)
            C7010_TRACE_PRINT("O2EM-NG: NSC800 ED-prefix PC=%04X OP=ED%02X\n", prefixPc, op2);

        auto read16 = [](unsigned short address) -> unsigned short
        {
            const Byte lo = C7010_NSC800_ReadMemory(address);
            const Byte hi = C7010_NSC800_ReadMemory(static_cast<unsigned short>(address + 1));
            return static_cast<unsigned short>(lo | (static_cast<unsigned short>(hi) << 8));
        };
        auto write16 = [](unsigned short address, unsigned short value)
        {
            C7010_NSC800_WriteMemory(address, static_cast<Byte>(value));
            C7010_NSC800_WriteMemory(static_cast<unsigned short>(address + 1), static_cast<Byte>(value >> 8));
        };
        auto getPair = [](unsigned int pair) -> unsigned short
        {
            switch (pair & 3u)
            {
            case 0: return NscGetBC();
            case 1: return NscGetDE();
            case 2: return gNsc.hl;
            default:return gNsc.sp;
            }
        };
        auto setPair = [](unsigned int pair, unsigned short value)
        {
            switch (pair & 3u)
            {
            case 0: NscSetBC(value); break;
            case 1: NscSetDE(value); break;
            case 2: gNsc.hl = value; break;
            default:gNsc.sp = value; break;
            }
        };
        auto adcSbcHl = [](unsigned short rhs, bool subtract)
        {
            const unsigned short lhs = gNsc.hl;
            const unsigned int carry = (gNsc.f & 1) ? 1u : 0u;
            const unsigned int wide = subtract
                ? static_cast<unsigned int>(lhs) - static_cast<unsigned int>(rhs) - carry
                : static_cast<unsigned int>(lhs) + static_cast<unsigned int>(rhs) + carry;
            const unsigned short result = static_cast<unsigned short>(wide);
            Byte f = static_cast<Byte>((result >> 8) & 0xA8);
            if (result == 0) f |= 0x40;
            if (subtract)
            {
                f |= 0x02;
                if ((lhs & 0x0FFFu) < ((rhs & 0x0FFFu) + carry)) f |= 0x10;
                if ((((lhs ^ rhs) & (lhs ^ result)) & 0x8000u) != 0) f |= 0x04;
                if (static_cast<unsigned int>(lhs) < static_cast<unsigned int>(rhs) + carry) f |= 0x01;
            }
            else
            {
                if (((lhs & 0x0FFFu) + (rhs & 0x0FFFu) + carry) > 0x0FFFu) f |= 0x10;
                if (((~(lhs ^ rhs) & (lhs ^ result)) & 0x8000u) != 0) f |= 0x04;
                if (wide > 0xFFFFu) f |= 0x01;
            }
            gNsc.hl = result;
            gNsc.f = f;
        };
        auto setInFlags = [](Byte value)
        {
            Byte f = static_cast<Byte>((gNsc.f & 0x01) | (value & 0xA8));
            if (value == 0) f |= 0x40;
            if (NscEvenParity(value)) f |= 0x04;
            gNsc.f = f;
        };
        auto doLdi = [](int direction)
        {
            unsigned short bc = NscGetBC();
            unsigned short de = NscGetDE();
            const Byte value = C7010_NSC800_ReadMemory(gNsc.hl);
            C7010_NSC800_WriteMemory(de, value);
            gNsc.hl = static_cast<unsigned short>(gNsc.hl + direction);
            de = static_cast<unsigned short>(de + direction);
            bc = static_cast<unsigned short>(bc - 1);
            NscSetDE(de);
            NscSetBC(bc);
            Byte f = static_cast<Byte>(gNsc.f & (0x80 | 0x40 | 0x01));
            if (bc != 0) f |= 0x04;
            const Byte sum = static_cast<Byte>(gNsc.a + value);
            f |= static_cast<Byte>(sum & 0x08);
            if (sum & 0x02) f |= 0x20;
            gNsc.f = f;
        };
        auto doCpi = [](int direction)
        {
            unsigned short bc = NscGetBC();
            const Byte value = C7010_NSC800_ReadMemory(gNsc.hl);
            const Byte oldC = static_cast<Byte>(gNsc.f & 1);
            (void)NscSub8(gNsc.a, value, 0);
            gNsc.hl = static_cast<unsigned short>(gNsc.hl + direction);
            bc = static_cast<unsigned short>(bc - 1);
            NscSetBC(bc);
            gNsc.f = static_cast<Byte>((gNsc.f & ~0x05) | oldC | (bc != 0 ? 0x04 : 0));
        };

        // IN r,(C) / OUT (C),r
        if ((op2 & 0xC7) == 0x40)
        {
            const unsigned int r = (op2 >> 3) & 7u;
            const Byte value = C7010_NSC800_In(gNsc.c);
            if (r != 6u) NscWriteReg(r, value); // ED70 discards the input value.
            setInFlags(value);
            return true;
        }
        if ((op2 & 0xC7) == 0x41)
        {
            const unsigned int r = (op2 >> 3) & 7u;
            C7010_NSC800_Out(gNsc.c, r == 6u ? 0 : NscReadReg(r));
            return true;
        }

        // SBC/ADC HL,rr
        if ((op2 & 0xCF) == 0x42)
        {
            adcSbcHl(getPair((op2 >> 4) & 3u), true);
            return true;
        }
        if ((op2 & 0xCF) == 0x4A)
        {
            adcSbcHl(getPair((op2 >> 4) & 3u), false);
            return true;
        }

        // LD (nn),rr -- ED43/53/63/73. ED73 is the first instruction that
        // stopped Chess after the 0030H IX/IY fix.
        if ((op2 & 0xCF) == 0x43)
        {
            const unsigned short address = NscFetch16();
            write16(address, getPair((op2 >> 4) & 3u));
            return true;
        }
        // LD rr,(nn) -- ED4B/5B/6B/7B.
        if ((op2 & 0xCF) == 0x4B)
        {
            const unsigned short address = NscFetch16();
            setPair((op2 >> 4) & 3u, read16(address));
            return true;
        }

        switch (op2)
        {
        // NEG aliases.
        case 0x44: case 0x4C: case 0x54: case 0x5C:
        case 0x64: case 0x6C: case 0x74: case 0x7C:
            gNsc.a = NscSub8(0, gNsc.a, 0);
            return true;

        // RETN aliases and RETI.
        case 0x45: case 0x55: case 0x65: case 0x75:
            gNsc.pc = NscPop16();
            gNsc.iff1 = gNsc.iff2;
            return true;
        case 0x4D: case 0x5D: case 0x6D: case 0x7D:
            gNsc.pc = NscPop16();
            gNsc.iff1 = gNsc.iff2;
            return true;

        // Interrupt modes and their documented aliases.
        case 0x46: case 0x4E: case 0x66: case 0x6E:
            gNsc.interruptMode = 0;
            return true;
        case 0x56: case 0x76:
            gNsc.interruptMode = 1;
            return true;
        case 0x5E: case 0x7E:
            gNsc.interruptMode = 2;
            return true;

        case 0x47: gNsc.i = gNsc.a; return true; // LD I,A
        case 0x4F: gNsc.r = gNsc.a; return true; // LD R,A
        case 0x57: // LD A,I
            gNsc.a = gNsc.i;
            gNsc.f = static_cast<Byte>((gNsc.f & 0x01) | (gNsc.a & 0xA8) |
                (gNsc.a == 0 ? 0x40 : 0) | (gNsc.iff2 ? 0x04 : 0));
            return true;
        case 0x5F: // LD A,R
            gNsc.a = gNsc.r;
            gNsc.f = static_cast<Byte>((gNsc.f & 0x01) | (gNsc.a & 0xA8) |
                (gNsc.a == 0 ? 0x40 : 0) | (gNsc.iff2 ? 0x04 : 0));
            return true;

        case 0x67: // RRD
        {
            const Byte mem = C7010_NSC800_ReadMemory(gNsc.hl);
            const Byte newMem = static_cast<Byte>((gNsc.a << 4) | (mem >> 4));
            gNsc.a = static_cast<Byte>((gNsc.a & 0xF0) | (mem & 0x0F));
            C7010_NSC800_WriteMemory(gNsc.hl, newMem);
            setInFlags(gNsc.a);
            return true;
        }
        case 0x6F: // RLD
        {
            const Byte mem = C7010_NSC800_ReadMemory(gNsc.hl);
            const Byte newMem = static_cast<Byte>((mem << 4) | (gNsc.a & 0x0F));
            gNsc.a = static_cast<Byte>((gNsc.a & 0xF0) | (mem >> 4));
            C7010_NSC800_WriteMemory(gNsc.hl, newMem);
            setInFlags(gNsc.a);
            return true;
        }

        case 0xA0: doLdi(+1); return true; // LDI
        case 0xA8: doLdi(-1); return true; // LDD
        case 0xA1: doCpi(+1); return true; // CPI
        case 0xA9: doCpi(-1); return true; // CPD

        case 0xA2: case 0xAA: // INI / IND
        {
            const int dir = op2 == 0xA2 ? +1 : -1;
            const Byte value = C7010_NSC800_In(gNsc.c);
            C7010_NSC800_WriteMemory(gNsc.hl, value);
            gNsc.hl = static_cast<unsigned short>(gNsc.hl + dir);
            gNsc.b = static_cast<Byte>(gNsc.b - 1);
            gNsc.f = static_cast<Byte>((gNsc.b & 0xA8) | (gNsc.b == 0 ? 0x40 : 0) | 0x02);
            return true;
        }
        case 0xA3: case 0xAB: // OUTI / OUTD
        {
            const int dir = op2 == 0xA3 ? +1 : -1;
            C7010_NSC800_Out(gNsc.c, C7010_NSC800_ReadMemory(gNsc.hl));
            gNsc.hl = static_cast<unsigned short>(gNsc.hl + dir);
            gNsc.b = static_cast<Byte>(gNsc.b - 1);
            gNsc.f = static_cast<Byte>((gNsc.b & 0xA8) | (gNsc.b == 0 ? 0x40 : 0) | 0x02);
            return true;
        }

        // Repeating block instructions. Execute their architecturally visible
        // repeat here; cycle accounting remains a later timing refinement.
        case 0xB0: // LDIR
            do { doLdi(+1); } while (NscGetBC() != 0);
            return true;
        case 0xB8: // LDDR
            do { doLdi(-1); } while (NscGetBC() != 0);
            return true;
        case 0xB1: // CPIR
            do { doCpi(+1); } while (NscGetBC() != 0 && (gNsc.f & 0x40) == 0);
            return true;
        case 0xB9: // CPDR
            do { doCpi(-1); } while (NscGetBC() != 0 && (gNsc.f & 0x40) == 0);
            return true;
        case 0xB2: case 0xBA: // INIR / INDR
        {
            const int dir = op2 == 0xB2 ? +1 : -1;
            do
            {
                const Byte value = C7010_NSC800_In(gNsc.c);
                C7010_NSC800_WriteMemory(gNsc.hl, value);
                gNsc.hl = static_cast<unsigned short>(gNsc.hl + dir);
                gNsc.b = static_cast<Byte>(gNsc.b - 1);
            } while (gNsc.b != 0);
            gNsc.f = 0x42; // Z,N after the repeat terminates.
            return true;
        }
        case 0xB3: case 0xBB: // OTIR / OTDR
        {
            const int dir = op2 == 0xB3 ? +1 : -1;
            do
            {
                C7010_NSC800_Out(gNsc.c, C7010_NSC800_ReadMemory(gNsc.hl));
                gNsc.hl = static_cast<unsigned short>(gNsc.hl + dir);
                gNsc.b = static_cast<Byte>(gNsc.b - 1);
            } while (gNsc.b != 0);
            gNsc.f = 0x42;
            return true;
        }

        default:
            std::printf("O2EM-NG: NSC800 diagnostic core stopped: unsupported ED%02X at PC=%04X\n", op2, prefixPc);
            C7010_TRACE_PRINT("O2EM-NG: 0030I ED trace captured; send this log for the next opcode/timing step.\n");
            gNsc.stopped = true;
            return false;
        }
    }

    void NscResetExecution()
    {
        gNsc = Nsc800DiagState{};
        gNsc.initialized = true;
        C7010_TRACE_PRINT("O2EM-NG: C7010 NSC800 execution started at PC=0000 SP=FFFF\n");
    }
}

static void C7010_ExecuteOneStep()
{
    if (!gEnabled || !gFirmwareLoaded || gCpuReset)
        return;

    if (!gNsc.initialized || gExecutionNeedsReset)
    {
        NscResetExecution();
        gExecutionNeedsReset = false;
    }
    if (gNsc.stopped)
        return;

    const unsigned short pc0 = gNsc.pc;
    gNscPcForTrace = pc0;
    const Byte op = NscFetch8();
    // Firmware checkpoints: possible calls/jump to the 63 response routine.
    // The register/stack snapshot identifies the path without changing execution.
    if ((pc0 == 0x14A7 || pc0 == 0x1A11 || pc0 == 0x1AAB || pc0 == 0x1759)
        && gDecisionTraceLines < 32) {
        ++gDecisionTraceLines;
        const unsigned int stackWord = C7010_NSC800_ReadMemory(gNsc.sp) |
            (static_cast<unsigned int>(C7010_NSC800_ReadMemory(static_cast<unsigned short>(gNsc.sp+1))) << 8);
        C7010_TRACE_PRINT("O2EM-NG: C7010 0030T DECISION PC=%04X OP=%02X A=%02X F=%02X SP=%04X stack=%04X FF58=%02X FF59=%02X FF5A=%02X in=%02X out=%02X\n",
            pc0, op, gNsc.a, gNsc.f, gNsc.sp, stackWord,
            gRam[0x758], gRam[0x759], gRam[0x75A], gToNsc800, gTo8048);
    }

    // 0030U: record each validation decision before its conditional branch.
    // Raw RAM is reported without assigning undocumented side/piece meanings.
    const bool validationPoint = pc0 == 0x1426 || pc0 == 0x1435 ||
        pc0 == 0x1441 || pc0 == 0x1449 || pc0 == 0x1453 ||
        pc0 == 0x145A || pc0 == 0x145E || pc0 == 0x1499 ||
        pc0 == 0x149D || pc0 == 0x14A1 || pc0 == 0x14A7;
    if (validationPoint && gValidationLines < 128) {
        ++gValidationLines;
        C7010_TRACE_PRINT("O2EM-NG: C7010 0030U CHECK PC=%04X OP=%02X A=%02X F=%02X Z=%u C=%u BC=%02X%02X DE=%02X%02X HL=%04X memHL=%02X FF37=%02X FF47=%02X FF77=%02X FF78=%02X\n",
            pc0, op, gNsc.a, gNsc.f, (gNsc.f & 0x40) ? 1u : 0u,
            (gNsc.f & 1) ? 1u : 0u, gNsc.b, gNsc.c, gNsc.d, gNsc.e,
            gNsc.hl, C7010_NSC800_ReadMemory(gNsc.hl),
            gRam[0x737], gRam[0x747], gRam[0x777], gRam[0x778]);
        if (pc0 == 0x1426 || pc0 == 0x14A7) {
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030U INPUTRAM FF56-FF63:");
            for (unsigned int a=0x756; a<=0x763; ++a) C7010_TRACE_PRINT(" %02X", gRam[a]);
            C7010_TRACE_PRINT("\n");
            for (unsigned int base : {0xFA00u, 0xFB00u}) {
                C7010_TRACE_PRINT("O2EM-NG: C7010 0030U BOARDRAM %04X-%04X:", base, base+63);
                for (unsigned int n=0; n<64; ++n)
                    C7010_TRACE_PRINT(" %02X", C7010_NSC800_ReadMemory(static_cast<unsigned short>(base+n)));
                C7010_TRACE_PRINT("\n");
            }
        }
        if (gValidationLines == 128)
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030U CHECK limit reached; reset for a new capture.\n");
    }
    // Keep startup tracing bounded: enough to prove execution and diagnose the
    // first unsupported instruction without flooding the console every frame.
    const bool repeatedDjnz = (op == 0x10 && gNsc.djnzPc == pc0 && gNsc.djnzIterations != 0);
    const bool insideActiveDjnzLoop =
        (gNsc.djnzIterations != 0 && gNsc.djnzPc != 0xFFFF && gNsc.djnzTarget != 0xFFFF &&
         pc0 >= gNsc.djnzTarget && pc0 < gNsc.djnzPc);
    if (gNsc.instructions < 128 && !repeatedDjnz && !insideActiveDjnzLoop)
        C7010_TRACE_PRINT("O2EM-NG: NSC800 PC=%04X OP=%02X A=%02X BC=%02X%02X DE=%02X%02X HL=%04X SP=%04X IM=%u IFF=%u\n",
            pc0, op, gNsc.a, gNsc.b, gNsc.c, gNsc.d, gNsc.e, gNsc.hl, gNsc.sp,
            static_cast<unsigned>(gNsc.interruptMode), gNsc.iff1 ? 1u : 0u);

    if (pc0 >= 0x1120 && pc0 <= 0x1130 && gNscIoTraceLines < kNscIoTraceLimit)
    {
        C7010_TRACE_PRINT(
            "O2EM-NG: C7010 NSCPC PC=%04X OP=%02X A=%02X F=%02X BC=%02X%02X DE=%02X%02X HL=%04X SP=%04X latchIN=%02X latchOUT=%02X\n",
            pc0, op, gNsc.a, gNsc.f, gNsc.b, gNsc.c, gNsc.d, gNsc.e,
            gNsc.hl, gNsc.sp, gToNsc800, gTo8048);
    }

    ++gNsc.instructions;

    switch (op)
    {
    case 0x00: break;                                      // NOP
    case 0x01: { auto v=NscFetch16(); gNsc.b=Byte(v>>8); gNsc.c=Byte(v); break; }
    case 0x02: C7010_NSC800_WriteMemory(NscGetBC(), gNsc.a); break;
    case 0x03: NscSetBC(static_cast<unsigned short>(NscGetBC() + 1)); break;
    case 0x04: gNsc.b=NscInc8(gNsc.b); break;
    case 0x05: gNsc.b=NscDec8(gNsc.b); break;
    case 0x06: gNsc.b=NscFetch8(); break;                 // LD B,n
    case 0x07: { const Byte c=static_cast<Byte>(gNsc.a>>7); gNsc.a=static_cast<Byte>((gNsc.a<<1)|c); gNsc.f=static_cast<Byte>((gNsc.f&0xC4)|(gNsc.a&0x28)|c); break; } // RLCA
    case 0x08: { const Byte a=gNsc.a,f=gNsc.f; gNsc.a=gNsc.a2; gNsc.f=gNsc.f2; gNsc.a2=a; gNsc.f2=f; break; }
    case 0x09: gNsc.hl=NscAdd16(gNsc.hl,NscGetBC()); break;
    case 0x0A: gNsc.a=C7010_NSC800_ReadMemory(NscGetBC()); break;
    case 0x0B: NscSetBC(static_cast<unsigned short>(NscGetBC() - 1)); break;
    case 0x0C: gNsc.c=NscInc8(gNsc.c); break;
    case 0x0E: gNsc.c=NscFetch8(); break;                 // LD C,n
    case 0x0F: { const Byte c=static_cast<Byte>(gNsc.a&1); gNsc.a=static_cast<Byte>((gNsc.a>>1)|(c<<7)); gNsc.f=static_cast<Byte>((gNsc.f&0xC4)|(gNsc.a&0x28)|c); break; } // RRCA
    case 0x0D: gNsc.c=NscDec8(gNsc.c); break;             // DEC C
    case 0x10:                                             // DJNZ e
    {
        const signed char displacement = static_cast<signed char>(NscFetch8());
        const Byte oldB = gNsc.b;
        gNsc.b = static_cast<Byte>(gNsc.b - 1);

        if (gNsc.djnzPc != pc0)
        {
            gNsc.djnzPc = pc0;
            gNsc.djnzTarget = static_cast<unsigned short>(gNsc.pc + displacement);
            gNsc.djnzIterations = 0;
        }
        ++gNsc.djnzIterations;

        if (gNsc.b != 0)
        {
            const unsigned short target = gNsc.djnzTarget;
            if (gNsc.djnzIterations == 1)
                C7010_TRACE_PRINT("O2EM-NG: NSC800 DJNZ loop PC=%04X B=%02X->%02X target=%04X\n",
                    pc0, oldB, gNsc.b, target);
            gNsc.pc = target;
        }
        else
        {
            C7010_TRACE_PRINT("O2EM-NG: NSC800 DJNZ loop completed at PC=%04X after %u iterations (B=%02X)\n",
                pc0, gNsc.djnzIterations, gNsc.b);
            gNsc.djnzPc = 0xFFFF;
            gNsc.djnzTarget = 0xFFFF;
            gNsc.djnzIterations = 0;
        }
        break;
    }
    case 0x11: { auto v=NscFetch16(); gNsc.d=Byte(v>>8); gNsc.e=Byte(v); break; }
    case 0x12: C7010_NSC800_WriteMemory(NscGetDE(), gNsc.a); break;
    case 0x13: NscSetDE(static_cast<unsigned short>(NscGetDE() + 1)); break;
    case 0x14: gNsc.d=NscInc8(gNsc.d); break;
    case 0x15: gNsc.d=NscDec8(gNsc.d); break;
    case 0x16: gNsc.d=NscFetch8(); break;
    case 0x17: { const Byte oldC=static_cast<Byte>(gNsc.f&1), c=static_cast<Byte>(gNsc.a>>7); gNsc.a=static_cast<Byte>((gNsc.a<<1)|oldC); gNsc.f=static_cast<Byte>((gNsc.f&0xC4)|(gNsc.a&0x28)|c); break; } // RLA
    case 0x18: { const auto d=static_cast<signed char>(NscFetch8()); gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); break; } // 0030AB: relative to PC after displacement fetch
    case 0x19: gNsc.hl=NscAdd16(gNsc.hl,NscGetDE()); break;
    case 0x1A: gNsc.a=C7010_NSC800_ReadMemory(NscGetDE()); break;
    case 0x1B: NscSetDE(static_cast<unsigned short>(NscGetDE() - 1)); break;
    case 0x1C: gNsc.e=NscInc8(gNsc.e); break;
    case 0x1D: gNsc.e=NscDec8(gNsc.e); break;
    case 0x1E: gNsc.e=NscFetch8(); break;
    case 0x1F: { const Byte oldC=static_cast<Byte>(gNsc.f&1), c=static_cast<Byte>(gNsc.a&1); gNsc.a=static_cast<Byte>((gNsc.a>>1)|(oldC<<7)); gNsc.f=static_cast<Byte>((gNsc.f&0xC4)|(gNsc.a&0x28)|c); break; } // RRA
    case 0x20: { auto d=static_cast<signed char>(NscFetch8()); if (!(gNsc.f&0x40)) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); break; }
    case 0x21: gNsc.hl=NscFetch16(); break;                // LD HL,nn
    case 0x22: { auto a=NscFetch16(); C7010_NSC800_WriteMemory(a,Byte(gNsc.hl)); C7010_NSC800_WriteMemory(a+1,Byte(gNsc.hl>>8)); break; }
    case 0x23: ++gNsc.hl; break;
    case 0x24: { Byte h=NscInc8(static_cast<Byte>(gNsc.hl>>8)); gNsc.hl=static_cast<unsigned short>((h<<8)|(gNsc.hl&0xFF)); break; }
    case 0x25: { Byte h=NscDec8(static_cast<Byte>(gNsc.hl>>8)); gNsc.hl=static_cast<unsigned short>((h<<8)|(gNsc.hl&0xFF)); break; }
    case 0x26: { const Byte h=NscFetch8(); gNsc.hl=static_cast<unsigned short>((static_cast<unsigned short>(h)<<8)|(gNsc.hl&0xFF)); break; }
    case 0x27: // DAA
    {
        const Byte oldA=gNsc.a, oldF=gNsc.f;
        Byte correction=0; bool carry=(oldF&1)!=0;
        if ((oldF&0x10) || (!(oldF&0x02) && (oldA&0x0F)>9)) correction|=0x06;
        if (carry || (!(oldF&0x02) && oldA>0x99)) { correction|=0x60; carry=true; }
        gNsc.a = (oldF&0x02) ? static_cast<Byte>(oldA-correction) : static_cast<Byte>(oldA+correction);
        Byte f=static_cast<Byte>((oldF&0x02)|(gNsc.a&0xA8));
        if (gNsc.a == 0) f |= 0x40;
        if (NscEvenParity(gNsc.a)) f |= 0x04;
        if (((oldA ^ gNsc.a ^ correction) & 0x10) != 0) f |= 0x10;
        if (carry) f |= 0x01;
        gNsc.f=f; break;
    }
    case 0x28: { auto d=static_cast<signed char>(NscFetch8()); if (gNsc.f&0x40) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); break; }
    case 0x29: gNsc.hl=NscAdd16(gNsc.hl,gNsc.hl); break;
    case 0x2A: { auto a=NscFetch16(); gNsc.hl=static_cast<unsigned short>(C7010_NSC800_ReadMemory(a)|(C7010_NSC800_ReadMemory(a+1)<<8)); break; }
    case 0x2B: --gNsc.hl; break;
    case 0x2C:                                             // INC L
    {
        const Byte l = NscInc8(static_cast<Byte>(gNsc.hl));
        gNsc.hl = static_cast<unsigned short>((gNsc.hl & 0xFF00u) | l);
        break;
    }
    case 0x2D: { const Byte l=NscDec8(static_cast<Byte>(gNsc.hl)); gNsc.hl=static_cast<unsigned short>((gNsc.hl&0xFF00u)|l); break; }
    case 0x2E: { const Byte l=NscFetch8(); gNsc.hl=static_cast<unsigned short>((gNsc.hl&0xFF00u)|l); break; }
    case 0x2F: gNsc.a=static_cast<Byte>(~gNsc.a); gNsc.f=static_cast<Byte>((gNsc.f & 0xC5) | 0x12 | (gNsc.a & 0x28)); break;
    case 0x30: { auto d=static_cast<signed char>(NscFetch8()); if (!(gNsc.f&1)) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); break; }
    case 0x31: gNsc.sp=NscFetch16(); break;                // LD SP,nn
    case 0x32: { auto a=NscFetch16(); C7010_NSC800_WriteMemory(a,gNsc.a); break; }
    case 0x33: ++gNsc.sp; break;
    case 0x34: { Byte v=C7010_NSC800_ReadMemory(gNsc.hl); C7010_NSC800_WriteMemory(gNsc.hl,NscInc8(v)); break; }
    case 0x35: { Byte v=C7010_NSC800_ReadMemory(gNsc.hl); C7010_NSC800_WriteMemory(gNsc.hl,NscDec8(v)); break; }
    case 0x36: { auto n=NscFetch8(); C7010_NSC800_WriteMemory(gNsc.hl,n); break; }
    case 0x37: gNsc.f=static_cast<Byte>((gNsc.f & 0xC4) | (gNsc.a & 0x28) | 0x01); break;
    case 0x38: { auto d=static_cast<signed char>(NscFetch8()); if (gNsc.f&1) gNsc.pc=static_cast<unsigned short>(gNsc.pc+d); break; }
    case 0x39: gNsc.hl=NscAdd16(gNsc.hl,gNsc.sp); break;
    case 0x3A: { auto a=NscFetch16(); gNsc.a=C7010_NSC800_ReadMemory(a); break; }
    case 0x3B: --gNsc.sp; break;
    case 0x3C: gNsc.a=NscInc8(gNsc.a); break;             // INC A
    case 0x3D: gNsc.a=NscDec8(gNsc.a); break;
    case 0x3E: gNsc.a=NscFetch8(); break;                 // LD A,n
    case 0x3F: { const Byte oldCarry=static_cast<Byte>(gNsc.f&1); gNsc.f=static_cast<Byte>((gNsc.f&0xC4)|(gNsc.a&0x28)|(oldCarry?0x10:0)|(oldCarry?0:1)); break; }
    case 0x76: gNsc.stopped=true; std::printf("O2EM-NG: NSC800 HALT at PC=%04X\n",pc0); break;
    case 0x77: C7010_NSC800_WriteMemory(gNsc.hl,gNsc.a); break;
    case 0x7E: gNsc.a=C7010_NSC800_ReadMemory(gNsc.hl); break;
    case 0xA7: NscAndA(); break;                          // AND A
    case 0xAF: gNsc.a=0; NscSetLogicFlags(gNsc.a,false); break; // XOR A
    case 0xC1: { auto v=NscPop16(); NscSetBC(v); break; }
    case 0xC4: NscConditionalCall(0); break;
    case 0xC7: NscRst(0x00); break;
    case 0xC8: NscConditionalRet(1); break;
    case 0xCB: NscExecuteCB(pc0); break;
    case 0xCC: NscConditionalCall(1); break;
    case 0xCF: NscRst(0x08); break;
    case 0xC5: NscPush16(NscGetBC()); break;
    case 0xC6: gNsc.a=NscAdd8(gNsc.a,NscFetch8(),0); break;
    case 0xCE: gNsc.a=NscAdd8(gNsc.a,NscFetch8(),(gNsc.f&1)?1:0); break;
    case 0xD0: NscConditionalRet(2); break;
    case 0xD1: { auto v=NscPop16(); NscSetDE(v); break; }
    case 0xD2: NscConditionalJump(2); break;
    case 0xD4: NscConditionalCall(2); break;
    case 0xD7: NscRst(0x10); break;
    case 0xD8: NscConditionalRet(3); break;
    case 0xDA: NscConditionalJump(3); break;
    case 0xDC: NscConditionalCall(3); break;
    case 0xDF: NscRst(0x18); break;
    case 0xD5: NscPush16(NscGetDE()); break;
    case 0xD6: gNsc.a=NscSub8(gNsc.a,NscFetch8(),0); break;
    case 0xDE: gNsc.a=NscSub8(gNsc.a,NscFetch8(),(gNsc.f&1)?1:0); break;
    case 0xE6: gNsc.a=static_cast<Byte>(gNsc.a & NscFetch8()); NscSetLogicFlags(gNsc.a,true); break;
    case 0xEE: gNsc.a=static_cast<Byte>(gNsc.a ^ NscFetch8()); NscSetLogicFlags(gNsc.a,false); break;
    case 0xF6: gNsc.a=static_cast<Byte>(gNsc.a | NscFetch8()); NscSetLogicFlags(gNsc.a,false); break;
    case 0xFE: (void)NscSub8(gNsc.a,NscFetch8(),0); break;
    case 0xC0: NscConditionalRet(0); break;
    case 0xC2: NscConditionalJump(0); break;
    case 0xC3: gNsc.pc=NscFetch16(); break;               // JP nn
    case 0xC9: gNsc.pc=NscPop16(); break;                 // RET
    case 0xCA: NscConditionalJump(1); break;
    case 0xCD: { auto a=NscFetch16(); NscPush16(gNsc.pc); gNsc.pc=a; break; }
    case 0xD3: { Byte p=NscFetch8(); C7010_NSC800_Out(p,gNsc.a); break; }
    case 0xDB: { Byte p=NscFetch8(); gNsc.a=C7010_NSC800_In(p); break; } // IN A,(n): flags are unchanged
    case 0xDD: NscExecuteIndexed(pc0, gNsc.ix, "DD"); break; // IX prefix
    case 0xFD: NscExecuteIndexed(pc0, gNsc.iy, "FD"); break; // IY prefix
    case 0xD9: {
        const Byte h=static_cast<Byte>(gNsc.hl>>8), l=static_cast<Byte>(gNsc.hl);
        const Byte b=gNsc.b,c=gNsc.c,d=gNsc.d,e=gNsc.e;
        gNsc.b=gNsc.b2; gNsc.c=gNsc.c2; gNsc.d=gNsc.d2; gNsc.e=gNsc.e2;
        gNsc.hl=static_cast<unsigned short>((gNsc.h2<<8)|gNsc.l2);
        gNsc.b2=b; gNsc.c2=c; gNsc.d2=d; gNsc.e2=e; gNsc.h2=h; gNsc.l2=l; break;
    }
    case 0xE0: NscConditionalRet(4); break;
    case 0xE1: gNsc.hl=NscPop16(); break;
    case 0xE2: NscConditionalJump(4); break;
    case 0xE4: NscConditionalCall(4); break;
    case 0xE7: NscRst(0x20); break;
    case 0xE8: NscConditionalRet(5); break;
    case 0xEA: NscConditionalJump(5); break;
    case 0xEC: NscConditionalCall(5); break;
    case 0xEF: NscRst(0x28); break;
    case 0xE3: { auto v=NscPop16(); NscPush16(gNsc.hl); gNsc.hl=v; break; }
    case 0xE5: NscPush16(gNsc.hl); break;
    case 0xE9: gNsc.pc=gNsc.hl; break;
    case 0xEB: { auto v=NscGetDE(); NscSetDE(gNsc.hl); gNsc.hl=v; break; }
    case 0xED: NscExecuteED(pc0); break;                  // Extended instruction prefix
    case 0xF0: NscConditionalRet(6); break;
    case 0xF2: NscConditionalJump(6); break;
    case 0xF3:                                           // DI
        gNsc.iff1 = false;
        gNsc.iff2 = false;
        break;
    case 0xF4: NscConditionalCall(6); break;
    case 0xF5: NscPush16(static_cast<unsigned short>((gNsc.a<<8)|gNsc.f)); break;
    case 0xF7: NscRst(0x30); break;
    case 0xF8: NscConditionalRet(7); break;
    case 0xF1: { auto v=NscPop16(); gNsc.a=Byte(v>>8); gNsc.f=Byte(v); break; }
    case 0xF9: gNsc.sp=gNsc.hl; break;
    case 0xFA: NscConditionalJump(7); break;
    case 0xFC: NscConditionalCall(7); break;
    case 0xFF: NscRst(0x38); break;
    case 0xFB:                                           // EI
        // Phase 3 records interrupt-enable state. Actual IRQ acceptance and
        // the Z80 one-instruction EI delay are wired in a later timing step.
        gNsc.iff1 = true;
        gNsc.iff2 = true;
        break;
    default:
        if (!NscExecuteRegisterMatrix(op))
        {
            std::printf("O2EM-NG: NSC800 core stopped: unsupported OP=%02X at PC=%04X\n", op, pc0);
            C7010_TRACE_PRINT("O2EM-NG: 0030E trace captured; this is now an execution-engine checkpoint, not a one-opcode phase.\n");
            gNsc.stopped = true;
        }
        break;
    }
}


// 0030S: distribute the existing approximate 10000-instruction frame budget
// across 8048 machine cycles. This is not yet exact NSC800 T-state timing.
void C7010_Run8048Cycles(unsigned int cycles, unsigned int frameCycles)
{
    if (!gEnabled || !gFirmwareLoaded || gCpuReset || frameCycles == 0) {
        gInterleaveCredit = 0;
        return;
    }
    gInterleaveCredit += static_cast<unsigned long>(cycles) * 10000UL;
    while (gInterleaveCredit >= frameCycles) {
        gInterleaveCredit -= frameCycles;
        C7010_ExecuteOneStep();
        if (gNsc.stopped) {
            gInterleaveCredit = 0;
            break;
        }
    }
}

void C7010_RunDiagnosticStep()
{
    // Frame-level trace bookkeeping only; execution now happens in cpu_exec.
    if (gMoveSlices) {
        const unsigned int slice = 301u-gMoveSlices;
        if (slice <= 5 || slice % 25 == 0)
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030Q SUMMARY slice=%u NSCPC=%04X stopped=%u reset=%u in=%02X out=%02X writes=%lu/%lu reads=%lu/%lu repeated=%lu/%lu\n",
                slice, gNsc.pc, gNsc.stopped ? 1u : 0u, gCpuReset ? 1u : 0u,
                gToNsc800, gTo8048, gMoveWrites[0], gMoveWrites[1],
                gMoveReads[0], gMoveReads[1], gMovePolls[0], gMovePolls[1]);
        if (--gMoveSlices == 0)
            C7010_TRACE_PRINT("O2EM-NG: C7010 0030Q COMM END - 300 slices complete.\n");
    }
}
