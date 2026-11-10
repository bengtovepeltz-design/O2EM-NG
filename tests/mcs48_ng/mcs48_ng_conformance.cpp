// O2EM-NG MCS48-NG standalone conformance suite.
//
// Tests hardware semantics derived from Intel MCS-48 documentation and MAME
// 0.289. Every test uses an owned Mcs48Cpu and a deterministic stub bus.

#include <cstdio>
#include <cstring>
#include <vector>

#include "cpu/mcs48_ng.h"
#include "mcs48_ng_stub_bus.h"
#include "src/mcs48/trace_record.h"

using o2em::ng::Mcs48Cpu;
using o2em::ng::Mcs48State;
using o2em::ng::StepResult;
using mcs48_ng_test::TestBus;

static int g_tests = 0;
static int g_failures = 0;

static void Check(bool condition, const char* name, const char* detail = "") {
    ++g_tests;
    if (condition)
        return;
    ++g_failures;
    std::printf("FAIL %-40s %s\n", name, detail);
}

static void CheckEq(long long actual, long long expected, const char* name) {
    char detail[96];
    std::snprintf(detail, sizeof(detail), "actual=%lld expected=%lld", actual, expected);
    Check(actual == expected, name, detail);
}

struct Fixture {
    TestBus bus;
    Mcs48Cpu cpu;

    Fixture() : cpu(bus) {}
    void Reset() {
        bus.Reset();
        cpu.PowerOn();
    }
    Mcs48State& S() { return cpu.MutableState(); }
    const Mcs48State& S() const { return cpu.State(); }
    StepResult Step() { return cpu.Step(); }
    void Load(const std::uint8_t* bytes, std::size_t count, std::uint16_t at = 0) {
        for (std::size_t i = 0; i < count; ++i)
            bus.program[(at + i) & 0x0FFF] = bytes[i];
    }
};

static bool IsIllegal(std::uint8_t op) {
    switch (op) {
    case 0x01: case 0x06: case 0x0B: case 0x22: case 0x33:
    case 0x38: case 0x3B: case 0x63: case 0x66: case 0x73:
    case 0x82: case 0x87: case 0x8B: case 0x9B: case 0xA2:
    case 0xA6: case 0xB7: case 0xC0: case 0xC1: case 0xC2:
    case 0xC3: case 0xD6: case 0xE0: case 0xE1: case 0xE2:
    case 0xF3:
        return true;
    default:
        return false;
    }
}

