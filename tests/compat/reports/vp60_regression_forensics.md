# VP60 / Trans American Rally — historical regression forensics

## A. Last known GOOD VP60 baseline

| Item | Value |
|---|---|
| Git tag | `v0.31.0-beta` |
| Commit | `7e2250593c1fbfaa98f7220e2af10d9f29c51b87` |
| Date | 2026-09-13 20:14:55 +0200 |
| Archived release exe | `dist/O2EM-NG-v0.31.0-beta.previous-20260917-203254-089/O2EM-NG.exe` (2026-09-17 19:21, 675,840 bytes) |
| Core files at GOOD | `cpu.cpp`, `vmachine.cpp`, `vdc_stub.cpp`, `vpp_sdl.cpp`, `vpp_cset.cpp`, `emulator_core.cpp` |

The GOOD runtime was reproduced by extracting the tag to a temp directory and
building it with only a headless input/frame-dump harness added **in the temp
copy**. The core emulation files are unmodified `v0.31.0-beta`.

The tag predates the uncommitted Checkpoint A/B video-timebase work in the
current tree.

## B. Exact gameplay input sequence

```text
ROM:        vp_60.bin (16 KiB, CRC 0D2D721D)
BIOS:       G7400.bin (CRC E20A9F41, VPP enabled)
Region:     PAL
Boot:       G7400 BIOS auto-starts the cartridge (no SELECT GAME)
Input:      KEY_0 (O2EM key code 27) for 4 frames at frame 550
Checkpoints: 650, 675, 700, 750, 800, 900, 1000, 1200
```

The manual's sequence is “press RESET, then `0`”. The G7400 BIOS boot
approximates the RESET step; `0` then selects the race.

GOOD result: US map → transition → full racing stage at frame 1200
(sky, desert, road, car, speedometer, tachometer, fuel gauge, score 0309).

CURRENT result: US map → missing bottom/car by frame 900 → black at 1000/1200.

## C. First frame/state divergence

| Checkpoint | GOOD | CURRENT |
|---|---|---|
| Final frame 550–642 | byte-identical | byte-identical |
| First VDC `A3` write | frame 643, mclk 4504, dat 36 | frame 642, mclk 5048, dat 36 |
| VDC framebuffer (`vscreen`) | diverges at 643/644 | diverges at 643/644 |
| Frame 900 | map + car, 4067 bytes | map shifted up, car missing, 3366 bytes |
| Frame 1000 | road/sky band, 1882 bytes | black, 459 bytes |
| Frame 1200 | full gameplay, 4452 bytes | black, 459 bytes |

The first causal divergence is at the **VBL/IRQ event around frame 642–643**,
not at the VPP compositor.

## D. GOOD CPU/video/VPP snapshot (frame 1000)

| Item | Value |
|---|---|
| VDC pre-compose hash | `36692B5D08B61F93` |
| VPP rebuilt layer hash | `3E3E5E2AFCEFB0A4` |
| VPP registers | `trans=F7 lum=85 mode=C1 dirty=1` |
| VPP `gMem` hash | `F11F5FF1A5D693F9` |
| VPP `gDynamicChars` hash | `5855B7D6AB1910DF` |
| `A3` writes 600–1000 | 643/4504/36, 922/6446/00, 922/7242/06, 971/5588/06 |
| IRQ trace | no vector at mclk 5245; VBL vector at mclk 6154 |

## E. CURRENT CPU/video/VPP snapshot (frame 1000)

| Item | Value |
|---|---|
| VDC pre-compose hash | `59FADCAFC6938D33` |
| VPP rebuilt layer hash | `3E3E5E2AFCEFB0A4` (identical) |
| VPP registers | `trans=F7 lum=85 mode=C1 dirty=1` (identical) |
| VPP `gMem` hash | `F11F5FF1A5D693F9` (identical) |
| VPP `gDynamicChars` hash | `5855B7D6AB1910DF` (identical) |
| `A3` writes 600–1000 | 642/5048/36, 898/7107/00, 898/7903/06; **971/5588/06 missing** |
| IRQ trace | extra `IRQ_VEC` at frame 642, mclk 5245, `pc_before=178`, A=48, PSW=18, P1=9E, P2=E0, before VBL 6154 |

