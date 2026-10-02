# MCS48-NG Phase 2C prep: PAL VBL timing divergence and probe

Date: 2026-09-21. Scope: **test/diagnostic infrastructure only.**
No emulator timing behavior was changed. This note records the verified
Phase 2B VBL finding, the new development-only probe that reproduces it, and
the proposed (not yet implemented) Phase 2C functional change.

## 1. Verified Phase 2B finding

The first proven IRQ divergence between O2EM-NG and MAME is **VBL phase/timing**:

| Observation | Machine cycle |
|---|---|
| O2EM-NG first VBL IRQ | **5494** |
| O2EM-NG first vector instruction | **5494** (PC=003) |
| MAME 0.289 PAL first VBL | ~**7924** (normalized) |
| MAME 0.289 PAL first vector instruction | ~**7926** |

- O2EM-NG's first VBL is **2430 cycles earlier** than the MAME 0.289 PAL
  reference.
- The MAME "IRQ at 1199" hypothesis was tested and **rejected**; it is not
  used as a reference anywhere.
- The legacy core reaches the vector at the same cycle as the VBL event
  (no two IRQ-entry cycles are charged), which is the same defect tracked by
  the conformance fixture `irq_entry_cycle_accounting` (legacy machine=1 vs
  reference machine=3).

## 2. Current PAL timing constants and scheduling path

Constants (`vmachine.h`):

| Constant | Value | Role |
|---|---|---|
| `VBLCLK` | 5493 | Fixed threshold for the **first** VBL from reset (`mstate == 0`) |
| `EVBLCLK_PAL` | 7259 | PAL cycles per frame (periodic `handle_evbl` threshold) |
| `EVBLCLK_NTSC` | 5964 | NTSC cycles per frame |
| `FPS_PAL` / `FPS_NTSC` | 50 / 60 | Frame rates |
| `LINECNT` | 21 | Horizontal clock divisor |

Scheduling path:

1. `cpu.cpp`, scheduler tail (per instruction): `master_clk += clk`, then
   `if ((mstate==0) && (master_clk > VBLCLK)) handle_vbl();`
2. `handle_vbl()` (`vmachine.cpp`) fires the observation
   `Event(13, 1, 0)` and calls `ext_IRQ()`, then sets `mstate = 1`.
3. `if ((mstate==1) && (master_clk > evblclk)) handle_evbl();`
   `handle_evbl()` subtracts `evblclk` from `master_clk`, advances the frame
   and sets `mstate = 0`.

Consequence: the **first** VBL is gated by the fixed `VBLCLK` (5493), while
every subsequent frame is gated by `evblclk` (PAL 7259). The first VBL phase
is therefore independent of the active region's frame period.

## 3. New development-only probe

Files:

| File | Role |
|---|---|
| `src/mcs48/vbl_timing_probe.h` | Probe entry-point declaration |
| `src/mcs48/vbl_timing_probe.cpp` | Env-gated measurement implementation |
| `main.cpp` | Single gated call before the frontend starts |
| `O2EM-NG.vcxproj` / `.filters` | Compile-unit membership |

The probe is **disabled by default**: `RunVblTimingProbeIfRequested()` returns
`false` immediately unless `O2EM_VBL_TIMING_PROBE` is set, so normal
production execution is byte-for-byte unchanged.

It boots a clean PAL system through the normal `EmulatorCore_StartRom(..,
RegionMode::PAL, ..)` path and measures using only the existing
null-by-default `mcs48::observation` callbacks:

- `advance` accumulates emulated machine cycles (`g_ticks`);
- `event` records every `kind 13, address 1` event (the VBL IRQ emitted by
  `handle_vbl`) and reads `master_clk`;
- `before` captures the first instruction fetched after the first VBL (the
  IRQ vector);
- `stop` ends the run once the requested number of VBLs is captured.

Observation is read-only; the callbacks write no CPU/machine state.

### How to run it

From the directory containing `O2EM-NG.exe` (so `BIOS` resolves under the
executable path), e.g. `x64\Debug`:

```powershell
$env:O2EM_VBL_TIMING_PROBE = "1"                       # enable (value not "1" also names the CSV)
$env:O2EM_VBL_ROM         = "D:\Project\O2EM-NG\x64\Debug\ROMS\vp_61.bin"
$env:O2EM_VBL_BIOS        = "o2rom.bin"                 # resolved under <exe>\BIOS
$env:O2EM_VBL_FRAMES      = "4"                         # optional, default 4, max 16
$env:O2EM_VBL_TRACE       = "vbl_timing.csv"            # optional CSV
& "D:\Project\O2EM-NG\x64\Debug\O2EM-NG.exe"
```

Exit codes: `0` = measurement completed, `2` = could not run.

## 4. Measured result (Debug x64, 2026-09-21)

ROM `vp_61.bin` (CRC `69D21F8F`), BIOS `o2rom.bin` (CRC `8016A315`), PAL:

```
first VBL IRQ cycle (observed)   : 5494  (master_clk=5494)
first vector instr   (observed)  : cycle 5494, PC=003
VBL cycle samples                : 5494 12753 20012 27271
frame period (observed)          : 7259 cycles
emulator constants               : VBLCLK=5493, EVBLCLK_PAL=7259
reference (MAME 0.289 PAL)       : first VBL ~7924, first vector ~7926
first VBL delta vs reference     : +2430 cycles (earlier than MAME)
frame period delta vs EVBLCLK_PAL: +0 cycles
```

Interpretation: the probe reproduces the Phase 2B finding exactly. The
**frame period** already matches the emulator's PAL constant (7259); the
divergence is the **first VBL phase** (5494 vs ~7924), driven by the fixed
`VBLCLK` threshold.

## 5. Proposed Phase 2C functional change (NOT implemented)

Goal: move the PAL **first VBL phase** to match the reference, without
touching opcode behavior, audio, LINE IRQ, CX, C7010, G7400, XROM or the
existing region-selection logic.

Candidate change (to be pinned before implementation):

1. Make the boot/first-VBL threshold **region-derived** instead of the fixed
   `VBLCLK = 5493`. Concretely, schedule the first VBL relative to the active
   frame period (`evblclk`) so a PAL boot reaches its first VBL at the
   reference phase (~7924 normalized) rather than at a constant ~5494.
2. Optionally account for the two IRQ-entry cycles so the first vector lands
   two cycles after the VBL (reference 7926 vs VBL 7924), which also aligns
   with the existing `irq_entry_cycle_accounting` conformance failure.

Guardrails for that change:

- Touch only the first-VBL threshold/phase source in `vmachine.*` / the
  `cpu.cpp` scheduler check; leave `VBLCLK` semantics for any code that still
  needs a boot threshold.
- Keep the periodic PAL frame period at `EVBLCLK_PAL` (already correct).
- The exact formula must be validated against the reference **using this
  probe**, not guessed; the MAME PAL full-frame period is not part of the
  recorded Phase 2B data, so only the first-VBL phase is actionable now.
- Re-run the MCS48 conformance suite after the change (must remain 4/4
  controls + 12/12 expected legacy failures) and this probe to confirm the new
  first VBL.

No functional timing change was made in this task.