static int ExpectedCycles(std::uint8_t op) {
    if (op >= 0x18 && op <= 0x1F) return 1;
    if (op >= 0x28 && op <= 0x2F) return 1;
    if (op >= 0x48 && op <= 0x4F) return 1;
    if (op >= 0x58 && op <= 0x5F) return 1;
    if (op >= 0x68 && op <= 0x6F) return 1;
    if (op >= 0x78 && op <= 0x7F) return 1;
    if (op >= 0xA8 && op <= 0xAF) return 1;
    if (op >= 0xB8 && op <= 0xBF) return 2;
    if (op >= 0xC8 && op <= 0xCF) return 1;
    if (op >= 0xD8 && op <= 0xDF) return 1;
    if (op >= 0xE8 && op <= 0xEF) return 2;
    if (op >= 0xF8 && op <= 0xFF) return 1;

    switch (op) {
    case 0x00: return 1;
    case 0x02: return 2;
    case 0x03: return 2;
    case 0x04: return 2;
    case 0x05: return 1;
    case 0x07: return 1;
    case 0x08: return 2;
    case 0x09: return 2;
    case 0x0A: return 2;
    case 0x0C: case 0x0D: case 0x0E: case 0x0F: return 2;
    case 0x10: case 0x11: return 1;
    case 0x12: return 2;
    case 0x13: return 2;
    case 0x14: return 2;
    case 0x15: return 1;
    case 0x16: return 2;
    case 0x17: return 1;
    case 0x20: case 0x21: return 1;
    case 0x23: return 2;
    case 0x24: return 2;
    case 0x25: return 1;
    case 0x26: return 2;
    case 0x27: return 1;
    case 0x30: case 0x31: return 1;
    case 0x32: return 2;
    case 0x34: return 2;
    case 0x35: return 1;
    case 0x36: return 2;
    case 0x37: return 1;
    case 0x39: case 0x3A: return 2;
    case 0x3C: case 0x3D: case 0x3E: case 0x3F: return 2;
    case 0x40: case 0x41: return 1;
    case 0x42: return 1;
    case 0x43: return 2;
    case 0x44: return 2;
    case 0x45: return 1;
    case 0x46: return 2;
    case 0x47: return 1;
    case 0x50: case 0x51: return 1;
    case 0x52: return 2;
    case 0x53: return 2;
    case 0x54: return 2;
    case 0x55: return 1;
    case 0x56: return 2;
    case 0x57: return 1;
    case 0x60: case 0x61: return 1;
    case 0x62: return 1;
    case 0x64: return 2;
    case 0x65: return 1;
    case 0x67: return 1;
    case 0x70: case 0x71: return 1;
    case 0x72: return 2;
    case 0x74: return 2;
    case 0x75: return 1;
    case 0x76: return 2;
    case 0x77: return 1;
    case 0x80: case 0x81: return 2;
    case 0x83: return 2;
    case 0x84: return 2;
    case 0x85: return 1;
    case 0x86: return 2;
    case 0x88: return 2;
    case 0x89: return 2;
    case 0x8A: return 2;
    case 0x8C: case 0x8D: case 0x8E: case 0x8F: return 2;
    case 0x90: case 0x91: return 2;
    case 0x92: return 2;
    case 0x93: return 2;
    case 0x94: return 2;
    case 0x95: return 1;
    case 0x96: return 2;
    case 0x97: return 1;
    case 0x98: return 2;
    case 0x99: return 2;
    case 0x9A: return 2;
    case 0x9C: case 0x9D: case 0x9E: case 0x9F: return 2;
    case 0xA0: case 0xA1: return 1;
    case 0xA3: return 2;
    case 0xA4: return 2;
    case 0xA5: return 1;
    case 0xA7: return 1;
    case 0xB0: case 0xB1: return 2;
    case 0xB2: return 2;
    case 0xB3: return 2;
    case 0xB4: return 2;
    case 0xB5: return 1;
    case 0xB6: return 2;
    case 0xC4: return 2;
    case 0xC5: return 1;
    case 0xC6: return 2;
    case 0xC7: return 1;
    case 0xD0: case 0xD1: return 1;
    case 0xD2: return 2;
    case 0xD3: return 2;
    case 0xD4: return 2;
    case 0xD5: return 1;
    case 0xD7: return 1;
    case 0xE3: return 2;
    case 0xE4: return 2;
    case 0xE5: return 1;
    case 0xE6: return 2;
    case 0xE7: return 1;
    case 0xF0: case 0xF1: return 1;
    case 0xF2: return 2;
    case 0xF4: return 2;
    case 0xF5: return 1;
    case 0xF6: return 2;
    case 0xF7: return 1;
    default: return 1; // explicit illegal policy: one cycle, recorded
    }
}

static void TestReset() {
    Fixture f;
    f.Reset();
    f.S().f1 = true;
    f.S().timer_flag = 1;
    f.S().prescaler = 17;
    f.S().timer_mode = 3;
    f.S().irq_enabled = true;
    f.cpu.Reset();
    const auto& s = f.S();
    Check(s.pc == 0 && s.a == 0 && s.psw == 0 && s.sp == 0,
          "reset: PC/A/PSW/SP");
    Check(!s.f1 && s.timer_flag == 0 && s.prescaler == 0 && s.timer_mode == 0,
          "reset: F1/timer/prescaler");
    Check(!s.irq_enabled && !s.timer_irq_enabled && !s.irq_in_service,
          "reset: IRQ state");
    Check(s.p1 == 0xFF && s.p2 == 0xFF,
          "reset: P1/P2 input mode (0xFF, MAME/hardware)");
}

static void TestPcWrap() {
    Fixture f;
    f.Reset();
    for (int i = 0; i < 2047; ++i) f.Step();
    CheckEq(f.S().pc, 0x7FF, "pc wrap: reach 07FF");
    f.Step();
    CheckEq(f.S().pc, 0x000, "pc wrap: 07FF -> 000");

    f.Reset();
    f.S().pc = 0x0FFF;
    f.bus.program[0x0FFF] = 0x00;
    f.Step();
    CheckEq(f.S().pc, 0x800, "pc wrap: 0FFF -> 800");
}

