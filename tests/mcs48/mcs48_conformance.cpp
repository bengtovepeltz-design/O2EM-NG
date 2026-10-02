// ============================================================================
// tests/mcs48/mcs48_conformance.cpp
//
// MCS48-NG Phase 2A: standalone 8048 conformance harness.
//
// Links the REAL production cpu.cpp (unmodified) against the inert stub
// machine environment and executes it with app_data.debug = 1 - the core's
// own existing single-instruction exit. Each cpu_exec() call therefore runs
// exactly one instruction plus the production scheduler tail. No frontend,
// no window, no audio, no user data, no ROM files.
//
// SUITE SEMANTICS (important):
//   - Tests labeled PASS_CONTROL assert legacy behavior already matches the
//     8048 reference (Intel manual / MAME as recorded in Phase 1). If one of
//     these fails, production cpu.cpp has drifted from the audited baseline.
//   - Tests labeled EXPECTED_LEGACY_FAILURE assert that the legacy core does
//     NOT yet match the reference. They are ALLOWED AND EXPECTED to fail
//     against cpu.cpp; they document known Phase 1 defects and define the
//     acceptance criteria for the future MCS48-NG core. cpu.cpp must NOT be
//     edited to make them pass.
//
// Exit codes: 0 = green (all controls pass AND all labeled legacy failures
// confirmed). 1 = drift (a control failed or a known failure started
// passing). 2 = suite integrity problem (sanity precondition failed).
// ============================================================================

#include <cstdio>
#include <cstring>

#include "types.h"
#include "cpu.h"
#include "vmachine.h"
#include "src/mcs48/trace_record.h"
#include "src/mcs48/input_schedule.h"

// ---------------------------------------------------------------------------
// Tiny reporting framework
// ---------------------------------------------------------------------------

static int g_controlsRun = 0, g_controlsPassed = 0;
static int g_knownFailuresRun = 0, g_knownFailuresConfirmed = 0;
static int g_sanityFailures = 0;
static bool g_suiteDrift = false;

static void SanityCheck(bool condition, const char* what) {
    if (!condition) {
        std::printf("  !! SANITY FAILURE: %s (test result not trustworthy)\n", what);
        ++g_sanityFailures;
    }
}

// PASS_CONTROL: reference condition must hold on legacy today.
static void ControlResult(const char* name, bool referenceConditionMet,
                          const char* observed, const char* expected) {
    ++g_controlsRun;
    if (referenceConditionMet) {
        ++g_controlsPassed;
        std::printf("[PASS_CONTROL]           %-34s passed   (legacy matches reference)\n", name);
    } else {
        g_suiteDrift = true;
        std::printf("[PASS_CONTROL-FAILED]    %-34s DRIFT    observed=%s expected=%s\n",
                     name, observed, expected);
    }
}

// EXPECTED_LEGACY_FAILURE: reference condition must NOT hold on legacy today.
static void KnownFailureResult(const char* name, bool referenceConditionMet,
                               const char* observed, const char* expected) {
    ++g_knownFailuresRun;
    if (!referenceConditionMet) {
        ++g_knownFailuresConfirmed;
        std::printf("[EXPECTED_LEGACY_FAILURE] %-34s confirmed (legacy=%s, reference=%s)\n",
                     name, observed, expected);
    } else {
        g_suiteDrift = true;
        std::printf("[EXPECTED_LEGACY_FAILURE] %-34s NO LONGER FAILS (legacy now=%s, reference=%s)\n",
                     name, observed, expected);
        std::printf("  -> cpu.cpp behavior changed since the Phase 1 audit, or MCS48-NG replaced it.\n");
    }
}

// ---------------------------------------------------------------------------
// Machine environment helpers. Reset() zeroes the stub machine exactly; the
// CPU owns nothing here except its globals, so init_cpu() plus a cleared
// machine is a fully deterministic cold start.
// ---------------------------------------------------------------------------

static void MachineReset() {
    std::memset(rom, 0, 4096);          // NOP-filled program space
    std::memset(intRAM, 0, 256);        // incomplete extern in vmachine.h
    std::memset(extRAM, 0, 256);
    std::memset(VDCwrite, 0, sizeof(VDCwrite));
    master_clk = 0;
    h_clk = 0;
    int_clk = 0;
    evblclk = 0;
    mstate = 0;
    enahirq = 0;
    pendirq = 0;
    clk_counter = 0;
    std::memset(&app_data, 0, sizeof(app_data));
    app_data.debug = 1;                  // core's existing single-step exit
    app_data.breakpoint = -1;            // never matches a 12-bit PC
    app_data.crc = 0;                    // avoids VP31/Atlantis CRC branches
    init_cpu();                          // production CPU reset (defects and all)
}

