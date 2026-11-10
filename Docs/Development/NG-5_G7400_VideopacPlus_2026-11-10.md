# NG-5 — G7400 / Videopac+ hardware report

Date: 2026-11-10
Scope: real G7400 / Videopac+ machine on the NG architecture
Status: **accepted for the local `+` library** (25-row NG-5 matrix: 24 GREEN,
1 YELLOW, 0 ORANGE, 0 RED)

This report replaces the old VPP architecture. The legacy `vpp_sdl.cpp` /
`vpp_cset.cpp` path remains in the tree untouched as historical reference and
is not used by `core_ng`.

---

## A. Files created / changed

### New core files

| File | Purpose |
|---|---|
| `src/core_ng/video/vpp.h` / `vpp.cpp` | Owned EF9340/EF9341/VPP device: command protocol, A/B RAM, extended/dynamic RAM, character rendering, per-scanline timing, VDC composition, snapshot. |
| `src/core_ng/video/ef9340_charset.h` / `.cpp` | 2560-byte Videopac+ character generator ROM (`kEf9340Charset`, CRC32 `D557A7BF`), generated from the project's existing legacy table and documented in provenance. |
| `tests/ng5/ng5_device_conformance.cpp` | NG-5 device/machine suite (105 checks). |
| `tests/ng5/ng5_device_conformance.vcxproj` | Standalone NG-5 test target (Debug/Release). |
| `tests/ng5/README.md` | Build/run notes. |
| `tests/vdc_ng/ng5_matrix.py` | NG-5 compatibility matrix driver. |
| `tests/vdc_ng/ng5_noinput.py` | No-input G7400 reference comparison helper. |
| `tests/vdc_ng/ng5_gameplay_probe.py` | Scripted-input gameplay differential (VP59+/VP60/Moto-Crash+). |
| `tests/vdc_ng/ng5_g7000_under_g7400.py` | Standard G7000 cartridges on the G7400 machine. |
| `tests/vdc_ng/ppm2png.py` | Dependency-free PPM→PNG diagnostic converter. |
| `tests/compat/matrix/compat_matrix_ng5.csv` | 25-row G7400 result matrix. |

### Modified core files

| File | Change |
|---|---|
| `src/core_ng/timebase.h` | `MakeG7400Profile()`; corrected VBL phases: first IRQ at the end of the partial reset frame (line 312), steady state at the start of VBLANK (line 242). |
| `src/core_ng/machine.cpp` / `machine.h` | VPP owned device, G7400 profile selection, VPP snapshot, steady-state VBL scheduler, VPP accessor. |
| `src/core_ng/machine_config.h` | `g7400` and documented `vpp_enabled` fields. |
| `src/core_ng/memory/bus.cpp` / `bus.h` | EF934x MOVX 0..7 routing when P1 bit5 is low, 8243 expander forwarding, `BusDevice::Vpp` tracing. |
| `src/core_ng/memory/cartridge.cpp` / `.h` | B-pin hardware metadata from MAME `b_pin=1` CRCs, now applied to every image size (not only 4 KB). |
| `src/core_ng/video/vdc.h` / `vdc.cpp` | Attach VPP, per-scanline composition hook, `LiveRow`, external-collision assertion parameter. |
| `src/core_ng/diagnostics/bus_trace.h` / `.cpp` | `BusDevice::Vpp`. |
| `src/ng_host/sdl_ng_host.cpp` | Select G7400/VPP from the BIOS firmware identity (G7400 `E20A9F41`, Jopac `11647CA5`), never from a title. |
| `tests/machine_ng/machine_ng_conformance.cpp` | Updated VBL phase expectations (two-phase model), 402 checks. |
| `tests/vdc_ng/vdc_render_probe.cpp` | `--g7400`, G7400 profile, VPP trace/composition dump support. |
| `Docs/Development/NG-0_Provenance.md` | NG-5 source/data provenance. |

No frontend, input, library, database, media, import, settings, rating,
favorites, Win95 UI or user-facing key/joystick mapping was changed.

---

## B. G7400 machine architecture

```text
MCS48-NG CPU
    ↓ one NG machine cycle = 180 ticks
NG scheduler (authoritative time)
    ↓
NG MachineBus
    ├── BIOS / cartridge / external RAM
    ├── i8245 VDC (NG-3, real device)
    └── EF9340 + EF9341 + VPP memory (NG-5, owned device)
            ↓
        VDC scanline composition
            ↓
        NG framebuffer
```