static void TestBusImmediate() {
    Fixture f;
    f.Reset();
    const std::uint8_t orl[] = {0x88, 0x5A};
    f.Load(orl, 2);
    f.bus.bus_value = 0x00;
    f.Step();
    CheckEq(f.S().pc, 0x002, "ORL BUS,#n: PC");
    CheckEq(f.bus.bus_value, 0x5A, "ORL BUS,#n: value");

    f.Reset();
    const std::uint8_t anl[] = {0x98, 0xA5};
    f.Load(anl, 2);
    f.bus.bus_value = 0xFF;
    f.Step();
    CheckEq(f.S().pc, 0x002, "ANL BUS,#n: PC");
    CheckEq(f.bus.bus_value, 0xA5, "ANL BUS,#n: value");
}

static void TestTimerCadence() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0x55; // STRT T
    f.Step();
    const long long start = static_cast<long long>(f.S().cycles);
    int last = f.S().timer;
    int at[3] = {-1, -1, -1};
    int n = 0;
    for (int i = 0; i < 200 && n < 3; ++i) {
        f.Step();
        if (f.S().timer != last) {
            at[n++] = static_cast<int>(f.S().cycles - start);
            last = f.S().timer;
        }
    }
    CheckEq(at[0], 31, "timer cadence: first");
    CheckEq(at[1], 63, "timer cadence: second");
    CheckEq(at[2], 95, "timer cadence: third");
}

static void TestTimerPrescalerReset() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0x55;
    f.S().prescaler = 17;
    f.Step();
    CheckEq(f.S().prescaler, 1, "STRT T: prescaler");
    int last = f.S().timer;
    long long at = -1;
    for (int i = 0; i < 64; ++i) {
        f.Step();
        if (f.S().timer != last) { at = static_cast<long long>(f.S().cycles); break; }
    }
    CheckEq(at, 32, "STRT T: first increment at 32");
}

static void TestTimerCounterExclusive() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x55, 0x45, 0x65};
    f.Load(prog, 3);
    f.Step();
    f.Step();
    CheckEq(f.S().timer_mode, 2, "STRT T/STRT CNT: exclusive");
    f.Step();
    CheckEq(f.S().timer_mode, 0, "STOP TCNT: clears both");
}

static void TestIrqEntry() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x05, 0x00, 0x00, 0x00, 0x93};
    f.Load(prog, 5);
    f.Step(); // EN I
    Check(f.S().irq_enabled, "EN I: enabled");
    f.cpu.SetIrqLevel(true);
    const StepResult entry = f.Step();
    Check(entry.irq_taken && entry.cycles == 3, "IRQ entry + vector: 3 cycles");
    CheckEq(f.S().pc, 0x004, "IRQ entry: vector instruction executed");
    Check(f.S().irq_in_service, "IRQ entry: in service");
    CheckEq(f.S().ram[8], 0x01, "IRQ entry: pushed return low");
    CheckEq(f.S().sp, 1, "IRQ entry: SP");
    CheckEq(f.S().cycles, 4, "IRQ entry: total 1+2+1 cycles");
    f.Step(); // RETR at 0x004
    CheckEq(f.S().pc, 0x001, "RETR: returns to pre-IRQ PC");
    Check(!f.S().irq_in_service, "RETR: clears in-service");
}

static void TestIrqDisabled() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0x00; // NOP
    f.cpu.SetIrqLevel(true);
    const StepResult r = f.Step();
    Check(!r.irq_taken, "disabled IRQ does not vector");
    CheckEq(f.S().pc, 0x001, "disabled IRQ: executes NOP");
}

static void TestRetRetrBank() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0x83; // RET
    f.S().sp = 6;
    f.S().ram[18] = 0x34;
    f.S().ram[19] = 0x08; // PC bit 11 set + PSW high nibble
    f.S().irq_in_service = true;
    f.Step();
    CheckEq(f.S().pc, 0x034, "RET during IRQ masks PC bit 11");
    CheckEq(f.S().sp, 5, "RET: SP");
    Check(f.S().irq_in_service, "RET: still in service");

    f.Reset();
    f.bus.program[0] = 0x93; // RETR
    f.S().sp = 6;
    f.S().ram[18] = 0x34;
    f.S().ram[19] = 0x18; // PC bit 11 + PSW bank
    f.S().irq_in_service = true;
    f.Step();
    CheckEq(f.S().pc, 0x834, "RETR restores PC bit 11");
    CheckEq(f.S().sp, 5, "RETR: SP");
    Check(!f.S().irq_in_service, "RETR: clears in-service");
    CheckEq(f.S().psw & 0x10, 0x10, "RETR: restores bank bit");
}

