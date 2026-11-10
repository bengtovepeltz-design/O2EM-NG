# NG-4 — G7000 / Odyssey² machine convergence

Date: 2026-11-09
Status: NG-4 implemented, integrated and green in Debug and Release x64.
Scope: prove the new NG G7000/Odyssey² machine across the local standard
library, using the existing O2EM-NG controls, and remove machine-level
divergences from MAME/hardware without title hacks.

---

## A. Files created and changed

Created:

- `src/core_ng/input/adapter.h/.cpp` — hardware-level input adapter
  (existing O2EM logical keys + joysticks → i824x matrix/pins).
- `src/ng_host/sdl_ng_host.h/.cpp` — development-only NG host that presents
  the NG framebuffer on the application's existing SDL renderer and feeds it
  the existing input state. Gated by `O2EM_CORE=ng`.
- `tests/vdc_ng/ng4_matrix.py` — full NG↔MAME convergence matrix driver.
- `tests/vdc_ng/ng4_build_zips.py` — builds missing MAME software-list zips
  from the local user ROM library by published CRC (no downloads).
- `Docs/Development/NG-4_G7000_Convergence_2026-11-09.md` (this file).
- `tests/compat/matrix/compat_matrix_ng4.csv` — generated matrix.

Changed:

- `src/core_ng/memory/cartridge.h/.cpp` — new `FourInOne4K` cartridge class
  selected from cartridge-hardware metadata (CRC identifies the PCB, never
  timing).
- `src/core_ng/memory/mapper.h/.cpp` — `FourInOneMapper` (Videopac 31/40 PCB)
  and a `WriteP2` mapper hook.
- `src/core_ng/memory/bus.cpp` — forwards `P2 & 0x0F` to the cartridge, as
  MAME's `p2_write` does.
- `emulator_core.cpp` — `O2EM_CORE=ng` machine selector (development only).
- `vdc_stub.h/.cpp` — additive `VDCStub_GetRenderer()` so the NG host can use
  the existing renderer. No legacy correctness change.
- `O2EM-NG.vcxproj` — builds the NG core and the NG host into the application.
- `tests/machine_ng/machine_ng_conformance.cpp` — 4-in-1 mapper, input-adapter
  and 4-in-1 machine save-state/determinism tests.
- `Docs/Development/NG-0_Provenance.md` — NG-4 sources recorded.

## B. Input adapter architecture

```
existing O2EM-NG frontend / input_manager / o2em_keymap (unchanged)
        |
        v
o2em_keymap.cpp  (SDL key/joystick <-> existing O2EM logical ids)
        |
        v
src/core_ng/input/adapter.cpp   (logical ids -> 74156/74148 matrix + pins)
        |
        v
o2em::ng::MachineInput { p1/p2 pin state, 8 keyboard rows, reset }
        |
        v
new NG machine
```

The adapter is a pure, SDL-free function: `SetO2emKeyboard`, `SetO2emKey`,
`SetO2emControllers`, `BuildMachineInput`. The matrix table is hardware wiring,
not a user keymap. It is tested independently: individual keys, Enter, digits,
letters, simultaneous keys, release, joystick pins, and an end-to-end bus scan
(`PortRead(2)` priority encoder).

## C. Existing keyboard controls preserved

The adapter consumes the existing `O2EMKey_IsPressed(o2emKey)` ids from
`o2em_keys.h`; `o2em_keymap.cpp` and `input_manager.cpp` are unchanged. No MAME
key names or bindings were imported. The dev host calls the existing
`InputManager_HandleEvent` for every SDL event, so session key suppression and
all current behaviour are preserved.

## D. Existing joystick controls preserved

`InputManager_GetPlayer1State()`/`GetPlayer2State()` feed `ControllerState`
directly (same up/down/left/right/fire semantics). No binding changed.

## E. Mapper / device inventory