static void Step() { cpu_exec(); }

static void LoadProgramAt(unsigned addr, const unsigned char* bytes, unsigned count) {
    for (unsigned i = 0; i < count; ++i) rom[(addr + i) & 0xfff] = bytes[i];
}

// ---------------------------------------------------------------------------
// EXPECTED LEGACY FAILURES (Phase 1 section 5, each recipe reproduced)
// ---------------------------------------------------------------------------

// PC increment must wrap within the 11-bit field, preserving the bank bit.
// Legacy: plain pc++ crosses 0x07FF -> 0x0800 and 0x0FFF -> 0x1000.
static void TestPcIncrement0x07FF() {
    MachineReset();
    for (int i = 0; i < 2047; ++i) Step();
    SanityCheck(pc == 0x7FF, "expected PC 0x7FF after 2047 NOPs");
    Step(); // NOP at 0x7FF
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    KnownFailureResult("pc_increment_07ff", pc == 0x000, obs, "PC=000");
}

static void TestPcIncrement0x0FFF() {
    MachineReset();
    for (int i = 0; i < 4095; ++i) Step();
    SanityCheck(pc == 0xFFF, "expected PC 0xFFF after 4094 NOPs");
    Step(); // NOP at 0xFFF
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    KnownFailureResult("pc_increment_0fff", pc == 0x800, obs, "PC=800");
}

// ORL BUS,#imm (0x88) / ANL BUS,#imm (0x98) must consume the operand byte.
// Legacy: neither updates BUS nor consumes the immediate (PC 0001, not 0002).
static void TestOrlBusImmediate() {
    MachineReset();
    const unsigned char prog[] = {0x88, 0x5A};
    LoadProgramAt(0, prog, 2);
    Step();
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    KnownFailureResult("orl_bus_immediate_88", pc == 0x002, obs, "PC=002");
}

static void TestAnlBusImmediate() {
    MachineReset();
    const unsigned char prog[] = {0x98, 0xA5};
    LoadProgramAt(0, prog, 2);
    Step();
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    KnownFailureResult("anl_bus_immediate_98", pc == 0x002, obs, "PC=002");
}

// Timer divide-by-32 cadence. Measured in machine cycles after STRT T's own
// cycle. Reference: increments every 32 -> at 31,63,95 here (absolute cycles
// 32,64,96 incl. STRT T - the Phase 1 convention). Legacy: subtracts 31,
// giving 32,63,94 absolute (31,62,93 here).
static void TestTimerDivideCadence() {
    MachineReset();
    const unsigned char prog[] = {0x55}; // STRT T, then NOP-filled space
    LoadProgramAt(0, prog, 1);
    Step(); // STRT T
    const int startClk = master_clk;
    int increments[3]; int n = 0;
    int lastTimer = itimer;
    for (int i = 0; i < 200 && n < 3; ++i) {
        Step();
        if (itimer != lastTimer) {
            increments[n++] = master_clk - startClk;
            lastTimer = itimer;
        }
    }
    SanityCheck(timer_on == 1, "STRT T must set timer_on");
    SanityCheck(n == 3, "expected 3 timer increments within 200 cycles");
    char obs[64]; std::snprintf(obs, sizeof(obs), "at %d,%d,%d", increments[0], increments[1], increments[2]);
    bool reference = increments[0] == 31 && increments[1] == 63 && increments[2] == 95;
    KnownFailureResult("timer_divide_32_cadence", reference, obs, "at 31,63,95");
}

// STRT T must clear the divide-by-32 prescaler. Legacy keeps counting from
// the previous prescaler value (Phase 1: 17 -> 18 instead of restart).
static void TestStrtTClrPrescaler() {
    MachineReset();
    const unsigned char prog[] = {0x55}; // STRT T
    LoadProgramAt(0, prog, 1);
    master_count = 17;                    // previous prescaler residue
    Step();                               // STRT T (+ its own cycle)
    int prescalerAfterStart = master_count;
    // First increment must come 32 machine cycles after STRT T.
    int incrementAt = -1;
    int lastTimer = itimer;
    for (int i = 0; i < 64; ++i) {
        Step();
        if (incrementAt < 0 && itimer != lastTimer) incrementAt = master_clk;
        if (incrementAt >= 0) break;
    }
    char obs[64]; std::snprintf(obs, sizeof(obs), "presc=%d inc@%d", prescalerAfterStart, incrementAt);
    bool reference = prescalerAfterStart == 1 && incrementAt == 32;
    KnownFailureResult("strt_t_clears_prescaler", reference, obs, "presc=1 inc@32");
}