static void TestCallRet() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x14, 0x10};
    f.Load(prog, 2);
    f.bus.program[0x10] = 0x83; // RET
    f.Step();
    CheckEq(f.S().pc, 0x010, "CALL: target");
    CheckEq(f.S().sp, 1, "CALL: SP");
    f.Step();
    CheckEq(f.S().pc, 0x002, "RET: return address");
    CheckEq(f.S().sp, 0, "RET: SP");
}

static void TestBanks() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0xD5, 0xB8, 0xAA, 0xC5, 0xF8};
    f.Load(prog, 5);
    f.Step(); // SEL RB1
    f.Step(); // MOV R0,#AA
    CheckEq(f.S().ram[24], 0xAA, "RB1 R0 at RAM 24");
    f.Step(); // SEL RB0
    f.Step(); // MOV A,R0
    CheckEq(f.S().a, 0x00, "RB0 R0 independent");
}

static void TestMovx() {
    Fixture f;
    f.Reset();
    f.S().ram[0] = 0x20;
    f.bus.data[0x20] = 0x5A;
    f.bus.program[0] = 0x80; // MOVX A,@R0
    f.Step();
    CheckEq(f.S().a, 0x5A, "MOVX A,@R0");
    f.S().a = 0x33;
    f.S().pc = 0;
    f.bus.program[0] = 0x90; // MOVX @R0,A
    f.Step();
    CheckEq(f.bus.data[0x20], 0x33, "MOVX @R0,A");
}

static void TestPortsAndBus() {
    Fixture f;
    f.Reset();
    f.bus.port_in[1] = 0x5A;
    f.S().p1 = 0xFF;
    f.bus.program[0] = 0x09; // IN A,P1
    f.Step();
    CheckEq(f.S().a, 0x5A, "IN A,P1");

    f.S().a = 0x66;
    f.S().pc = 0;
    f.bus.program[0] = 0x39; // OUTL P1,A
    f.Step();
    CheckEq(f.bus.port_out[1], 0x66, "OUTL P1,A");

    f.S().p1 = 0x60;
    f.S().pc = 0;
    const std::uint8_t orl[] = {0x89, 0x0F};
    f.Load(orl, 2);
    f.Step();
    CheckEq(f.S().p1, 0x6F, "ORL P1,#n");
    f.S().pc = 0;
    const std::uint8_t anl[] = {0x99, 0x0F};
    f.Load(anl, 2);
    f.Step();
    CheckEq(f.S().p1, 0x0F, "ANL P1,#n");

    f.S().a = 0x5A;
    f.S().pc = 0;
    f.bus.program[0] = 0x02; // OUTL BUS,A
    f.Step();
    CheckEq(f.bus.bus_value, 0x5A, "OUTL BUS,A");
    f.bus.bus_value = 0x33;
    f.S().pc = 0;
    f.bus.program[0] = 0x08; // INS A,BUS
    f.Step();
    CheckEq(f.S().a, 0x33, "INS A,BUS");
}

static void TestExpander() {
    Fixture f;
    f.Reset();
    f.S().a = 0x05;
    f.bus.program[0] = 0x3C; // MOVD P4,A
    f.Step();
    CheckEq(f.bus.expander[4], 0x05, "MOVD P4,A");
    f.bus.program[0] = 0x0C; // MOVD A,P4
    f.S().pc = 0;
    f.Step();
    CheckEq(f.S().a, 0x05, "MOVD A,P4");
    f.S().a = 0x0A;
    f.S().pc = 0;
    const std::uint8_t orld[] = {0x8C};
    f.Load(orld, 1);
    f.Step();
    CheckEq(f.bus.expander[4], 0x0F, "ORLD P4,A");
    f.S().a = 0x03;
    f.S().pc = 0;
    const std::uint8_t anld[] = {0x9C};
    f.Load(anld, 1);
    f.Step();
    CheckEq(f.bus.expander[4], 0x03, "ANLD P4,A");
}

static void TestBranches() {
    Fixture f;
    f.Reset();
    f.S().pc = 0x1FF;
    f.bus.program[0x1FF] = 0xF6; // JC
    f.bus.program[0x200] = 0x45;
    f.S().psw |= 0x80;
    f.Step();
    CheckEq(f.S().pc, 0x245, "JC page-relative");
}

