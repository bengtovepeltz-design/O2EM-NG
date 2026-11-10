# Legacy O2EM compatibility debt inventory — Checkpoint A

This is a classification, not a removal plan. Nothing in the production core was
changed for this inventory. Locations refer to the current working tree.

Priority for replacement is hardware/MAME evidence, not historical behaviour.

| # | Location | Legacy behaviour | Why it exists | Titles affected | MAME equivalent | Hardware evidence | Removable? | Replacement model |
|---|---|---|---|---|---|---|---|---|
| 1 | `vmachine.cpp:do_kluges` (`setvideomode(1/0)` blocks) | Per-title PAL/NTSC FPS and `evblclk` overrides | Historical per-title region/frame workarounds | Atlantis, Jake, Power Lords, Labirinth, Nightmare, Shark Hunter, Super Bee, War of the Nerves, Hockey/Soccer, Four in 1 Row, Moto-Crash, Helicopter Rescue+, Rally+, Blobbers, Le Tresor Englouti+, Tutankham, VP53, Flashpoint, Catch the Ball, Blockout, Demon Attack, Restaurant, Shark Hunter NTSC | One `screen` geometry per system/BIOS; cart does not select FPS | i8245 PAL/NTSC geometry is a board property | **Yes, after the unified timebase** | Derive region/frame from the selected system/BIOS and the i8244/8245 device, not the cart CRC |
| 2 | `vmachine.cpp:do_kluges` (`evblclk=12000/7642/6100/8000`) | Per-title frame length | Compensates old core frame pacing | Pick Axe Pete, Mousing Cat, Frogger, Commando Noturno, Wall Street, Depth Charge/Marksman | Frame length is fixed by the video standard | Same | **Yes** | Scheduler/beam frame length from the active PAL/NTSC geometry |
| 3 | `vmachine.cpp:do_kluges` (`regionoff=...`) | Per-title scanline offset | Old renderer/frame-local calibration | Blockout, Q*bert, Popeye, Out of this World, Smithereens, Type & Tell, Dynasty/Samurai, Atlantis, Frogger, Mousing Cat, Four in 1 Row, P.T. Barnum, Catch the Ball, Commando | `update_now()` at the real beam; no per-title line offset | Video register write time is the beam position | **Yes, after proving any true fixed panel offset** | Exact beam line; retain only a documented fixed visible-area offset if hardware evidence demands it |
| 4 | `vdc_stub.cpp:draw_region` CRC branches (`A7344D1F`, `D0BC4EE6`, `26517E77`, `A57E1724`) | Per-title `/22`, `/24`, `/20` raster scales and +6/-5/+7/-3 offsets | Compensates old raster scale | Atlantis, Frogger, Commando Noturno, Catch the Ball | Exact `screen().hpos()` / `update_now()` | One beam position | **Yes, after the unified timebase** | One `beamLine()`; remove the CRC raster class |
| 5 | `vdc_stub.cpp:draw_region` (`/20`, `/22`, `/24`, `palRenderOffset`) | Legacy line-rate approximations and renderer fudge | Old core used frame-local/20-22-cycle beam math | All PAL/NTSC titles in legacy paths | i8245 456 px/line, 18 px/MCS48 cycle | 25.333 cycles/line | **Yes** | Authoritative phase/beam line |
| 6 | `vmachine.cpp:VM_PalA4BeamValue` vs `draw_region` round vs `I8244HStatusBit` | Multiple floor/round/offset beam calculations | Grew incrementally during VP48 work | All PAL titles | One `screen().hpos()/vpos()` | One beam | **Yes** | One beam API used by A4/A5/A1/T1/VBL/renderer |
| 7 | `vmachine.cpp:snapline` + `mxsnap` | Snap register write to a nearby previously drawn line | De-jitter old renderer | Blockout, Catch the Ball, Clay Pigeon, Verkehr, Super Cobra, Wall Street, Turtles, Frogger, Four in 1 Row, and default `mxsnap=2` | `update_now()` is already exact | No hardware snap-to-nearest-line behaviour | **Yes** | Remove; keep only if a proven hardware sampler needs it |
| 8 | `vmachine.cpp:do_kluges` (`useforen`) | Ignore A0 bit 5 for selected titles | Legacy O2EM always drew objects; gate added for Turtles/Q*bert/Flashpoint | Turtles, Q*bert, Flashpoint 5 | `control & 0x20` gate applies to all | Display enable is hardware | **Yes** (the corrected renderer already applies the gate) | A0 display enable, no per-title switch |
| 9 | `vmachine.cpp:do_kluges` (`pendirq`, `enahirq`) | Per-title IRQ request/latch behaviour | Old IRQ model lost requests or served at the wrong boundary | Atlantis, Blockout, Wall Street | Persistent `/INT`, pre-fetch sampling, A1 clear | Level IRQ line | **Yes** (persistent-line model now available) | Hardware IRQ line model |
| 10 | `vmachine.cpp:do_kluges` (`sproff`) | Per-title sprite horizontal offset | Old sprite placement workaround | Catch the Ball, Volleyball | Object x register/beam | No per-cart offset | **Yes, after renderer parity** | Real sprite x/beam |
| 11 | `vmachine.cpp:do_kluges` (`tweakedaudio`) | Per-title audio path tweak | Old audio model | Frogger, Popeye, Musician, Helicopter Rescue+, Rally+ | i8244 sound registers/stream | Deferred audio task | Deferred | Separate audio architecture pass |
| 12 | `vmachine.cpp:handle_evbl` (Atlantis 140-line `ColorVector` special case) | Only first 140 lines refreshed | Old partial-redraw issue | Atlantis | `screen_update`/`update_now()` | Full raster | **Yes** | Unified per-line color/beam |
| 13 | `vmachine.cpp:setvideomode` / `VM_ConfigurePalRasterTiming` ordering | Per-title FPS/evblclk can change after the PAL timing model is configured | Historical init order | All `do_kluges` titles | Device geometry selected at machine start | Same | **Yes** | Configure geometry once from system/BIOS; cart cannot override |
| 14 | `cpu.cpp:T1BeamLevel` NTSC branch | Legacy `h_clk>16` / `VBLCLK` T1 | Old NTSC model | NTSC titles | `vblank()|hblank()` at `screen().hpos/vpos` | Same i8244 T1 | **Yes, after NTSC beam parity** | Same beam/T1 model as PAL |
| 15 | `cpu.cpp` counter mode legacy `h_clk` wrap / `LINECNT` | One counter increment per 21-cycle wrap | Old line counter | NTSC/legacy paths | One increment per real hblank falling edge | 456 px/line | **Yes** | Beam-edge counter |
| 16 | `c7010.cpp` + `vdc_stub.cpp` C7010 `/22 - 5` and NSC interleave | Independent C7010 raster/CPU timebase | Old C7010 compatibility | C7010 Chess | C7010/NSC800 as scheduled devices | Separate clock, same scheduler | Partly | Schedule NSC800 from the same timebase; keep only the documented C7010 character multiplex |
| 17 | `vpp_sdl.cpp` / `vpp.h` VPP compose/luminance | Separate VPP timebase/palette/luminance | Old VPP implementation | All `+` titles, G7400 BIOS | EF9340/EF9341 devices | Separate VPP hardware | Deferred (VPP phase) | Separate G7400/VPP parity pass |
| 18 | `vmachine.cpp:do_kluges` CRC bank/reset blocks | Per-CRC mapper/reset behaviour | Old cartridge mapper workarounds | Many | Cartridge slot/mapper device | Cartridge hardware | Investigate per class | Generic mapper/bank model |

