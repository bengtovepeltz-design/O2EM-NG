# MCS48-NG Phase 2A: test and trace scaffolding

Date: 2026-09-20. Scope: infrastructure, tests and interfaces only.
The legacy `cpu.cpp` remains the active production CPU, byte-identical
(`git diff cpu.cpp cpu.h` is empty). No opcode implementation, no timing
change, no behavior change.

## 1. What was built

| Deliverable | Location | Notes |
|---|---|---|
| Conformance harness | `tests/mcs48/` | Standalone exe; links the real `cpu.cpp` unmodified |
| Stub machine environment | `tests/mcs48/mcs48_stub_machine.cpp` | Inert definitions of every machine symbol cpu.cpp references |
| Conformance suite | `tests/mcs48/mcs48_conformance.cpp` | 12 legacy expected-failure fixtures + 4 positive controls |
| Standalone project | `tests/mcs48/mcs48_conformance.vcxproj` | x64 Debug and Release; never built by, or part of, the emulator |
| Trace record format | `src/mcs48/trace_record.h` | Bounded ring, disabled by default, non-mutating |
| CPU adapter contract | `src/mcs48/cpu_adapter.h/.cpp` | ICpu + LegacyCpuAdapter + Mcs48NgAdapter skeleton |
| Input schedule | `src/mcs48/input_schedule.h` | Emulated-timestamp input events; pure data, no frontend wiring |

Visual Studio membership: the main `O2EM-NG.vcxproj` gained only the four
new `src/mcs48` files (one compiled, three headers). No existing entry was
touched. `cpu_adapter.cpp` compiles into the emulator but nothing calls it;
it contains no global constructors and no side effects.

## 2. How the harness executes the real core

The suite sets `app_data.debug = 1`, the core's existing single-instruction
exit, so each `cpu_exec()` call runs exactly one instruction plus the
production scheduler tail (clocks, pending IRQs, h_clk wrap, prescaler,
VBL checks). Program bytes are written into a stub-owned 4 KiB `rom`;
registers are preset through the public `cpu.h` globals. `app_data.crc`
is zero so no CRC-gated production branch can fire. No frontend, no
window, no audio, no user data, no ROM or BIOS files.

Suite semantics are pinned in the harness header comment:

- `PASS_CONTROL` tests must pass against legacy today.
- `EXPECTED_LEGACY_FAILURE` tests must fail against legacy today; they
  document Phase 1 defects and define the acceptance bar for MCS48-NG.
- Exit codes: 0 green, 1 drift from the audited baseline, 2 sanity failure.

Do NOT edit `cpu.cpp` to turn expected failures green. MCS48-NG will flip
the labels instead.

## 3. Harness results (Debug and Release x64, identical)

| Test | Label | Result |
|---|---|---|
| pc_increment_07ff | EXPECTED_LEGACY_FAILURE | confirmed (PC=800, ref 000) |
| pc_increment_0fff | EXPECTED_LEGACY_FAILURE | confirmed (PC=1000, ref 800) |
| orl_bus_immediate_88 | EXPECTED_LEGACY_FAILURE | confirmed (PC=001, ref 002) |
| anl_bus_immediate_98 | EXPECTED_LEGACY_FAILURE | confirmed (PC=001, ref 002) |
| timer_divide_32_cadence | EXPECTED_LEGACY_FAILURE | confirmed (31,62,93 vs 31,63,95 after STRT T) |
| strt_t_clears_prescaler | EXPECTED_LEGACY_FAILURE | confirmed (presc=18 inc@15 vs presc=1 inc@32) |
| strt_t_cnt_exclusive | EXPECTED_LEGACY_FAILURE | confirmed (T=1 CNT=1) |
| stop_tcnt_clears_both | PASS_CONTROL | passed |
| reset_clears_f1 | EXPECTED_LEGACY_FAILURE | confirmed (F1=1) |
| reset_clears_timer_flag | EXPECTED_LEGACY_FAILURE | confirmed (TF=1) |
| reset_clears_prescaler | EXPECTED_LEGACY_FAILURE | confirmed (presc=17) |
| irq_entry_cycle_accounting | EXPECTED_LEGACY_FAILURE | confirmed (machine=1 clk=3, ref machine=3) |
| ret_bank_during_irq | EXPECTED_LEGACY_FAILURE | confirmed (PC=834, ref 034) |
| addc_immediate_exhaustive | PASS_CONTROL | passed (0/131072 mismatches) |
| addc_register_exhaustive | PASS_CONTROL | passed (0/131072 mismatches) |
| branch_page_jc_1ff | PASS_CONTROL | passed (PC=245) |

Cadence numbers are measured in machine cycles after STRT T's own cycle;
the absolute Phase 1 convention (32, 63, 94 legacy / 32, 64, 96 reference)
includes it. RET fixture: stacked return address 0x834 with stack high byte
0x08, `sp=12`, simulated in-service `irq_ex=1`; RET must pop two bytes and
NOT exit service (sanity-checked inside the test).

## 4. Trace record design