static void TestAddcExhaustive() {
    unsigned mismatches = 0;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned d = 0; d < 256; ++d) {
            for (unsigned c = 0; c < 2; ++c) {
                Fixture f;
                f.Reset();
                f.bus.program[0] = 0x13;
                f.bus.program[1] = static_cast<std::uint8_t>(d);
                f.S().a = static_cast<std::uint8_t>(a);
                f.S().psw = c ? 0x80 : 0x00;
                f.Step();
                const unsigned full = a + d + c;
                const unsigned expAcc = full & 0xFF;
                const unsigned expCy = full > 0xFF ? 0x80 : 0;
                const unsigned expAc = (((a & 0x0F) + (d & 0x0F) + c) > 0x0F) ? 0x40 : 0;
                if (f.S().a != expAcc || (f.S().psw & 0x80) != expCy ||
                    (f.S().psw & 0x40) != expAc)
                    ++mismatches;
            }
        }
    }
    CheckEq(mismatches, 0, "ADDC A,#n exhaustive (131072)");
}

static void TestAddcRegisterExhaustive() {
    unsigned mismatches = 0;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned d = 0; d < 256; ++d) {
            for (unsigned c = 0; c < 2; ++c) {
                Fixture f;
                f.Reset();
                f.bus.program[0] = 0x78; // ADDC A,R0
                f.S().a = static_cast<std::uint8_t>(a);
                f.S().ram[0] = static_cast<std::uint8_t>(d);
                f.S().psw = c ? 0x80 : 0x00;
                f.Step();
                const unsigned full = a + d + c;
                const unsigned expAcc = full & 0xFF;
                const unsigned expCy = full > 0xFF ? 0x80 : 0;
                const unsigned expAc = (((a & 0x0F) + (d & 0x0F) + c) > 0x0F) ? 0x40 : 0;
                if (f.S().a != expAcc || (f.S().psw & 0x80) != expCy ||
                    (f.S().psw & 0x40) != expAc)
                    ++mismatches;
            }
        }
    }
    CheckEq(mismatches, 0, "ADDC A,R0 exhaustive (131072)");
}

static void TestOpcodeSmokeAndCycles() {
    int illegalPass = 0;
    int legalPass = 0;
    for (int op = 0; op < 256; ++op) {
        Fixture f;
        f.Reset();
        f.bus.program[0] = static_cast<std::uint8_t>(op);
        f.bus.program[1] = 0x00;
        f.bus.program[2] = 0x00;
        f.S().pc = 0;
        f.S().sp = 0;
        f.bus.data[0] = 0x00;
        const StepResult r = f.Step();
        Check(f.S().pc <= 0x0FFF, "opcode smoke: PC is 12-bit");
        Check(r.cycles >= 1 && r.cycles <= 2, "opcode smoke: cycles 1..2");
        if (IsIllegal(static_cast<std::uint8_t>(op))) {
            if (f.S().illegal_count == 1 && f.S().illegal_opcode == op)
                ++illegalPass;
            else
                Check(false, "illegal opcode policy", "not recorded");
        } else {
            if (r.cycles == ExpectedCycles(static_cast<std::uint8_t>(op)))
                ++legalPass;
            else {
                char detail[96];
                std::snprintf(detail, sizeof(detail), "op=%02X actual=%u expected=%d",
                              op, r.cycles, ExpectedCycles(static_cast<std::uint8_t>(op)));
                Check(false, "legal opcode cycle count", detail);
            }
        }
    }
    CheckEq(illegalPass, 26, "all illegal opcodes recorded");
    CheckEq(legalPass, 230, "all legal opcodes cycle-checked");
}

static void TestSaveRestore() {
    Fixture f;
    f.Reset();
    // Deterministic ALU/register-only program: no external state changes.
    const std::uint8_t prog[] = {
        0x23, 0x11, 0x03, 0x22, 0x13, 0x33, 0x53, 0x0F,
        0x43, 0x80, 0xE7, 0x77, 0x67, 0x57, 0x00, 0x00
    };
    f.Load(prog, sizeof(prog));
    for (int i = 0; i < 5; ++i) f.Step();
    std::vector<std::uint8_t> saved;
    f.cpu.SaveState(saved);
    for (int i = 0; i < 40; ++i) f.Step();
    const Mcs48State after = f.S();

    Check(f.cpu.LoadState(saved.data(), saved.size()), "LoadState accepts snapshot");
    for (int i = 0; i < 40; ++i) f.Step();
    Check(std::memcmp(&after, &f.S(), sizeof(Mcs48State)) == 0,
          "save/restore replay determinism");
    CheckEq(f.S().cycles, after.cycles, "save/restore cycles");
}