## Immediate observations from the matrix

- **VP16 Depth Charge/Marksman** is one of the clearest geometry probes: the
  O2EM image loses the upper sky band while MAME keeps it. It is also a
  `setvideomode(1) + evblclk=8000` title, i.e. it sits directly on debt items
  **1, 2 and 4/5**.
- **VP15 Samurai** is restored at the grid level, but the top status row is
  missing in the automated O2EM capture while MAME shows it. This points at the
  same vertical origin/visible-area class (items 3/6), not at the object code.
- **VP31 Musician** and **Moto-Crash** produced a blank O2EM capture in the
  default run. Both are special cases (Musician has `tweakedaudio`; Moto-Crash
  is a Jopac/G7400 variant selected with the G7000 BIOS in the matrix). They
  must be re-run with the correct system/input before being called renderer
  failures.
- **VP60 Rally** is near-black in both emulators at the automated checkpoint;
  this is an input/start issue in the matrix, not yet evidence about gameplay.
- The `G7400/VPP` titles are UNKNOWN because the local MAME set lacks the
  G7400 BIOS/EF9340 charset (`g7400.bin` + `charset_ef9340_1.rom`). The O2EM
  side can still be exercised with `G7400.bin`.

## Classification rules used

- **A. Hardware-proven production semantic** → keep.
- **B. Useful A/B legacy switch** → retain only as an explicit diagnostic
  (`O2EM_I8244_LEGACY_RENDER`, `O2EM_PAL_RASTER_LEGACY_TIMING`).
- **C. Useful diagnostic trace** → retain, opt-in, no production effect.
- **D. Obsolete experiment** → remove after the replacement class is proven.