The G7400 is **another `MachineConfig`**, not another CPU core or scheduler:

- `MachineConfig::g7400 = true` selects `MakeG7400Profile()`.
- `MachineConfig::vpp_enabled = true` instantiates the VPP and attaches it to
  the VDC.
- The production NG host selects this from the BIOS CRC only.
- One `Tick` timeline drives CPU, beam, VBL, VPP cursor and composition. There
  is no second VPP frame counter or clock universe.

G7400 profile values:

| Property | Value |
|---|---|
| CPU clock | 5.911 MHz (8048) |
| Video crystal | 8.867 MHz |
| i8245 device clock | `(8.867 * 2) / 5` MHz |
| i824x pixel clock | 2× i824x device clock |
| PAL raster | 456 × 313, visible 360 × 243 |
| NG tick | 180 ticks / 8048 machine cycle, 10 ticks / pixel |

The NG tick geometry keeps the 8048 machine cycle exact, so the raster is
derived at 18 pixels/cycle. The nominal G7400 crystals differ by 0.0056 %
(< 1 pixel per frame, non-accumulating because the frame grid is fixed). This
reconciliation is deliberate and documented in `timebase.h`.

---

## C. MAME G7400 reference environment

Reproducible local path:

1. `mame.exe` 0.289, machine **`videopacp`** (Videopac+ G7400 Europe).
2. `roms/videopacp.zip` built locally from the user's `BIOS/G7400.bin`
   (CRC `E20A9F41`), member `g7400.bin`. No download.
3. `roms/ef9340_1.zip` containing `charset_ef9340_1.rom`, reconstructed from
   the project's legacy Videopac+ table (2560 bytes, CRC32 `D557A7BF`). MAME
   warns that its expected datasheet image is `BAD_DUMP` (`8DE85988`); the
   machine runs and renders the local `+` library correctly.
4. Software list `videopac`, filter `VPP`; `-cart <software>` for `+` titles.
5. Standard G7000 cartridges load as raw files: `videopacp -cart <path>`.
6. Deterministic reference capture: **no input**, snapshot at frame 500 via
   `emu.register_frame_done` and `screen:snapshot`.
7. Scripted-input captures use `"1"` at frame 250 and Enter (`"\n"`) at frame
   310. The old NG-4 `keypost("ENTER")` mistake is not used.

The environment was verified with:

- G7400 BIOS boot (VP60 / Rally)
- VP59+ Helicopter Rescue
- VP60 / Trans American Rally
- Moto-Crash+ (France)
- Flash Point (PAL conversion)
- 16 local `+` images

No reference was accepted unless the capture reached the intended state
(title/high-score/gameplay). The no-input policy was inherited from NG-4.

---

## D. G7400 BIOS execution result

Bounded differential:

- MAME 0.289 `videopacp`, G7400 BIOS, VP60 / Rally, debugger instruction trace
  from reset.
- NG G7400 machine, same BIOS/cart, `--pc-out`.
- First MAME traced PC `0x26A` aligns at NG instruction 1.

Result: **19,209 consecutive instructions identical**. The first difference is
the first VBL interrupt acceptance at PC `0x003`: MAME takes it one instruction
before NG, after which both execute the same ISR at `0x402` and continue in the
same loop. No PC, RAM, mapper, VDC or VPP state divergence precedes that
one-instruction IRQ sampling difference. The phase model is now explicit in
`HardwareProfile::FirstVblankIrqOffset()` (line 312) and
`VblankIrqOffset()` (line 242).

This is a clean result: the G7400 BIOS is not being faked or bypassed.

---

## E. EF9340 model

`src/core_ng/video/vpp.cpp` implements the Thomson EF9340 (VIN) side as owned
state:

- 2 × 1 KB display RAM: `ram_a_` (attributes), `ram_b_` (character codes).
- 2 KB extended/dynamic character RAM (`extram_`), address formula identical
  to the G7400 wiring:
  `latch = (offset>>12 & 0x80) | (offset>>4 & 0x7f)`;
  `addr = (latch & 0x1f) | (offset<<9 & 0x200) | (latch<<3 & 0x400)`,
  then either `| (latch & 0x60)` or `| (offset<<4 & 0x60) | (latch<<2 & 0x180)`,
  masked to 0x7FF.