static void TestTrace() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x23, 0x5A, 0x03, 0x01, 0xE7, 0x00};
    f.Load(prog, sizeof(prog));
    mcs48::TraceRing ring(16);
    mcs48::TraceRing::enabled = true;
    for (int i = 0; i < 6; ++i) {
        mcs48::TraceEvent ev;
        ev.machineCycles = f.S().cycles;
        ev.pc = f.S().pc;
        ev.opcode = f.bus.program[f.S().pc & 0x0FFF];
        ev.a = f.S().a;
        ev.psw = f.S().psw;
        ev.timerValue = f.S().timer;
        ev.prescaler = f.S().prescaler;
        ev.timerMode = f.S().timer_mode;
        ev.regBank = static_cast<std::uint8_t>(f.S().reg_bank ? 24 : 0);
        ev.p1Latch = f.S().p1;
        ev.p2Latch = f.S().p2;
        ring.Observe(ev);
        f.Step();
    }
    mcs48::TraceRing::enabled = false;
    CheckEq(static_cast<long long>(ring.Count()), 6, "trace: record count");
    CheckEq(static_cast<long long>(ring.At(0).pc), 0x000, "trace: first PC");
    CheckEq(static_cast<long long>(ring.At(0).machineCycles), 0, "trace: first cycle");
    Check(ring.At(5).machineCycles > ring.At(0).machineCycles, "trace: monotonic cycles");
}

static void TestCounterT1() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    f.Load(prog, sizeof(prog));
    f.cpu.SetT1(false);
    f.Step(); // STRT CNT
    CheckEq(f.S().timer_mode, 2, "STRT CNT: mode");
    const int start = f.S().timer;

    f.cpu.SetT1(true);
    f.Step(); // high sample
    f.cpu.SetT1(false);
    f.Step(); // falling edge -> increment
    CheckEq(f.S().timer, start + 1, "counter: falling edge increments");
    f.Step(); // low/low -> no edge
    CheckEq(f.S().timer, start + 1, "counter: low/low no increment");
    f.cpu.SetT1(true);
    f.Step();
    f.cpu.SetT1(false);
    f.Step();
    CheckEq(f.S().timer, start + 2, "counter: second falling edge");
}

static void TestTimerIrqAndJtf() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0x25, 0x55, 0x00, 0x00, 0x00, 0x07, 0x00};
    f.Load(prog, sizeof(prog));
    f.Step(); // EN TCNTI
    f.Step(); // STRT T
    f.S().timer = 0xFF;
    f.S().prescaler = 31;
    f.Step(); // NOP burns one cycle -> overflow, pending
    Check(f.S().timer_flag != 0, "timer overflow: flag");
    Check(f.S().timer_irq_pending, "timer overflow: IRQ pending");
    const auto entry = f.Step(); // timer IRQ entry + vector NOP
    Check(entry.irq_taken && entry.cycles == 3, "timer IRQ: 2+1 cycles");
    CheckEq(f.S().pc, 0x008, "timer IRQ: vector 0x007 executed");
    Check(f.S().irq_in_service, "timer IRQ: in service");

    f.Reset();
    f.bus.program[0] = 0x16; // JTF
    f.bus.program[1] = 0x10;
    f.S().timer_flag = 1;
    f.Step();
    CheckEq(f.S().pc, 0x010, "JTF: taken");
    CheckEq(f.S().timer_flag, 0, "JTF: clears flag");
}

