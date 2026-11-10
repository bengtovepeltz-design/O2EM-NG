# O2EM-NG architecture audit — Checkpoint A

This document is the reference for the architecture reset. It is deliberately
written before any further core change.

Reference priority used throughout:

1. real G7000/G7400 evidence
2. Intel 8244/8245, Philips/Signetics/Thomson documentation
3. MAME 0.289 implementation and behavior
4. Manopac/O2EM historical sources (provenance only)
5. legacy O2EM-NG behavior (last)

## 1. MAME 0.289 time and data flow

```text
attotime scheduler
  └─ mcs48 CPU device (execute_run)
       ├─ check_irqs() before each opcode fetch
       ├─ burn_cycles(n) per instruction
       ├─ timer/counter and T0/T1 inputs
       └─ IRQ line (persistent level)
  └─ i8244/i8245 device
       ├─ hblank_timer / vblank_timer scheduled with screen().time_until_pos()
       ├─ screen().hpos() / vpos() = single authoritative beam position
       ├─ vblank() / hblank()            = T1 source
       ├─ video register write           = screen().update_now() first
       └─ screen_update(scanline)
            ├─ grid / major / minor / collision / priority per scanline
            └─ object/character output at the current beam line
  └─ G7400/VPP (EF9340/EF9341) as separate devices with their own clock/phase
```

Key property: **one beam position** (`screen().hpos/vpos()`). CPU, T1, IRQ,
A4/A5/A1 status, `update_now()` and the raster renderer all consume that one
position.

## 2. O2EM-NG current time and data flow

```text
cpu_exec() instruction loop
  ├─ clk = instruction machine cycles
  ├─ consume IRQ-entry pending cycles (2-cycle taken-IRQ cost)
  ├─ sample persistent /INT before fetch; PAL T1 boundary sample
  ├─ execute opcode
  ├─ master_clk += clk                  (FRAME-LOCAL; reset by handle_evbl)
  ├─ PAL: g_videoPhasePixels += clk*18 mod 142728   (CONTINUOUS, separate)
  ├─ h_clk += clk; LINECNT wrap        (legacy line counter, NTSC/legacy)
  ├─ timer/counter, pending IRQs
  ├─ PAL: VBL if phase crosses 242*456+365
  └─ NTSC: VBL if master_clk > VBLCLK
handle_evbl()
  ├─ master_clk -= VM_EffectiveFrameClk()   (7929/7930 PAL, evblclk NTSC)
  ├─ VM_RasterTimingFrameBoundary()
  └─ frame++, finish_display()
```

Renderer:

```text
ext_write()
  ├─ corrected renderer: flush before A0/A2/A3 changes
  └─ draw_region()
       ├─ PAL corrected: i = round(master_clk*18/456) - palRenderOffset
       ├─ C7010: C7010 raster path (corrected beam or legacy /22 - 5)
       ├─ regionoff titles: corrected beam + regionoff (or legacy /22 + regionoff)
       ├─ CRC titles: legacy /22 /24 /20 branches (legacy modes only)
       ├─ clip [last_line, i)
       └─ draw_display(): background, grid, A0 gate, chars, quads, sprites
finish_display(): vpp_compose(), SDL texture
```

Derived beam/status functions today:

| Consumer | Current derivation |
|---|---|
| PAL T1 | `g_videoPhasePixels` + `(beamMaster-master_clk)*18`, PAL vblank/hblank |
| PAL VBL event | continuous phase crossing pixel 110717 |
| NTSC VBL/T1 | `master_clk > VBLCLK`, `h_clk > 16`, `h_clk` wrap `LINECNT` |
| A4 (8244 0xA4) | `VM_PalA4BeamValue()` = `floor(master*18/456)` |
| A1 hstatus | `floor(master*18/456)` + PAL bgate/vblank formula |
| Renderer line | `round((master*18+228)/456) - palRenderOffset` (+ per-title paths) |
| A3/audio beam | `RenderBeamLine()` = `floor(master*18/456)` |
| `snapline` | per-title de-jitter around the candidate line |

**This is the architectural fault line:** O2EM-NG has several independent
answers to "where is the beam?", and the renderer adds title offsets on top.

## 3. Ranked architectural mismatches

1. **No single authoritative timebase.**
   Frame-local `master_clk`, continuous `g_videoPhasePixels`, legacy `h_clk` /
   `LINECNT`, and renderer-specific line math coexist. The corrected renderer
   uses round(), A4/A1 use floor(), the C7010/regionoff/CRC paths add their own
   scales/offsets. This is the root cause class behind the C7010 missing
   bottom half, the Samurai missing rows, the VP15 top-label loss and the VP16
   sky-band loss.

2. **Beam-derived status and raster output are not the same clock.**
   MAME's `screen().hpos()/vpos()` feeds T1, VBL, A4/A5/A1 and `update_now()`.
   O2EM-NG computes them with different formulas and offsets. A write can be
   drawn on one line while the A1/A4 status reports another.

3. **Legacy per-title raster instrumentation is still present as a class.**
   `regionoff`, `/20`, `/22`, `/24`, `palRenderOffset`, CRC-specific branches,
   `snapline`/`mxsnap` and fixed scanline offsets. These were calibrated for
   the old frame-local/20-22-cycle model. They are still reachable and still
   shape the corrected path for several titles.

4. **NTSC and PAL are two different machines in the core.**
   PAL now has the phase/beam model; NTSC still uses `VBLCLK`, `LINECNT`,
   `h_clk` and the old T1/hstatus. MAME parameterizes one geometry for both.

5. **G7400/VPP is not covered by the same differential.**
   VPP compose/luminance and the G7400 BIOS path have their own timing and
   palette behaviour. The local MAME set lacks the G7400 BIOS/EF9340 charset,
   so `+` titles are UNKNOWN in the matrix. The manual frontend currently
   selects the G7400 BIOS, so the VP48 manual image differs from the G7000
   reference by the VPP palette/tint (deferred palette item).

6. **C7010/NSC800 has its own raster and interleave model.**
   The C7010 character multiplex path uses `/22 - 5` (legacy) or the corrected
   beam, and the NSC800 is interleaved per 8048 cycles rather than scheduled
   from the same source as the i8244 beam.

7. **Mapper/IRQ compatibility flags remain.**
   `pendirq`, `enahirq`, `useforen`, `sproff`, `mxsnap`, `tweakedaudio`,
   CRC-specific banking and reset hacks are still per-title switches, not
   hardware-derived state.

8. **Palette and audio are separate and deferred.**
   The 16-colour palette, VPP luminance and the VDC audio path are visibly
   different from MAME/real hardware. They are explicitly out of scope for the
   timing architecture pass.

## 4. Recommended first replacement

Replace the **legacy raster/timebase class**, not an individual game.

Target model:

```text
one PAL/NTSC video phase (pixels)
  ├─ beamLine()/hpos()/vpos()
  ├─ vblank()/hblank() → T1
  ├─ VBL event → persistent IRQ line
  ├─ A4/A5/A1 status
  └─ draw_region/update_now() line
```

Then delete, as a class:

- `/20`, `/22`, `/24` raster scales
- `regionoff` line-rate compensation
- CRC-specific raster offsets (keep only proven fixed hardware offsets, if any)
- `palRenderOffset` as a raster fudge
- independent renderer/beam round-vs-floor math

Keep legacy behaviour only behind the existing A/B switches while the matrix
proves parity. After that, VP60 becomes a probe of the unified timebase rather
than a target.

## 5. What must not be mixed into this pass

- palette correction
- VPP luminance/colour correction
- audio pitch/RF correction
- frontend layout/database/version changes
