# NG-1 — MCS48-NG production CPU

Date: 2026-11-09
Status: standalone CPU core complete and green.
Production wiring: intentionally not yet connected; NG-2 owns the machine bus.

---

## 1. Scope and result

NG-1 delivers a real, owned-state Intel 8048 CPU for `src/core_ng/`:

```text
src/core_ng/cpu/mcs48_ng.cpp
src/core_ng/cpu/mcs48_ng.h
src/core_ng/cpu/mcs48_bus.h
src/core_ng/cpu/mcs48_state.h
```

It is independent of `cpu.cpp`, SDL, the frontend, the VDC, the database and
individual games. It owns all CPU state and talks only to `Mcs48Bus`.

Standalone suite:

```text
tests/mcs48_ng/mcs48_ng_conformance.cpp
tests/mcs48_ng/mcs48_ng_stub_bus.h
tests/mcs48_ng/mcs48_ng_conformance.vcxproj
```

Final NG-1 suite: **667 checks, 0 failures** in Debug and Release x64 (the
extra check is the NG-2 reset-port correction described below).

---

## 2. CPU architecture

`Mcs48Cpu` contains:

- A, 12-bit PC, PSW/SP, F0, F1
- active register bank (RB0/RB1) and 64-byte internal RAM
- MB0/MB1/A11 latch
- P1/P2, BUS latch
- 8-bit timer, 5-bit prescaler, timer/counter mode
- timer flag and timer IRQ pending
- T1 history for counter mode
- external IRQ enabled/pending/in-service, timer IRQ enabled/pending
- T0/T1 input levels
- absolute machine cycles and last instruction PC
- illegal-opcode record

No legacy globals, no SDL, no VDC knowledge.

Execution model:

```text
Step():
  CheckIrqs()              // before the fetch, per hardware/MAME
    - external /INT level + EN I + not in service -> 2-cycle entry, vector 0x003
    - timer overflow + EN TCNTI               -> 2-cycle entry, vector 0x007
  Fetch()
  Execute(opcode)
  Burn(cycles)             // timer/prescaler/T1 + cycle count
  return { total cycles, irq_taken }
```

An IRQ entry and the vector instruction execute in the same `Step()`; the
returned cycle count includes the 2-cycle entry plus the vector instruction.
This matches MAME's `execute_run()` order (check, fetch, execute).

---

## 3. Opcode coverage

| Class | Count |
|---|---:|
| Legal 8048 opcodes | **230** |
| Explicit illegal opcodes | **26** |
| Total | 256 |

Illegal values (recorded, one cycle, never aliased to NOP):

```text
01 06 0B 22 33 38 3B 63 66 73 82 87 8B 9B A2 A6 B7
C0 C1 C2 C3 D6 E0 E1 E2 F3
```

Implemented families:

- MOV A/Rr/@Rr/#n, MOV Rr,A/#n, MOV @Rr,A/#n, MOV A,PSW, MOV PSW,A,
  MOV A,T, MOV T,A