// STRT T and STRT CNT are mutually exclusive on the 8048. Legacy leaves both
// modes active. STOP TCNT (0x65) must clear both (that part is a control).
static void TestStrtTStrtCntExclusive() {
    MachineReset();
    const unsigned char prog[] = {0x55, 0x45, 0x65}; // STRT T, STRT CNT, STOP TCNT
    LoadProgramAt(0, prog, 3);
    Step(); Step();
    bool bothActive = timer_on == 1 && count_on == 1;
    char obs[48]; std::snprintf(obs, sizeof(obs), "T=%d CNT=%d", timer_on, count_on);
    KnownFailureResult("strt_t_cnt_exclusive", !bothActive, obs, "counter mode only");

    Step(); // STOP TCNT
    ControlResult("stop_tcnt_clears_both", timer_on == 0 && count_on == 0,
                  (std::snprintf(obs, sizeof(obs), "T=%d CNT=%d", timer_on, count_on), obs),
                  "T=0 CNT=0");
}

// Reset state: Intel specifies F1, timer flag and prescaler cleared on RST.
// init_cpu() leaves all three (Phase 1 probe).
static void TestResetF1() {
    MachineReset();
    f1 = 1;
    init_cpu();
    char obs[16]; std::snprintf(obs, sizeof(obs), "F1=%u", (unsigned)f1);
    KnownFailureResult("reset_clears_f1", f1 == 0, obs, "F1=0");
}

static void TestResetTimerFlag() {
    MachineReset();
    t_flag = 1;
    init_cpu();
    char obs[16]; std::snprintf(obs, sizeof(obs), "TF=%u", (unsigned)t_flag);
    KnownFailureResult("reset_clears_timer_flag", t_flag == 0, obs, "TF=0");
}

static void TestResetPrescaler() {
    MachineReset();
    master_count = 17;
    init_cpu();
    char obs[16]; std::snprintf(obs, sizeof(obs), "presc=%d", master_count);
    KnownFailureResult("reset_clears_prescaler", master_count == 0, obs, "presc=0");
}

// Interrupt entry cycle accounting. Reference: the two entry cycles become
// machine time. Legacy adds them to clk AFTER master_clk was advanced, and
// the next iteration zeroes clk, losing them (elapsed machine time 1, clk 3).
static void TestIrqEntryCycles() {
    MachineReset();
    const unsigned char prog[] = {0x05, 0x00}; // EN I, NOP, then NOP space
    LoadProgramAt(0, prog, 2);
    Step(); // EN I
    SanityCheck(xirq_en == 1, "EN I must set xirq_en");
    xirq_pend = 1;                         // synthetic external IRQ pending
    const int before = master_clk;
    Step(); // NOP executes; pending IRQ serviced in the scheduler tail
    const int elapsed = master_clk - before;
    SanityCheck(pc == 0x003, "IRQ entry must vector to 0x003");
    SanityCheck(irq_ex == 1, "IRQ entry must mark in-service");
    SanityCheck(sp == 10 && intRAM[8] == 0x02, "entry must push return address");
    char obs[48]; std::snprintf(obs, sizeof(obs), "machine=%d clk=%ld", elapsed, clk);
    // Reference: 1 (NOP) + 2 (entry) = 3 machine cycles.
    KnownFailureResult("irq_entry_cycle_accounting", elapsed == 3, obs, "machine=3");
}

// RET during interrupt service must not resurrect memory-bank bit 11.
// Phase 1 probe: stacked return address 0x834 (high stack byte 0x08) ->
// legacy restores 0x834; reference keeps bit 11 clear: 0x034.
static void TestRetDuringIrqBank() {
    MachineReset();
    const unsigned char prog[] = {0x83}; // RET, then NOP space
    LoadProgramAt(0, prog, 1);
    irq_ex = 1;                            // simulated in-service interrupt
    sp = 12;
    intRAM[10] = 0x34;                     // return address low byte
    intRAM[11] = 0x08;                     // high: bit 11 set, bits 8-10 clear
    Step();
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    SanityCheck(sp == 10, "RET must pop two stack bytes");
    SanityCheck(irq_ex == 1, "RET must not exit interrupt service (RETR does)");
    KnownFailureResult("ret_bank_during_irq", pc == 0x034, obs, "PC=034");
}