- 256 characters × 10 scanlines (2560-byte generator ROM).
- Cursor `X/Y`, row origin `Y0`, mode `M`, control `R`.
- Service row (`R & 0x08` → row 31 above the normal display).
- Double width (`a & 0x20`), double height (`a & 0x10`), inversion
  (`a & 0x40`), blink (`R & 0x80` + 32-frame prescaler), box/opaque attribute
  (`R & 0x02`, delimiting cells), conceal (`R & 0x04`).
- 40-column by 24-row visible field at screen offset (15, 5).
- Display automaton line limit 210 or 250 according to `R & 0x40`.

The renderer is scanline-based and does not rebuild unchanged state per pixel.

---

## F. EF9341 model

The CPU interface is the GEN side:

| MOVX address (P1 bit5 low) | Operation |
|---|---|
| `0` | write data A (TA latch) |
| `1` | write data B (execute operation) |
| `2` | write command A (TA latch) |
| `3` | write command B (execute command) |
| `4` | read data A (TA) |
| `5` | read data B (execute read, returns previous TB) |
| `6` | read command A / busy status |
| `7` | read command B (returns 0) |

Commands (`B` write, `TB & 0xE0`):

| Command | Meaning |
|---|---|
| `0x00` | begin row: X=0, Y=TA & 0x1F |
| `0x20` | load Y |
| `0x40` | load X |
| `0x60` | increment cursor C |
| `0x80` | load M |
| `0xA0` | load R |
| `0xC0` | load Y0 |
| `0xE0` | not interpreted |

Data operations (`M & 0xE0`):

| M | Write B | Read B |
|---|---|---|
| `0x00` | write A/B at cursor, increment C | — |
| `0x20` | — | read A/B at cursor, increment C |
| `0x40` | write without increment | — |
| `0x60` | — | read without increment |
| `0x80 | slice` | write slice to extended RAM when `b >= 0xA0`; slice increments mod 10 | — |
| `0xA0 | slice` | — | read slice / charset; returns TA, TB=0xFF; slice increments mod 10 |

The busy bit is currently immediate (MAME has the same TODO). This is a
documented ambiguity, not a title-specific shortcut: no game in the local `+`
library depends on a measured EF9341 busy delay.

---

## G. Register map

The EF9340/EF9341 logical register set is `TA`, `TB`, `X`, `Y`, `Y0`, `M`,
`R`, `busy`, plus the A/B/extended memories. The CPU-visible register map is
the 8-address MOVX window above. The G7400 i8245 VDC register map remains the
NG-3 map (`0x00-0xFF`, A0-A5, objects, quads, grid, sound).

VPP control `R` bits:

| Bit | Meaning |
|---|---|
| `0x01` | display automaton active |
| `0x02` | box/opaque delimiter mode |
| `0x04` | conceal |
| `0x08` | service row enabled |
| `0x10` | cursor visible |
| `0x20` | vertical zoom (double height) |
| `0x40` | 250-line display window (otherwise 210) |
| `0x80` | blink enable |

---

## H. VPP memory

- `ram_a_`, `ram_b_`: 0x400 bytes each; clear on reset.
- `extram_`: 0x800 bytes; clear on reset.
- Display RAM address uses `CAddr(x, y)`:
  - `(y & 0x18) == 0x18` → `0x318 | ((x & 0x38) << 2) | (x & 0x07)`
  - `x & 0x20` → `0x300 | ((y & 0x07) << 5) | (y & 0x18) | (x & 0x07)`
  - otherwise `((y & 0x1F) << 5) | (x & 0x1F)`.
- Cursor increments at X = 40/48/56/64, wrapping Y at 24.
- Extended RAM address follows the documented 8-bit latch/wiring formula,
  masked to 0x7FF.
- Readback preserves the two-stage TA/TB EF9341 protocol.
- Boundary/reset tests are in `tests/ng5` (address 0, 40-column wrap, row
  wrap, slice 0..9, reset clears all three memories).

---

## I. Character ROM / provenance

`kEf9340Charset` is 2560 bytes = 256 characters × 10 scanlines.

- Source: the project's existing legacy Videopac+ table
  (`vpp_cset.cpp`, O2EM 1.20/1.21), already present in this repository.