## F. Classification

**CASE B — upstream timing / IRQ delivery**, not a VPP/EF9340 renderer or
composition defect.

Proof:

- VPP registers, `gMem`, `gDynamicChars` and the rebuilt `gLayer` are
  **byte-identical** at frame 1000.
- Only the VDC framebuffer underneath differs.
- The first divergence is an **extra IRQ vector at mclk 5245**, before the VBL
  event at mclk 6154.
- The game's later `A3=06` write (frame 971) is missed, so the VDC background
  never receives the racing-stage color; the VPP layer is then composed over a
  black VDC frame.

This is specifically the **VBL IRQ delivery semantics**, which is part of the
frame-boundary/VBL scheduling class.

## G. Exact historical source change most likely responsible

GOOD (`v0.31.0-beta`, `vmachine.cpp` `handle_vbl()`):

```cpp
draw_region();
ext_IRQ();          // 5-cycle /INT pulse, immediate ISR entry
mstate = 1;
```

CURRENT working tree (uncommitted Checkpoint A/B change):

```cpp
// vmachine.cpp
draw_region();
mcs48::observation::Event(13, 1, 0);
// The CPU scheduler asserts the persistent VBL IRQ line at the event
// point (PAL beam crossing / NTSC threshold), not here.
mstate = 1;

// cpu.cpp: VBL crossing
CPU_VBL_IRQ();      // sets g_irqLine = 1
...
// before the next opcode fetch:
if (g_irqLine && xirq_en && !irq_ex)
    ext_IRQ();
```

The change is uncommitted (no commit hash). It replaced the immediate 5-cycle
pulse with a persistent `/INT` level sampled before the next opcode fetch.

Runtime effect: the persistent line is serviced at **frame 642, mclk 5245**,
when the game re-enables interrupts, instead of at the VBL event at **mclk
6154**. The extra ISR entry alters the game's VDC/VPP setup and the `A3=06`
write is skipped.

## H. Controlled experiment proving the cause

A generic, default-off forensic switch was added (not a title fix):

```text
O2EM_I8244_LEGACY_VBL_IRQ=1
```

- `vmachine.cpp::handle_vbl()` calls `ext_IRQ()`.
- `cpu.cpp` skips `CPU_VBL_IRQ()`.

Results:

| Configuration | Frame 1200 | A3 sequence | VDC hash at 1000 |
|---|---|---|---|
| Production default | 459 (black) | missing 971 | `59FADCAFC6938D33` |
| Production + legacy IRQ | 4455 (gameplay) | 4 writes present | gameplay |
| Production + PAL legacy timing + legacy IRQ | 4452 (gameplay) | exactly GOOD | `36692B5D08B61F93` (exact) |
| Unified default | 464 (black) | — | black |
| Unified + legacy IRQ | 4455 (gameplay) | — | gameplay |

With PAL legacy timing + legacy IRQ, the `A3` sequence is exactly
`643/4504/36, 922/6446/00, 922/7242/06, 971/5588/06` and the VDC pre-compose
hash matches GOOD exactly. The VPP layer hash was already identical.

## I. Production vs unified

Both fail identically without the switch, and both recover with the legacy VBL
IRQ. The regression predates and is independent of the unified timebase; the
unified model is not the cause.

## J. Recommended generic fix direction

Do **not** add a VP60 CRC branch, force VPP registers, or restore the old pulse
blindly. The fix must reconcile the persistent VBL `/INT` level with the 8048
sampling/ack semantics:

1. Assert the level at the VBL crossing, but do not let a level that was
   asserted while interrupts were disabled be serviced before the next VBL
   event. The observed early service at mclk 5245 is the failure mode.
2. Re-check the A1 read/ack timing (`CPU_IrqLineAck`) and the 8048 `/INT`
   sampling point; the persistent line may be cleared/qualified too late.
3. If the hardware model cannot be reconciled, add a generic
   “legacy VBL IRQ pulse” compatibility option, then run the full matrix and
   the MCS48 harness to ensure no regressions. It must not be keyed to VP60.

Until the fix is designed and validated, the default current behavior remains
the regression; the switch only reproduces the GOOD behavior.