static void TestMbAndProgramMemory() {
    Fixture f;
    f.Reset();
    const std::uint8_t prog[] = {0xF5, 0x04, 0x10};
    f.Load(prog, 3);
    f.Step(); // SEL MB1
    f.Step(); // JMP page0 offset 0x10 -> 0x810
    CheckEq(f.S().pc, 0x810, "SEL MB1 + JMP: bank 1");

    f.Reset();
    f.bus.program[0] = 0xA3; // MOVP A,@A
    f.bus.program[0x05] = 0x5A;
    f.S().a = 0x05;
    f.Step();
    CheckEq(f.S().a, 0x5A, "MOVP A,@A");

    f.Reset();
    f.bus.program[0] = 0xE3; // MOVP3 A,@A
    f.bus.program[0x305] = 0x5A;
    f.S().a = 0x05;
    f.Step();
    CheckEq(f.S().a, 0x5A, "MOVP3 A,@A");

    f.Reset();
    f.bus.program[0] = 0xB3; // JMPP @A
    f.bus.program[0x05] = 0x10;
    f.S().a = 0x05;
    f.Step();
    CheckEq(f.S().pc, 0x010, "JMPP @A");

    f.Reset();
    const std::uint8_t call_bank[] = {0xF5, 0x14, 0x10}; // SEL MB1; CALL 0x010
    f.Load(call_bank, 3);
    f.bus.program[0x810] = 0x83; // RET in bank 1
    f.Step();
    f.Step();
    CheckEq(f.S().pc, 0x810, "SEL MB1 + CALL: bank 1 target");
    f.Step();
    CheckEq(f.S().pc, 0x003, "CALL/RET: returns to bank-0 caller");

    f.Reset();
    const std::uint8_t mb0[] = {0xF5, 0xE5, 0x04, 0x10}; // SEL MB1; SEL MB0; JMP 0x010
    f.Load(mb0, 4);
    f.Step();
    f.Step();
    f.Step();
    CheckEq(f.S().pc, 0x010, "SEL MB0: jump returns to bank 0");
}

static void TestFlagsAndRotates() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0xF7; // RLC A
    f.S().a = 0x80;
    f.S().psw = 0x01; // C=0
    f.Step();
    CheckEq(f.S().a, 0x00, "RLC A: value");
    CheckEq(f.S().psw & 0x80, 0x80, "RLC A: carry out");

    f.Reset();
    f.bus.program[0] = 0x67; // RRC A
    f.S().a = 0x01;
    f.S().psw = 0x00;
    f.Step();
    CheckEq(f.S().a, 0x00, "RRC A: value");
    CheckEq(f.S().psw & 0x80, 0x80, "RRC A: carry out");

    f.Reset();
    f.bus.program[0] = 0x47; // SWAP A
    f.S().a = 0x12;
    f.Step();
    CheckEq(f.S().a, 0x21, "SWAP A");

    f.Reset();
    f.bus.program[0] = 0x57; // DA A
    f.S().a = 0x0A;
    f.Step();
    CheckEq(f.S().a, 0x10, "DA A: low nibble adjust");
}

static void TestAddExhaustive() {
    unsigned mismatches = 0;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned d = 0; d < 256; ++d) {
            Fixture f;
            f.Reset();
            f.bus.program[0] = 0x03;
            f.bus.program[1] = static_cast<std::uint8_t>(d);
            f.S().a = static_cast<std::uint8_t>(a);
            f.Step();
            const unsigned full = a + d;
            const unsigned expAcc = full & 0xFF;
            const unsigned expCy = full > 0xFF ? 0x80 : 0;
            const unsigned expAc = (((a & 0x0F) + (d & 0x0F)) > 0x0F) ? 0x40 : 0;
            if (f.S().a != expAcc || (f.S().psw & 0x80) != expCy ||
                (f.S().psw & 0x40) != expAc)
                ++mismatches;
        }
    }
    CheckEq(mismatches, 0, "ADD A,#n exhaustive (65536)");
}

static void TestT0T1Branches() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0x36; // JT0
    f.bus.program[1] = 0x10;
    f.cpu.SetT0(true);
    f.Step();
    CheckEq(f.S().pc, 0x010, "JT0: taken");

    f.Reset();
    f.bus.program[0] = 0x56; // JT1
    f.bus.program[1] = 0x10;
    f.cpu.SetT1(true);
    f.Step();
    CheckEq(f.S().pc, 0x010, "JT1: taken");

    f.Reset();
    f.bus.program[0] = 0x26; // JNT0
    f.bus.program[1] = 0x10;
    f.cpu.SetT0(false);
    f.Step();
    CheckEq(f.S().pc, 0x010, "JNT0: taken");

    f.Reset();
    f.bus.program[0] = 0x75; // ENT0 CLK
    f.Step();
    Check(f.bus.t0_clock, "ENT0 CLK: enabled");
}