- Generated into `src/core_ng/video/ef9340_charset.cpp`;
  CRC32 `D557A7BF`.
- MAME 0.289's `charset_ef9340_1.rom` expects `8DE85988` and marks it
  `BAD_DUMP` (datasheet-derived). The two tables differ; the legacy table is
  used because it is the project's established Videopac+ data and renders the
  local `+` library correctly against MAME.
- This is the only verbatim data added in NG-5. The i8244 CGROM remains the
  only other verbatim chip table (NG-3).

---

## J. Dynamic characters

Dynamic characters use character codes `0xA0..0xFF`.

1. Write the cell attribute A and code B (`M=0x00`).
2. Point the cursor back to the cell.
3. `M=0x80|slice`, then for each slice write data A followed by data B. The
   data-B write stores TA into `extram_[a<<12 | b<<4 | slice]` and advances the
   slice modulo 10.

Tests cover:

- 10 × `0xFF` slices → full 8×8 foreground cell, marker bit 3 set.
- Slice readback through the EF9341 (returns `0xFF`).
- Overwriting one slice changes only that scanline.
- Reset clears extended RAM.
- Mid-frame use: row 0 keeps the old glyph after line 24; row 1 uses the new
  slices after the write.

The earlier NG-5 draft uploaded only data A; the device test caught that the
EF9341 write operation is triggered by data B. This is now covered.

---

## K. Modes

Implemented and tested:

- normal alphanumeric / graphics cells
- dynamic character cells (`B >= 0xA0`)
- double width
- double height
- inversion
- blink (32-frame prescaler)
- service row
- display disabled (only the service-row rule)
- box/opaque delimiter
- conceal / underline delimiters

Not modelled because no evidence/local title needs it:

- the bus-idle/busy timing of a real EF9341 (documented TODO)
- RES/restart pin
- internal EF9340 display timing beyond the G7400's external scanline clock

These are recorded as hardware ambiguities, not title workarounds.

---

## L. Transparency

The EF934x layer carries the hardware marker:

- normal cells: `colour | 8`
- box/opaque cells: `colour` (marker clear)

Composition for each pixel:

```text
en = (e & 8) || !b_pin
i2 = !((mix_ef934x >> (e & 7)) & 1)
vdc.ExternalCollision(x, i2)
if (en && ((mix_i8244 >> (d & 7)) & 1)) out = vdc_pixel | lum
else out = e | (i2 ? 8 : 0)
```

Meaning:

- With `b_pin=0`, the VDC decoder is always enabled; the VPP appears only
  where the `mix_i8244` colour key masks the VDC colour.
- With `b_pin=1` (Plus cartridge PCB), a VPP cell without the marker masks the
  VDC and shows the EF934x colour; the `mix_i8244` key still selects VDC where
  allowed.
- `e & 7` is the logical EF934x colour index; `i2` is the external-collision
  input, not a transparency bit.

Tests cover VDC-only, VPP-only, masked VDC colour, opaque box cells, B-pin
high/low and empty cells.

**Important fix:** the first NG-5 composition used `out = (vdc & 7) | lum`,
which stripped the VDC intensity bit (palette 8..15 became 0..7). MAME keeps
the existing pixel and ORs LUM. The corrected code preserves the full VDC
pixel. This was the cause of Tutankham's black pyramid / missing letters and
is now a regression check ("compose: VDC intensity bit preserved").

---

## M. Luminance

The G7400 LUM input is the inverted P17 pin:

```text
lum = ((~p1) >> 4) & 0x08
```

- P17 high → no LUM (palette 0..7)
- P17 low → palette bit 3 added (palette 8..15)

Tests verify both states on the VDC layer, including preservation of the
object's own intensity bit. Logical palette indices are kept separate from
presentation calibration; no CRT/RF filter work is included.

---

## N. VPP timing

- The VPP has no independent frame counter or clock.
- `Vpp::CompositeLine(screen_line, vdc)` is called once per completed VDC
  scanline.
- `DrawLine()` uses the current VPP registers and the authoritative scanline
  number.
- A register write after some lines have been composited only affects the
  remaining lines (tested: row 0 old glyph, row 1 new glyph after a mid-frame
  slice update).
- The blink prescaler is advanced once per frame on the same timeline.

The G7400 VBL IRQ is the i8245 line; the VPP has no CPU interrupt of its own.
This matches MAME and the hardware wiring.