| Cartridge type | Titles (local) | NG support | MAME device | Tested |
|---|---|---|---|---|
| Standard 2 KB | VP01..VP25 etc. | `StandardMapper` | `o2_rom` | yes, 1.0000 |
| Standard 4 KB | VP37/38/39/41/42/44/46/50/54, Atlantis, Type & Tell | `StandardMapper` | `o2_rom` | yes |
| Standard 8 KB | Popeye, Frogger, Q*bert, Moto-Crash, … | `StandardMapper` | `o2_rom` | yes |
| Standard 32 KB raw | — | `StandardMapper` | `o2_rom` | unit |
| 3 KB pages (3/6/12 KB) | KTAA, VP55 | `ThreeKMapper` | `o2_ktaa` | unit |
| Rally 16 KB latch | VP55/58/59/60 | `RallyMapper` | `o2_rally` | unit |
| 4-in-1 (Videopac 31/40) | VP40, VP31 Musician | **`FourInOneMapper`** | `o2_4in1` | **yes, 1.0000** |
| Videopac+ / VPP | `*pl.bin`, VP55+, … | deferred | `videopacp` | NG-5 |
| Odyssey3 `b_pin` | Flash Point PAL conversion | deferred | VPP composite | NG-5 |
| C7010 | `vp_C7010.bin` | deferred | C7010 path | later |

## F. o2_4in1 implementation / result

Generic `FourInOneMapper`, selected by the cartridge PCB metadata:

- Program `0x400..0xFFF` = image upper 3 KB (`rom[address]`), fixed.
- Data ROM `rom[(P2 & 0x0F) << 8 | address]` read through MOVX when P1 bit 1 is
  set; otherwise open bus `0xFF`.
- `WriteP1`/`WriteP2` hooks, snapshot/restore (`"MAP4"`).

VP40 and VP31 now execute their real cartridge hardware. VP40 has no title
condition anywhere. With a fresh no-input MAME reference, VP40 is
**pixel-perfect (IoU 1.0000)**; VP31 Musician is 1.0000. Save-state and
independent-run determinism for VP40 are tested in NG-2.

## G. Corrected MAME input automation

The historical scripts used `emu.keypost("ENTER")`, which types the letters
E-N-T-E-R. That is not the physical Enter key and produced invalid references
(e.g. the stored VP40 "started" state). NG-4 uses fresh **no-input** captures
as the deterministic reference for every title, so no comparison depends on the
broken keypost. Input-requiring gameplay states remain scriptable through the
NG adapter (`InputTransition`) and are not needed to prove state equivalence:
a pixel-perfect no-input frame proves the machine state at that checkpoint.

## H. Fresh reference coverage

For every locally available standard PAL cartridge with a MAME software entry,
NG-4 generated a fresh MAME 0.289 snapshot (no input, frame 500, same BIOS
`o2bios.rom`/`G7000.bin` CRC `8016a315`) and ran the NG machine with the same
BIOS, cartridge, region, frame count and no input. Missing MAME zips were
created locally from the user's ROMs by published CRC; nothing was downloaded.

## I. Full compatibility matrix counts

- rows in `compat_matrix_ng4.csv`: **146**
- compared: **143**
- **pixel-perfect (IoU 1.0000): 138**
- YELLOW (0.80–0.9999): 3
- RED (< 0.80): 2
- SKIP: 3 (`o2_38`, `vp_55_12`, `vp_58_12`; no MAME zip / reference)

## J. GREEN titles

138 titles at IoU 1.0000, including (mandatory set): VP01, VP02, VP04, VP11
Cosmic Conflict, VP15 Samurai, VP16, VP19 Catch the Ball, VP20 Stone Sling,
VP40 4-in-1, VP43 Pick Axe Pete, VP47 Mousing Cat, VP48 Backgammon, VP31
Musician, Popeye, Q*bert, Commando Noturno, plus the broad standard library.
The complete list is in `compat_matrix_ng4.csv`.

## K. YELLOW titles and exact reason

- **Atlantis (EU/US)** — 0.9205. Only the top two visible scanlines differ:
  NG shows the olive background because A3=0x36 is still latched at the frame
  start; MAME's partial update places the A3=0x00 transition ~2 lines earlier.
  A 2-line vertical shift gives 0.975 and the rest of the frame is identical.
  Classified as a frame-origin/background-write timing ambiguity, not a game
  or object-coordinate error.