static void TestStackDepths() {
    for (int depth = 0; depth < 8; ++depth) {
        Fixture f;
        f.Reset();
        f.S().sp = static_cast<std::uint8_t>(depth);
        f.bus.program[0] = 0x14; // CALL 0x010
        f.bus.program[1] = 0x10;
        f.bus.program[0x10] = 0x83; // RET
        f.Step();
        CheckEq(f.S().sp, (depth + 1) & 7, "CALL: stack depth");
        f.Step();
        CheckEq(f.S().sp, depth, "RET: stack depth restored");
        CheckEq(f.S().pc, 0x002, "RET: return PC");
    }

    for (int depth = 0; depth < 8; ++depth) {
        Fixture f;
        f.Reset();
        f.S().sp = static_cast<std::uint8_t>(depth);
        f.S().irq_enabled = true;
        f.S().irq_pending = true;
        f.bus.program[3] = 0x00; // vector NOP
        f.bus.program[4] = 0x93; // RETR
        f.Step(); // IRQ entry + vector NOP
        CheckEq(f.S().sp, (depth + 1) & 7, "IRQ: stack depth");
        Check(f.S().irq_in_service, "IRQ: in service");
        f.Step(); // RETR
        CheckEq(f.S().sp, depth, "RETR: stack depth restored");
        Check(!f.S().irq_in_service, "RETR: service cleared");
    }
}

static void TestPsw() {
    Fixture f;
    f.Reset();
    f.bus.program[0] = 0xD7; // MOV PSW,A
    f.bus.program[1] = 0xC7; // MOV A,PSW
    f.S().a = 0x9B; // carry + bank + bit3 + SP=3
    f.Step();
    CheckEq(f.S().sp, 3, "MOV PSW,A: SP");
    CheckEq(f.S().psw & 0x90, 0x90, "MOV PSW,A: C/BS");
    f.Step();
    CheckEq(f.S().a, 0x9B, "MOV A,PSW: bit3 + SP");
}

static void TestRandomInvariants() {
    Fixture f;
    f.Reset();
    std::uint32_t seed = 0x12345678u;
    int failures = 0;
    for (int i = 0; i < 4096; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const std::uint8_t op = static_cast<std::uint8_t>(seed >> 24);
        f.bus.program[0] = op;
        f.bus.program[1] = static_cast<std::uint8_t>(seed >> 16);
        f.bus.program[2] = static_cast<std::uint8_t>(seed >> 8);
        f.S().pc = 0;
        const auto r = f.Step();
        if (f.S().pc > 0x0FFF || f.S().sp > 7 || r.cycles < 1 || r.cycles > 4)
            ++failures;
    }
    CheckEq(failures, 0, "random instruction invariants (4096)");
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--dump-cycles") == 0) {
        for (int op = 0; op < 256; ++op)
            std::printf("%d,%d,%d\n", op,
                        ExpectedCycles(static_cast<std::uint8_t>(op)),
                        IsIllegal(static_cast<std::uint8_t>(op)) ? 1 : 0);
        return 0;
    }

    std::printf("MCS48-NG standalone conformance suite\n");
    std::printf("core: src/core_ng/cpu/mcs48_ng.cpp, owned state, stub bus\n\n");

    TestReset();
    TestPcWrap();
    TestBusImmediate();
    TestTimerCadence();
    TestTimerPrescalerReset();
    TestTimerCounterExclusive();
    TestIrqEntry();
    TestIrqDisabled();
    TestRetRetrBank();
    TestCallRet();
    TestStackDepths();
    TestPsw();
    TestBanks();
    TestMovx();
    TestPortsAndBus();
    TestExpander();
    TestBranches();
    TestT0T1Branches();
    TestCounterT1();
    TestTimerIrqAndJtf();
    TestMbAndProgramMemory();
    TestFlagsAndRotates();
    TestAddExhaustive();
    TestAddcExhaustive();
    TestAddcRegisterExhaustive();
    TestOpcodeSmokeAndCycles();
    TestSaveRestore();
    TestTrace();
    TestRandomInvariants();

    std::printf("\n--- summary ---\n");
    std::printf("checks: %d, failures: %d\n", g_tests, g_failures);
    if (g_failures == 0) {
        std::printf("RESULT: GREEN - MCS48-NG conformance checks passed.\n");
        return 0;
    }
    std::printf("RESULT: RED\n");
    return 1;
}