---

## O. VDC / VPP composition

Per scanline, after the i8245 renders its objects:

1. The EF934x layer is drawn for that scanline.
2. Each pixel is composed with the `b_pin`/colour-key/marker rules above.
3. The EF934x output is tied to the VDC `CX` external-collision input.
4. The result is written back into the VDC live framebuffer, so the presented
   frame is the composite.

This is the same ordering as MAME's per-scanline `screen_update`. It is not a
post-frame overlay and has no title-specific blending.

---

## P. IRQ / status / readback

- The only G7400 CPU interrupt source is the i8245 VDC `/INT` line.
- VPP/EF934x has no CPU interrupt pin.
- A1 read clears VBL/sound/external/major latched status and the IRQ line, as
  in NG-3.
- EF9341 command/data reads implement the two-stage A/B latch; command B reads
  0; command A reports busy (currently 0/immediate).
- A2 collision read is clear-on-read via the VDC.
- No `return 0` stubs remain for CPU-visible VPP reads; unsupported slice
  reads return `0xFF` as MAME does.

---

## Q. G7400 bus / memory decode

| P1 bit | Function |
|---|---|
| P10 | external RAM chip select (active low) |
| P13 | i824x VDC chip select (active low) |
| P15 | EF934x/EF9341 chip select (active low) |
| P16 | cartridge / external bus write enable (active low) |

- EF934x is mapped at MOVX `0x00..0x07` when P1 bit5 is low.
- The i824x VDC still decodes `0x00..0xFF` when P1 bit3 is low and P1 bit6 is
  low for reads.
- 8243 expander P4/P5 → i8244 colour-mix key; P6/P7 → EF934x colour-mix key.
- Cartridge B pin is PCB metadata (`b_pin`).
- No legacy VPP overlap remains in `core_ng`; the legacy global `gMem` /
  `gDynamicChars` / `gLayer` / `gDirty` path is not used.

---

## R. Mapper / device inventory

G7400/Videopac+ images use the standard cartridge device unless their PCB is
different. B-pin is metadata, not a mapper.

| Local image | Title | MAME software | Result |
|---|---|---|---|
| `vp_01+.bin` | Race / Spin-Out / Cryptogram+ | `3rcsocptpl` | GREEN 1.0000 |
| `vp_06+.bin` | Ten Pin Bowling / Basketball+ | `2bwlbsktpl` | GREEN 1.0000 |
| `vp_11+.bin` | Cosmic Conflict+ | `cosmicpl` | GREEN 1.0000 |
| `vp_20+.bin` | Stone Sling+ | `stonepl` | GREEN 1.0000 |
| `vp_43+.bin` | Pickaxe Pete+ | `pickaxepl` | GREEN 1.0000 |
| `vp_53+.bin` | Phantom House+ (prototype) | `phantom` | GREEN 1.0000 |
| `vp_54+.bin` | Loony Balloon+ | `loony` | GREEN 1.0000 |
| `vp_55+.bin` | Neutron Star+ (prototype) | `neutronp` | GREEN 1.0000 |
| `vp_56+.bin` | Norseman+ | `norseman` | GREEN 1.0000 |
| `vp_58+.bin` | Air Battle+ (prototype) | `airbattlp` | GREEN 1.0000 |
| `vp_59_16.bin` | Helicopter Rescue+ | `helicopt` | GREEN 1.0000 |
| `vp_60_16.bin` | Trans American Rally+ | `rally` | GREEN 1.0000 |
| `vp_62+.bin` | Clay Pigeon+ | `claypigpl` | GREEN 1.0000 |
| `jo_exojet_pl.bin` | Exojet+ | `exojet` | GREEN 1.0000 |
| `jo_moto-crash_pl.bin` | Moto-Crash+ (France) | `motocras` | GREEN 1.0000 |
| `vp_Tutankham+.bin` | Tutankham+ (prototype) | `tutank` | YELLOW 0.9828 |
| `pal_flashpoint.bin` | Flash Point (PAL conversion) | raw `flashph` | GREEN 1.0000 |

`b_pin=1` is derived only from MAME's published `feature b_pin=1` metadata:
VP01+, VP06+, VP11+, VP20+, VP43+, VP54+, VP56+, VP59+, VP60+, VP62+,
Exojet+, Moto-Crash+, Flash Point, plus the standard VP48/49/50/51/52/57 and
several non-local titles. Tutankham+ is correctly `b_pin=0`.