// ---------------------------------------------------------------------------
// POSITIVE CONTROLS
// ---------------------------------------------------------------------------

// Exhaustive ADDC A,#data (0x13): all 131072 A/operand/carry-in combinations.
// Phase 1 recorded 0 mismatches; this guards the arithmetic core.
static void TestAddcImmediateExhaustive() {
    MachineReset();
    const unsigned char prog[] = {0x13, 0x00}; // ADDC A,#d (operand patched)
    LoadProgramAt(0, prog, 2);
    unsigned mismatches = 0; unsigned firstBad = 0; bool haveBad = false;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned d = 0; d < 256; ++d) {
            for (unsigned c = 0; c < 2; ++c) {
                init_cpu();
                acc = static_cast<Byte>(a);
                cy = static_cast<Byte>(c);
                ac = 0;
                pc = 0;
                rom[1] = static_cast<Byte>(d);
                Step();
                const unsigned full = a + d + c;
                const Byte expAcc = static_cast<Byte>(full & 0xFF);
                const Byte expCy = full > 0xFF ? 1 : 0;
                const Byte expAc = (((a & 0x0F) + (d & 0x0F) + c) > 0x0F) ? 0x40 : 0;
                if (acc != expAcc || cy != expCy || ac != expAc) {
                    ++mismatches;
                    if (!haveBad) { firstBad = (a << 16) | (d << 8) | c; haveBad = true; }
                }
            }
        }
    }
    char obs[48]; std::snprintf(obs, sizeof(obs), "mism=%u", mismatches);
    ControlResult("addc_immediate_exhaustive", mismatches == 0, obs, "mism=0/131072");
    if (haveBad) std::printf("  first mismatch: A=%02X D=%02X C=%u (key %06X)\n",
                             (firstBad >> 16) & 0xFF, (firstBad >> 8) & 0xFF, firstBad & 0xFF, firstBad);
}

// Exhaustive ADDC A,R0 (0x78) through the register file, same coverage.
static void TestAddcRegisterExhaustive() {
    MachineReset();
    const unsigned char prog[] = {0x78}; // ADDC A,R0
    LoadProgramAt(0, prog, 1);
    unsigned mismatches = 0;
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned d = 0; d < 256; ++d) {
            for (unsigned c = 0; c < 2; ++c) {
                init_cpu();
                acc = static_cast<Byte>(a);
                cy = static_cast<Byte>(c);
                ac = 0;
                pc = 0;
                intRAM[0] = static_cast<Byte>(d);
                Step();
                const unsigned full = a + d + c;
                const Byte expAcc = static_cast<Byte>(full & 0xFF);
                const Byte expCy = full > 0xFF ? 1 : 0;
                const Byte expAc = (((a & 0x0F) + (d & 0x0F) + c) > 0x0F) ? 0x40 : 0;
                if (acc != expAcc || cy != expCy || ac != expAc) ++mismatches;
            }
        }
    }
    char obs[48]; std::snprintf(obs, sizeof(obs), "mism=%u", mismatches);
    ControlResult("addc_register_exhaustive", mismatches == 0, obs, "mism=0/131072");
}

// Conditional branch page rule (Phase 1: JNZ at 01FF reached 0245). This
// core implements JC (0xF6); same in-page rule must hold at a page boundary.
static void TestBranchPageControl() {
    MachineReset();
    const unsigned char prog[] = {0xF6, 0x45}; // JC 0x45, then NOP space
    LoadProgramAt(0x1FF, prog, 2);
    pc = 0x1FF;
    cy = 1;                                 // condition true
    Step();
    char obs[32]; std::snprintf(obs, sizeof(obs), "PC=%03X", (unsigned)pc);
    ControlResult("branch_page_jc_1ff", pc == 0x245, obs, "PC=245");
}

// ---------------------------------------------------------------------------
// Trace + schedule smoke demonstration (Phase 2A deliverables, off by default)
// ---------------------------------------------------------------------------