`mcs48::TraceEvent` is a compact POD: monotonic sequence number, absolute
emulated machine-cycle timestamp, pre-instruction PC, opcode, A, PSW,
timer value, prescaler, timer/counter mode bits, TF / xirq_en / tirq_en /
xirq_pend / tirq_pend / in-service / F0 / F1 packed flags, register-bank
pointer, P1/P2 latches, T0/T1 pins. Event kinds cover Instruction,
ProgRead, MovxRead/MovxWrite, BusWrite, P1Write, P2Write, IrqTransition,
VdcRead/VdcWrite, with address/value payload fields.

`mcs48::TraceRing` is a fixed-capacity ring (default 4096) with snapshot
iteration; overflow overwrites the oldest record. Master switch
`TraceRing::enabled` is false by default; observing costs one branch when
disabled. There are no locks, no allocation on the hot path, no wall-clock
reads anywhere: traces are byte-comparable across runs, suitable for
"stop at first divergence" work. Observation never mutates emulation
state. A `--trace-demo` harness flag demonstrates the format end to end.

## 5. Deterministic input scheduling

`mcs48::InputSchedule` stores at most 256 `ScheduledInput` events keyed by
ABSOLUTE emulated machine cycles (never wall-clock time), with sorted
insertion, `PeekNext` and `PopDue(machineCycles)`. It is pure data: the
frontend does not consume it yet. The smallest future hook is a single
call site in the machine loop that, once per frame or per scheduler tick,
drains events due at the current machine time into the existing key
latch/pin paths; that wiring is deliberately left to the later Work
session. No VP61-specific behavior, no CRC checks.

## 6. Adapter contract

`mcs48::ICpu` declares Init / ExtIrq / TimIrq / ReadPc.
`LegacyCpuAdapter` forwards to the existing global functions with zero
semantic change; it is compiled but unreferenced, so legacy remains the
default (there is no selection point anywhere). `Mcs48NgAdapter` holds a
placeholder `State` and returns NotImplemented; it contains no opcode
engine. There is deliberately no `Step()` on the shared interface: legacy
has no owned single-step entry point, and its app_data.debug path belongs
to production configuration, not to an adapter.

## 7. Snapshot / state inventory (documented, not implemented)

Current `savestate()`/`loadstate()` (vmachine.cpp:1010) saves: app_data.crc,
app_data.bios, VDCwrite[256], extRAM[256], intRAM[64], pc, sp, bs, p1, p2,
ac, cy, f0, A11, A11ff, timer_on, count_on, reg_pnt, tirq_en, xirq_en,
irq_ex, xirq_pend, tirq_pend.

A deterministic CPU/machine checkpoint will eventually additionally need
(currently NOT saved - these are the gaps):

- CPU: `acc` (accumulator), `f1`, `psw`, `lastpc`, `clk`
- Timer: `itimer` (timer/counter value), `t_flag`, `master_count` (prescaler), `int_clk` (synthetic /INT pulse)
- Machine clocks: `master_clk`, `h_clk`, `clk_counter`, `evblclk`, `mstate`, `frame`, `last_line`
- Machine/misc: `enahirq`, `pendirq`, `useforen`, `regionoff`, `megarom`/banking allocations, `key2` keyboard latch state
- VDC: internal raster/beam/collision/status state beyond the 256 register bytes
- Audio: shift/divider/pending-write state in `audio_sdl.cpp`, output ring position
- C7010: NSC800 CPU state, communication latches, trace arming state
- VPP/G7400 and XROM/MegaCART expansion state as applicable

Phase 1's conclusion stands, now verified line by line: current
save/load cannot serve as a deterministic differential checkpoint. The
production save-state format was NOT expanded in this task.

## 8. What was NOT done (by design)

- No MCS48-NG opcodes, no CPU timing changes, no fixes to cpu.cpp.
- No VDC, audio, C7010, XROM, controller or frontend changes.
- No automatic input integration, no frontend hook.
- No save-state format changes.
- No CRC compatibility hacks, no VP48/VP59/VP61-specific work.
- No Project.md/CHANGELOG.md rewrite; this note is the documentation.
- No commits, pushes or other Git state changes.

## 9. Handoff to the later Work session

1. Flip the 12 `EXPECTED_LEGACY_FAILURE` labels to expectations for the
   MCS48-NG core; extend to full-opcode fixtures (Stage 3 gate).
2. Wire `mcs48::TraceRing` observation points into the new core's
   instruction/bus/IRQ paths; keep disabled-by-default semantics.
3. Wire `mcs48::InputSchedule` into a machine-level drain point and define
   the VP61 fixture (reset, Select Game, key 0 press/release at fixed
   machine cycles with pinned ROM/BIOS hashes).
4. Design the complete snapshot format from section 7 before any
   differential capture that needs mid-execution checkpoints.
5. Keep `LegacyCpuAdapter` forwarding untouched; introduce a development-
   internal selector only when MCS48-NG passes the harness.

## 10. Build results

- Main `O2EM-NG.vcxproj` Debug x64: 0 errors, 0 warnings.
- Main `O2EM-NG.vcxproj` Release x64 (`-p:SkipReleasePackaging=true`):
  0 errors, 0 warnings.
- `tests/mcs48/mcs48_conformance.vcxproj` Debug and Release x64: 0 errors,
  0 warnings; suite GREEN (4/4 controls, 12/12 legacy failures confirmed)
  in both configurations.