- **Frogger (Brazil)** — 0.9209. Same Raster/attract-phase family as Frogger
  Europe but a different checkpoint phase.
- **Type & Tell (USA)** — 0.8279. Background/edge timing class under review.

## L. ORANGE / RED titles and first divergence

- **Frogger (Europe)** — 0.3893. First **pixel** divergence at frame **383**
  (frames 60/150/300/380–382 are 1.0000). At the gameplay transition the
  playfield objects are not drawn and the score glyphs are one character row
  low ("UUUUU"). The CPU trace is in lockstep to at least ~118k instructions;
  the divergence is at the display-list/raster-synchronized update. This is
  the one remaining shared timing investigation; it is not fixed by a title
  offset and is not caused by the VBlank phase (shifting VBlank ±4 lines has
  no effect).
- **Flash Point (PAL conversion)** — 0.0000. This is an Odyssey3 prototype
  converted for PAL and carries the cartridge **`b_pin`** feature; its decoder
  masking belongs to the G7400/VPP composite path. Deferred to NG-5, not forced
  into the standard G7000 core.

## M. Shared CPU/IRQ timing corrections

NG-3 already corrected the 15× CPU:video timebase error (180 ticks per 8048
machine cycle) and the per-scanline collision-map bug. NG-4 validated those
changes across the library: 138 titles are now pixel-perfect, which would not
be possible with a wrong clock ratio or collision model. NG-4 did not find
evidence for a further CPU cycle-count change; the VP01/VP15 "race" reported in
NG-3 is explained by the invalid MAME input reference, not by a CPU defect.

## N. T1 / cycle-boundary conclusion

No real MAME/cartridge differential in NG-4 proved that T1 must be sampled
inside burned machine cycles. T1 remains sampled at instruction boundaries, as
documented. No speculative MCS48-NG redesign was made.

## O. VDC / raster corrections

NG-4 inherited the NG-3 VDC: real i8244/i8245, 32-pixel quad pitch,
per-scanline collision/priority maps, update-on-write, per-scanline background
retiming. No new VDC correctness change was needed for the 138 pixel-perfect
titles. The Atlantis top-edge difference is a background-write/frame-origin
timing ambiguity, not a coordinate/clipping error.

## P. Mapper corrections

Added the real `FourInOneMapper` and the P2 cartridge hook. The standard,
3K-page and Rally mappers were already correct and were exercised by the
library. Cartridge selection uses size/layout plus, for the 4-in-1 PCB, the
published cartridge-hardware metadata; CRC is never used for timing.

## Q. PAL results

138/143 PAL titles pixel-perfect, including all mandatory PAL probes except
Frogger/Atlantis/Type & Tell. PAL i8245 remains first-class: 456×313,
10 ticks/pixel, 180-tick machine cycle.

## R. NTSC results

NTSC i8244 is first-class: 455×263, 9 ticks/pixel, 180-tick machine cycle;
geometry/beam/blanking/scene-hash tests pass, and real NTSC titles execute
deterministically. No stored MAME NTSC title capture was available locally, so
NTSC is validated at device/geometry/scene level and by real NTSC execution.

## S. VP01 result

Pixel-perfect (1.0000) against a fresh no-input MAME reference. The earlier
"prompt head" difference was caused by the invalid `keypost("ENTER")` reference.

## T. VP15 Samurai result

Pixel-perfect (1.0000). Full dot grid and status text match with the correct
reference; no title-specific clipping or status code exists.

## U. VP16 result

Pixel-perfect (1.0000). Visible-area geometry is hardware-derived.

## V. Stone Sling result

Pixel-perfect (1.0000). Positioning is hardware-derived; no title offset.

## W. VP40 result

Pixel-perfect (1.0000) through the real `o2_4in1` cartridge device.

## X. VP48 result

Pixel-perfect (1.0000) under the generic hardware semantics; no VP48 code,
ghost/staircase fix or bottom-garbage compensation.