static void TraceAndScheduleDemo() {
    std::printf("\n--- trace/input-schedule demonstration (scaffolding only) ---\n");

    mcs48::TraceRing ring(8);
    mcs48::TraceRing::enabled = false;
    mcs48::TraceEvent ev;
    ev.kind = mcs48::TraceEventKind::Instruction;
    ev.pc = 0x123; ev.opcode = 0x55; ev.a = 0x2A; ev.psw = 0x08;
    ev.machineCycles = 987654;
    ring.Observe(ev);
    std::printf("disabled ring count after Observe: %zu (must be 0)\n", ring.Count());

    mcs48::TraceRing::enabled = true;
    MachineReset();
    const unsigned char prog[] = {0x55, 0x00};
    LoadProgramAt(0, prog, 2);
    for (int i = 0; i < 2; ++i) {
        ev = mcs48::TraceEvent{};
        ev.kind = mcs48::TraceEventKind::Instruction;
        ev.machineCycles = static_cast<std::uint64_t>(master_clk);
        ev.pc = static_cast<std::uint32_t>(pc);
        ev.opcode = rom[pc & 0xfff];
        ev.a = acc; ev.psw = psw; ev.timerValue = itimer;
        ev.prescaler = static_cast<std::uint8_t>(master_count & 0xFF);
        ev.timerMode = static_cast<std::uint8_t>((timer_on ? 1 : 0) | (count_on ? 2 : 0));
        ev.irqFlags = static_cast<std::uint8_t>((t_flag ? 1 : 0) | (xirq_en ? 2 : 0) |
                                                (tirq_en ? 4 : 0) | (xirq_pend ? 8 : 0) |
                                                (tirq_pend ? 16 : 0) | (irq_ex ? 32 : 0) |
                                                (f0 ? 64 : 0) | (f1 ? 128 : 0));
        ev.regBank = reg_pnt;
        ev.p1Latch = p1; ev.p2Latch = p2;
        ring.Observe(ev);
        Step();
    }
    mcs48::TraceRing::enabled = false;
    std::printf("enabled ring captured %zu records; last: seq=%llu cyc=%llu pc=%03X op=%02X\n",
                ring.Count(),
                (unsigned long long)ring.At(ring.Count() - 1).seq,
                (unsigned long long)ring.At(ring.Count() - 1).machineCycles,
                (unsigned)ring.At(ring.Count() - 1).pc,
                (unsigned)ring.At(ring.Count() - 1).opcode);

    mcs48::InputSchedule schedule;
    schedule.Add(100, 0, mcs48::InputAction::KeyDown);   // e.g. key 0 press
    schedule.Add(2500, 0, mcs48::InputAction::KeyUp);    // e.g. key 0 release
    mcs48::ScheduledInput fired, a, b;
    const bool noneBefore = !schedule.PopDue(99, fired);
    const bool press = schedule.PopDue(100, a) && a.key == 0 && a.action == mcs48::InputAction::KeyDown;
    const bool noneBetween = !schedule.PopDue(2499, fired);
    const bool release = schedule.PopDue(2500, b) && b.action == mcs48::InputAction::KeyUp;
    std::printf("schedule: none@99=%d press@100=%d none@2499=%d release@2500=%d (all must be 1)\n",
                 noneBefore ? 1 : 0, press ? 1 : 0, noneBetween ? 1 : 0, release ? 1 : 0);
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    std::printf("MCS48-NG Phase 2A conformance harness\n");
    std::printf("core: production cpu.cpp (unmodified), stub machine environment\n\n");

    TestPcIncrement0x07FF();
    TestPcIncrement0x0FFF();
    TestOrlBusImmediate();
    TestAnlBusImmediate();
    TestTimerDivideCadence();
    TestStrtTClrPrescaler();
    TestStrtTStrtCntExclusive();
    TestResetF1();
    TestResetTimerFlag();
    TestResetPrescaler();
    TestIrqEntryCycles();
    TestRetDuringIrqBank();
    TestAddcImmediateExhaustive();
    TestAddcRegisterExhaustive();
    TestBranchPageControl();

    if (argc > 1 && std::strcmp(argv[1], "--trace-demo") == 0) {
        TraceAndScheduleDemo();
    }

    std::printf("\n--- summary ---\n");
    std::printf("positive controls passed:      %d/%d\n", g_controlsPassed, g_controlsRun);
    std::printf("legacy failures confirmed:     %d/%d\n", g_knownFailuresConfirmed, g_knownFailuresRun);
    if (g_sanityFailures > 0) {
        std::printf("SANITY FAILURES:               %d (results not trustworthy)\n", g_sanityFailures);
        return 2;
    }
    if (g_suiteDrift) {
        std::printf("RESULT: DRIFT from the Phase 1 audited baseline.\n");
        return 1;
    }
    std::printf("RESULT: GREEN - legacy baseline pinned (known defects present, controls intact).\n");
    std::printf("MCS48-NG must later flip the EXPECTED_LEGACY_FAILURE labels to pass.\n");
    return 0;
}