- MOVX A,@R0/@R1, MOVX @R0/@R1,A
- MOVP A,@A, MOVP3 A,@A, JMPP @A
- XCH, XCHD
- ADD/ADDC/ANL/ORL/XRL (A,Rr/@Rr/#n)
- INC/DEC (A/Rr/@Rr)
- CLR/CPL A/C/F0/F1
- DA A, RL, RLC, RR, RRC, SWAP
- JMP, CALL, RET, RETR, DJNZ
- JZ/JNZ/JC/JNC/JF0/JF1/JTF/JT0/JNT0/JT1/JNT1/JB0..JB7/JNI
- IN A,P1/P2, OUTL P1/P2,A, ANL/ORL P1/P2,#n
- INS A,BUS, OUTL BUS,A, ANL/ORL BUS,#n
- MOVD A,P4..P7, MOVD P4..P7,A, ANLD/ORLD P4..P7,A
- ENT0 CLK, STRT T, STRT CNT, STOP TCNT
- EN I, DIS I, EN TCNTI, DIS TCNTI
- SEL RB0/RB1, SEL MB0/MB1, NOP

Illegal policy is explicit and tested: PC advances by one, one cycle is burned,
`illegal_opcode`/`illegal_count` are recorded.

---

## 4. Cycle model

Every legal opcode has an exact machine-cycle count. The NG cycle table was
differentially compared against MAME 0.289 `mcs48.cpp`:

- 256 opcode entries
- 240 MAME handler names
- **0 cycle mismatches**
- **0 legality mismatches**

The suite also checks every legal opcode's actual `Step()` cycles against the
table and every illegal opcode's policy.

Cycle groups follow the 8048 rules:

- 1 cycle: register/accumulator/flag/rotate/timer-control/SEL/NOP
- 2 cycles: immediate, address, branch, CALL/JMP, RET/RETR, MOVX, MOVP,
  MOVD/ANLD/ORLD, port/BUS, JTF/JNI, DJNZ, MOV Rr,#n / MOV @Rr,#n

---

## 5. IRQ model

- `/INT` is a level input through `SetIrqLevel(bool)`.
- The CPU samples it before every fetch (`CheckIrqs`).
- `EN I` enables; `DIS I` disables but does not discard a held level.
- `irq_in_service` suppresses re-entry until `RETR`.
- External entry burns 2 cycles, pushes PC and PSW, sets vector `0x003`.
- Timer overflow entry burns 2 cycles, vector `0x007`, clears timer IRQ pending.
- External IRQ has priority over timer IRQ, matching MAME.
- `AcknowledgeIrq()` is the A1-read/line-clear hook.
- `RET` during an IRQ masks return PC bit 11 (bank 0); `RETR` clears in-service
  and restores PC bit 11 plus PSW/bank from the stack.
- A held level that was asserted while interrupts were disabled is serviced at
  the first eligible fetch after `EN I`. No "ignore until next VBL" rule exists.

IRQ entry cycle test: `EN I` + 2-cycle entry + vector NOP = 4 machine cycles.

---

## 6. Timer model

- 8-bit timer + 5-bit prescaler.
- `STRT T`: timer mode, prescaler cleared, then the instruction's own cycle is
  burned (prescaler is 1 after `STRT T`).
- Cadence: timer increments at 31, 63, 95 cycles measured from after `STRT T`
  (32-cycle divide), matching MAME.
- Overflow sets `timer_flag`; if `EN TCNTI` is active it also sets timer IRQ
  pending.
- `JTF` branches on `timer_flag` and clears it.
- `MOV A,T` / `MOV T,A` read/write the timer.
- `DIS TCNTI` clears timer IRQ enable and pending.
- `STOP TCNT` clears both timer and counter mode.

---

## 7. Counter / T1 model

- `STRT CNT` selects counter mode exclusively and initializes T1 history.
- Each burned machine cycle shifts the T1 pin into `t1_history`.
- A high→low transition (`history & 3 == 2`) increments the timer.
- Overflow follows the same timer flag/IRQ rules.
- `JT1`/`JNT1` use the CPU's `SetT1` input.
- Tests drive high/high, high/low, low/low and repeated edges.

`T0` is an input for `JT0`/`JNT0`; `ENT0 CLK` enables the T0 clock output
through `Mcs48Bus::T0Clock(true)`. There is no documented 8048 disable for
the T0 clock output; this is recorded as a limitation.

---

## 8. BUS / P1 / P2 / MOVX / 8243

- `MOVX` uses the active bank's R0/R1 as an 8-bit external address.
- `INS A,BUS` / `OUTL BUS,A` use the bus byte interface.
- `ANL/ORL BUS,#n` read, combine, and write the bus.
- `IN A,P1/P2` masks the external pin value with the port latch.
- `OUTL P1/P2,A` and `ANL/ORL P1/P2,#n` update the latch and bus output.
- `MOVD/ANLD/ORLD` use `Mcs48Bus::ExpanderRead/Write` as the 8243 transaction
  boundary. `ProgWrite` is provided for a future pin-level 8243 model; the
  current CPU uses the logical expander transaction.

---

## 9. Stack / PSW / banks

- Stack is 8 levels in internal RAM 8..23.
- `Push` stores PC low, then PC high nibble + PSW upper nibble; SP wraps 3-bit.
- `RET` pops PC; during in-service it masks PC bit 11 (bank 0).
- `RETR` clears in-service, pops PC+PSW, restores BS/bank and PC bit 11.
- `MOV A,PSW` returns `psw | 0x08 | SP`.
- `MOV PSW,A` restores flags/BS/SP and updates the active register bank.
- RB0/RB1 map R0-R7 to RAM 0..7 / 24..31.
- MB0/MB1 set the A11 latch used by JMP/CALL; fetch wraps the low 11 bits and
  preserves the current PC bit 11.
- Stack depth 0..7 tested for CALL/RET and IRQ/RETR.

---

## 10. Reset model

`PowerOn()` zeroes the whole owned state and calls `Reset()`.

`Reset()`:

- PC=0, A=0, PSW=0, SP=0, bank 0
- A11/MB=0
- timer=0, prescaler=0, timer mode=0, timer flag=0, T1 history=0
- P1=0xFF, P2=0xFF (quasi-bidirectional input mode), BUS latch=0
- F0=0, F1=0
- IRQ enable/pending/in-service=0, timer IRQ enable/pending=0
- RAM is **preserved** (hardware RESET does not clear internal RAM)
- cycles are preserved (machine time continues); `PowerOn` sets cycles=0

MAME 0.289 and the MCS-48 reset description put P1/P2 in input mode (0xFF).
NG-1 originally used 0; NG-2 real-BIOS execution showed that this prevented
the cartridge/BIOS from seeing the correct reset port state. The reset value
was corrected to 0xFF and the NG-1 reset test now asserts it. No other NG-1
CPU semantics changed.

---

## 11. Save state

`SaveState(std::vector<uint8_t>&)` copies the complete trivially-copyable
`Mcs48State`, including registers, RAM, stack, timer/prescaler, flags, IRQ,
banks, ports, T0/T1 and cycle count. `LoadState` validates the size.

Test: run 5 instructions, snapshot, run 40, snapshot; restore and replay 40;
the complete state and cycle count match exactly.

---

## 12. Trace

The suite records a deterministic `mcs48::TraceEvent` ring before each step:
cycle, PC, opcode, A, PSW, timer, prescaler, timer mode, bank, P1/P2. The ring
is disabled by default; enabling it never changes CPU state. The test verifies
count, first PC/cycle and monotonic timestamps.

---

## 13. Conformance results

| Check | Result |
|---|---|
| NG standalone suite | **667 checks, 0 failures** |
| Debug x64 | GREEN |
| Release x64 | GREEN |
| Legal opcode cycle table | 230/230 pass |
| Illegal opcode policy | 26/26 recorded |
| MAME cycle differential | 0/256 mismatches |
| MAME legality differential | 0/256 mismatches |
| ADDC immediate exhaustive | 131,072 combinations, 0 mismatches |
| ADDC register exhaustive | 131,072 combinations, 0 mismatches |
| ADD immediate exhaustive | 65,536 combinations, 0 mismatches |
| Stack depths | 0..7 CALL/RET and IRQ/RETR pass |
| Save/restore | deterministic replay pass |
| Random instruction invariants | 4,096 deterministic opcodes, no invalid PC/SP/cycles |

Legacy reference harness remains:

```text
positive controls passed: 5/5
legacy failures confirmed: 11/11
RESULT: GREEN
```

---

## 14. Former legacy failures → NG results

| Legacy failure | NG equivalent | NG result |
|---|---|---|
| pc_increment_07ff | TestPcWrap | PASS |
| pc_increment_0fff | TestPcWrap | PASS |
| orl_bus_immediate_88 | TestBusImmediate | PASS |
| anl_bus_immediate_98 | TestBusImmediate | PASS |
| timer_divide_32_cadence | TestTimerCadence | PASS |
| strt_t_clears_prescaler | TestTimerPrescalerReset | PASS |
| strt_t_cnt_exclusive | TestTimerCounterExclusive | PASS |
| reset_clears_f1 | TestReset | PASS |
| reset_clears_timer_flag | TestReset | PASS |
| reset_clears_prescaler | TestReset | PASS |
| irq_entry_cycle_accounting | TestIrqEntry | PASS (4 cycles) |
| ret_bank_during_irq | TestRetRetrBank | PASS |

---

## 15. MAME differential status

Source-level differential is complete for opcode legality and cycle counts:
MAME 0.289 `mcs48.cpp` opcode table vs NG `ExpectedCycles`/`IsIllegal` = zero
mismatches.

Runtime MAME CPU fixtures were not run in NG-1 because:

- MAME does not expose a standalone 8048 CPU harness in this environment.
- The G7400 reference environment still lacks the EF9340 charset ROM.
- A full MAME driver run would not isolate CPU semantics from VDC/BIOS.

Deterministic NG fixtures for arithmetic, flags, stack, banks, branches, IRQ,
timer, counter, MOVX and ports are embedded in the suite. Runtime MAME
differential moves to NG-2 when the machine bus exists.

---

## 16. Unresolved ambiguities / limitations

1. **8243 pin-level PROG** is abstracted through `ExpanderRead/Write`. The
   logical MOVD/ANLD/ORLD behavior is implemented; a pin-level P2/PROG timing
   model is deferred.
2. **ENT0 CLK** enables the T0 clock output; no documented disable path is
   modeled.
3. **RESET RAM preservation** follows hardware; `PowerOn` clears RAM.
4. **RET during an IRQ masks PC bit 11** (MAME and the legacy reference). The
   stack stores PC bit 11; RETR restores it. This distinction is tested.
5. **MAME timer/IRQ timing notes**: MAME documents known timer/IRQ timing
   inaccuracies in mcs48. NG follows the documented 32-cycle cadence and
   before-fetch IRQ sampling; ambiguous sub-cycle details remain for runtime
   differential in NG-2.
6. **Illegal opcodes** are recorded, not executed as NOP. Hardware behavior for
   undefined encodings is not guaranteed; the policy is explicit.

---

## 17. References and provenance

- Intel MCS-48 Family User's Manual / 8048 instruction set (primary).
- Intel 8243 I/O expander documentation (logical expander behavior).
- MAME 0.289 `src/devices/cpu/mcs48/mcs48.cpp` and `mcs48.h`
  (BSD-3-Clause), used as behavioral/source reference for opcode semantics,
  legality and cycle counts. No verbatim file copy; implementation is
  independently written against this contract.
- O2EM legacy `cpu.cpp` and the existing `tests/mcs48` harness: historical
  evidence only, not correctness authority.

Provenance is also recorded in `Docs/Development/NG-0_Provenance.md`.

---

## 17a. NG-2 reset addendum (2026-11-09)

NG-2 found one real reset semantic defect while executing the real G7000
BIOS: the MCS-48 RESET state must place P1/P2 in quasi-bidirectional input
mode (0xFF), not 0. The CPU reset code and test were corrected. This is the
only NG-1 change made during NG-2. The standalone suite remains green:
**667 checks, 0 failures** in Debug and Release x64.

## 18. NG-2 recommendation

Proceed to NG-2 with the CPU unchanged:

1. Implement `MachineBus` (BIOS overlay, cartridge, external RAM, open bus,
   P1/P2 decode, banking/MegaCART) around `Mcs48Bus`.
2. Implement the machine scheduler/timebase and `Machine` lifecycle.
3. Load a real BIOS/cartridge and run deterministic ROM execution with no
   video device (stub VDC).
4. Add timestamped bus/IRQ trace replay and compare small deterministic ROM
   programs against MAME where feasible.
5. Do not start i8244/i8245 rendering until NG-3.