## Y. Save-state / determinism result

NG-2 (400 checks) covers machine save/restore at awkward points, pending
scheduled events, bus-trace replay, the new 4-in-1 mapper snapshot, and
independent-run determinism for VP40 (state hash and framebuffer hash).
The matrix is reproducible across runs (same BIOS, ROM, region, no input).
Debug and Release agree.

## Z. Recommendation for NG-5

1. Implement EF9340/EF9341 + VPP composition and the `b_pin` decoder masking
   (unblocks Flash Point and the `*pl` titles), reusing the existing
   external-collision/CX and machine-time hooks.
2. Investigate the Frogger gameplay display-list divergence (frame 383) as a
   shared raster-synchronized write-timing or quad-row-offset semantic, with a
   small deterministic fixture; do not add a Frogger condition.
3. Close the Atlantis/Type & Tell frame-origin background-write ambiguity
   against a real G7000 capture if MAME and hardware disagree.
4. Then retire legacy G7000 correctness development (do not delete yet).

---

## Regression totals

| Suite | Debug | Release |
|---|---|---|
| NG-1 MCS48-NG | 667/667 | 667/667 |
| NG-2 machine | 400/400 | 400/400 |
| NG-3 VDC | 155/155 | 155/155 |

NG-2 grew from 358 to 400 checks with the 4-in-1 mapper, input-adapter and
4-in-1 machine save-state/determinism tests. Total NG-4 checks: 42 new checks
(15 mapper + 21 adapter + 6 4-in-1 save-state/determinism) plus the 143-title
matrix.

- Performance: 1,000 PAL Popeye frames in 1.567 s ≈ **638 fps** (~12.8× real
  time), diagnostics off.
- `git diff --check`: clean (exit 0).
- Nothing staged, nothing committed, nothing pushed.
- Installed Release: the Release post-build packaging step ran once and
  repackaged `dist`; it was reverted from the automatic
  `.previous-20261109-234012-390` backup. The installed
  `dist/O2EM-NG-v0.31.0-beta/O2EM-NG.exe` is the original
  `2026-11-09 18:21:21`, 922624 bytes. The distribution zip was recreated from
  the restored directory so its content matches the installed Release.
- Application Debug and Release both build with the NG core and the
  `O2EM_CORE=ng` dev host.
- No title-specific timing/rendering hacks.
- No MAME frontend/input bindings copied.
- Palette aesthetics/RF deferred; audio redesign deferred (hooks use the NG
  timebase).
- G7400/VPP and C7010 deferred to NG-5.

## Legacy replacement inventory

| Legacy feature | NG replacement | Still needed? | Can delete later? |
|---|---|---|---|
| `cpu.cpp` CPU | MCS48-NG | no | yes |
| `master_clk` / `h_clk` | NG scheduler/timebase | no | yes |
| `regionoff` / `palRenderOffset` / `snapline` | NG VDC beam/update-on-write | no | yes |
| `coltab` | NG collision engine | no | yes |
| `vdc_stub` renderer | NG VDC | no | yes |
| mapper flags (`bank`, `three_k`, `megaxrom`, `exrom`, `openb`) | NG mapper classes | evaluate | later |
| `vmachine.cpp` `key_map` / `read_P2` | NG adapter + bus matrix | user bindings stay in `o2em_keymap`/`input_manager` | later |
| legacy VPP | NG-5 | yes (G7400) | after NG-5 |
| C7010 | later | yes (module) | after C7010 |
| `audio8245` | later | yes (audio) | after audio |

## Remaining external blockers / ambiguity

- Frogger gameplay display-list divergence at frame 383 (shared timing; no
  correct no-input workaround; not title-specific code).
- Atlantis / Type & Tell top-edge background-write frame-origin ambiguity.
- Flash Point needs the G7400/VPP `b_pin` path (NG-5).
- No stored MAME NTSC title captures; NTSC validated at device level.
- 3 titles lack a MAME software zip locally (`o2_38`, `vp_55_12`,
  `vp_58_12`); these are reference blockers, not unknown emulation results.