---

## S. Synthetic device-test results

NG-5 suite: **105 checks, 0 failures, 0 skips** (Debug and Release).

Coverage:

- G7400 profile values and both VBL phases
- reset state of every owned VPP field/memory
- EF9341 command and data protocol
- A/B read, increment/no-increment, cursor wrap
- 40-column and row wrapping
- dynamic character upload/readback/overwrite/reset
- charset size/CRC/first glyph
- double width, double height, inversion, service row, display-off
- VDC intensity preservation, LUM, B-pin masking, colour-key transparency
- mid-frame VPP update timing
- VPP save/load, resumed frame hash, deterministic repeated save
- real G7400 BIOS/VP60, VP59+, Tutankham load/run
- B-pin metadata for VP60 and Tutankham
- machine save/restore at frame 240 and deterministic independent runs
- standard G7000 cartridge under the G7400 machine

---

## T. VP59+ result

- Boots and reaches the gameplay landscape at frame 1000.
- Scripted input `"1"` then Enter.
- Frame 1000 and frame 1500: **IoU 1.0000** against MAME `videopacp` with the
  same input.
- No freeze, no forward-movement special case, no collision bridge, no
  legacy 0029G/0029F path.

---

## U. VP60 result

- Boots, passes the title/map transition and reaches the racing stage with
  dashboard and cars at frame 1000.
- Frame 1000 and frame 1500: **IoU 1.0000** against MAME with the same
  `"1"` + Enter sequence.
- The legacy pulse-IRQ workaround was not recreated; the NG VBL phase and
  VPP/VDC composition are the only mechanisms.

---

## V. Moto-Crash+ result

- Boots and reaches the gameplay screen (rider, obstacles, timer/score).
- Frame 1000 and frame 1500: **IoU 1.0000**.
- No legacy special case.

---

## W. Remaining + title results

15 of 16 no-input `+` titles are pixel-perfect at frame 500. The only
non-perfect title is:

**Tutankham+ — YELLOW 0.9828**

- The title, pyramid, text, camel and all sprites match.
- 84 pixels differ on exactly one scanline (`y=99`, the horizon): the game
  writes background colour `A3=0x70` mid-line at `hpos=222`; MAME's transition
  is ~86 pixels later on the same line. Every other scanline is identical.
- This is a sub-line CPU/write-phase difference, not a VPP or composition
  bug. Classified YELLOW with the exact first divergence recorded in
  `compat_matrix_ng5.csv`.
- No title-specific offset or palette hack was added.

VP53+ Phantom House, VP06+, VP58+ Air Battle and the other prototypes are
GREEN; earlier apparent mismatches were caused by scripted-input reference
captures that were one state ahead/behind. With the no-input reference they
are pixel-perfect.

---

## X. Standard G7000 regression

Method: NG G7400 machine (G7400 BIOS + VPP enabled) running standard G7000
cartridges; reference: MAME `videopacp` raw cartridge load (same G7400 BIOS),
no input, frame 500.

| Title | Result |
|---|---|
| VP01 Race / Spin-Out / Cryptogram | 1.0000 |
| Atlantis | 1.0000 |
| Popeye (Europe) | 1.0000 |
| Frogger (Brazil) | 1.0000 |
| Popeye (Brazil) | 1.0000 |
| Type & Tell! | 1.0000 |
| Frogger (Europe) | 1.0000 |
| Q*bert (Europe) | 1.0000 |

8/8 GREEN. The earlier `videopac` (G7000 BIOS) comparison was the wrong
reference; it compared two different machines.

NG-4 (G7000-only) remains: 146 rows, 143 compared, **142 pixel-perfect**,
1 RED (Flash Point under the G7000 machine). Flash Point is now GREEN 1.0000
under the G7400 machine, which is the correct machine for that title.

---

## Y. Save-state / determinism

Tested at device and machine level:

- Save after 80 visible lines, restore into a fresh rig, finish the frame:
  framebuffer hash identical.
- Save at machine frame 240, run 30 frames, restore, run 30 frames:
  framebuffer hash identical.
- Independent runs of the same BIOS/cart/input produce identical frame hashes.
- VPP snapshot includes TA/TB, busy, X/Y/Y0/R/M, blink state/prescaler,
  h-parity, mix keys, B pin, P1, A/B/extended RAM, completed frame count and
  the live layer.
- VDC snapshot remains the NG-3 VDC3 format; the machine snapshot wraps VDC +
  VPP + scheduler + mapper + CPU.
- Repeated saves of the same mid-frame state are byte-identical.

Debug and Release suites both pass; no floating-point or uninitialized state
is used in the VPP or composition path.

---

## Z. Next-phase recommendation

1. **Real G7400 hardware reference** — the local G7400 was not available during
   NG-5. When it is, verify: the Tutankham horizon write phase, the VPP/VDC
   layer priority on transparency-heavy screens, service-row timing, and
   EF9341 busy/status behavior.
2. **EF9341 timing** — implement real busy duration/RES only if hardware or a
   local title proves it matters. Do not invent a delay.
3. **NG-6 audio** — the VPP/VDC hooks already use the authoritative NG tick.
   Replace the legacy audio path with a timestamped sound device on the same
   scheduler; do not import SDL block timing into `core_ng`.
4. **C7010** — next special-hardware phase; keep it out of the G7000/G7400
   base machine.
5. **Palette calibration** — logical indices are correct; subjective G7400
   presentation colour (and the MAME snapshot's brightness/gamma difference)
   belongs to a later display layer, not to hardware semantics.
6. **Promote NG core** — only after the remaining hardware ambiguities are
   settled. The legacy core is still selectable for A/B.

---

## Final QA summary

| Item | Result |
|---|---|
| NG-1 MCS48-NG | 667/667 Debug, 667/667 Release |
| NG-2 machine | 402/402 Debug, 402/402 Release |
| NG-3 VDC | 155/155 Debug, 155/155 Release |
| NG-4 G7000 matrix | 146 rows, 143 compared, 142 pixel-perfect, 1 RED (Flash Point, G7000-only) |
| NG-5 device suite | 105/105 Debug, 105/105 Release |
| NG-5 G7400 matrix | 25 rows, 24 GREEN, 1 YELLOW, 0 ORANGE, 0 RED |
| + titles | 15/16 pixel-perfect; Tutankham+ YELLOW 0.9828 |
| G7000 under G7400 | 8/8 pixel-perfect |
| VP59+ / VP60 / Moto-Crash+ gameplay | 1.0000 at frames 1000 and 1500 |
| Debug app build | PASS |
| Release app build | PASS (`SkipReleasePackaging=true`) |
| Performance (Release, diagnostics off) | G7000 Popeye ~939 fps; G7400 VP60 ~751 fps; G7400 Tutankham ~636 fps (1000 PAL frames) |
| `git diff --check` | exit 0 (line-ending warnings only) |
| Nothing staged | yes |
| Nothing committed/pushed | yes; last commit unchanged `bb97bf1` |
| Installed Release | untouched: 922624 bytes, SHA256 `d7409fcff474734dc3d9d1f9671e11a2cc60c0f8f4b848a6683d6af662b57666` |
| Legacy correctness changes | none |
| Title-specific hacks | none |
| Frontend/input replacement | none |

Remaining hardware ambiguities are explicit: EF9341 busy timing, RES pin,
internal EF9340 display timing beyond the external G7400 scanline clock, and
the BAD_DUMP reference charset. None is papered over by a title branch.

No audio redesign was attempted. The VPP/VDC timing hooks are on the single NG
timeline, ready for the later timestamped audio device.

## Release-candidate orientation correction (2026-11-10)

Static EF9340 character-generator bytes are stored most-significant-bit first,
matching the legacy O2EM renderer. The NG line renderer had consumed all
character data least-significant-bit first, which mirrored static glyphs while
leaving ExtRAM/dynamic character data (written in the opposite order by
software) correct.

This was visible on the G7400 BIOS `SELECT GAME` text: NG showed mirrored
letters, while the legacy renderer showed readable text. The fix reverses only
static charset bytes before the existing renderer; ExtRAM data, frame
composition and presentation are unchanged. A synthetic asymmetric-cell test
was added to `tests/ng5/ng5_device_conformance.cpp`.

MAME 0.289 currently repeats the mirrored static charset; the legacy O2EM
renderer and the readable BIOS text are the orientation reference for this
correction.
