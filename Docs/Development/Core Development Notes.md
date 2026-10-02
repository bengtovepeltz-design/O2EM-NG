# Core development notes (audio, video, CPU)

These notes consolidate the durable technical outcomes of the incremental
development notes that were kept in the repository root during development
(`README_Patch_*`, `README_Rollback_*` and related files). Those working notes
were temporary scaffolding and have been removed from the project root; this is
a curated summary of what they established, not a changelog.

For release-visible changes see `../CHANGELOG.md`. For project history and the
current state see `../Project.md`.

## Intel 8244/8245 audio

- The active sound core is `audio8245.cpp` / `audio8245.h`, selected by
  `O2EM_USE_AUDIO8245` in `audio_config.h`. `audio_sdl.cpp` provides the SDL3
  output stage and the emulator-timed ring buffer.
- The 24-bit sound shift register persists across SDL audio buffers.
- Shifting is clocked from emulated horizontal lines rather than from the
  44.1 kHz output-sample counter. Control bit 5 selects one shift every 4 or 16
  HBLANK pulses. PAL line rate is 15625 Hz; NTSC is approximately 15734.26 Hz.
- A7-A9 writes load a separate pending register. The full 24-bit value is
  committed atomically at a HBLANK boundary, and a pending shift is preserved
  while a register write is being serviced (MAME-style write/pending handling).
- Noise feedback is generated inside the 24-bit shift register using taps 0 and
  5 and looped into bit 23 when recirculation is enabled; AA bit 6 is treated as
  unconnected.
- Volume is a 16-step duty cycle rendered as its audible average rather than as
  direct amplitude scaling.
- Audio is produced once per emulated frame into a ring buffer (about 882
  samples/frame at PAL 50 Hz; about 735/frame at NTSC 60 Hz for a 44.1 kHz
  output), and SDL only drains already-generated samples so temporary queue
  fullness cannot stall the 8244 state.
- For `SDL_AUDIO_U8`, silence is the midpoint value `0x80` (a zero byte is full
  negative deflection, not silence).
- Debug diagnostics can emit `sound_signal_walk.log` and
  `sound_output_u8_44100_mono.raw` from `audio_sdl.cpp`. These are generated
  outputs and are ignored by Git.

## Videopac+ / G7400 (VPP)

- `vpp_sdl.cpp` and `vpp_cset.cpp/.h` were restored from Mark Guttenbrunner's
  working O2EM 1.20/1.21 VPP core. The earlier `vpp_stub.cpp` is no longer
  compiled.
- Character rendering uses the real EF9340/EF9341 10-scanline tables; dynamic
  character bit order, double-width source selection and the original overlay
  position (x=9, y=5) are preserved.
- Transparency is interpreted as in the original: the EF934x TraReg selects
  which colours of the underlying 8244/G7000 framebuffer become transparent so
  the VPP layer is visible, not the other way round.
- Double-width/double-height and serial attribute state latch and advance with
  alternating parity, matching the original state machine.
- The VPP collision bridge (patch 0029G: `load_colplus()` contributing
  `COL_VPP` records into the collision mask) was tried and then **rolled back**
  because it caused a regression; the known-good VPP renderer was restored.
  The current `clear_collision()` in `vdc_stub.cpp` simply clears the collision
  state with `memset`, and `COL_VPP` is defined but unused. Do not reintroduce
  that bridge without fresh testing.
- VDC compatibility restored in `vmachine.cpp`: object-RAM write inhibit while
  foreground objects are active, grid-write "garbage" modelling and VDC
  readback behaviour, matching the original core.

## C7010 Chess module (NSC800/Z80)

- `c7010.cpp` / `c7010.h` detect `vp_C7010.bin` (CRC32 `77066338`) and load the
  separate 8 KiB firmware from `BIOS/C7010/c7010_z80.bin`. The module models the
  8 KiB ROM plus 2 KiB mirrored RAM, two communication latches, the P1 reset
  control and gated 8048-side bus hooks.
- The NSC800 executor supports the Z80 `0x40`-`0xBF` register/ALU matrix,
  DD/FD (IX/IY) prefixes including indexed and DDCB/FDCB forms, the ED-prefix
  families (16-bit loads, block operations, NEG aliases, RETN/RETI, IM modes,
  RRD/RLD, I/O), and IM0/1/2 interrupt state.
- The two processors are interleaved: the NSC800 runs about 2 instructions per
  8048 instruction. Earlier approaches ran large per-frame bursts and raced the
  coprocessor ahead of the cartridge/VDC side.
- Interface select follows the service-manual P10 + P14 model; a P10-only
  attempt was rolled back. P11 remains the NSC800 reset.
- Confirmed interface/bus observations worth keeping: the 8048 reaches the
  C7010 only through the A7/A5-selected MOVX read window
  (`(address & 0xA0) == 0xA0`); with P16 high, cartridge writes are blocked
  while VDC writes are still delivered (P1=AE / C0 writes remain accepted).
- Confirmed keyboard/C7010 path: ENTER arrives through the Videopac keyboard
  matrix on row 5 and reaches the C7010 cartridge 8048 at PC=0BE, which then
  runs a counted debounce loop around PC 0C4-0C8 (`IN A,P2` / `XRL A,R0` /
  `JNZ` / `DJNZ R2`). This is the reference path if ENTER handling regresses.
- Confirmed raster correction: for C7010 the master clock is mapped with
  `/22 - 5` to match the VDC beam latch. Example from the logs: clock 3991
  maps to line 176 (where the lower objects sit at Y=176), and clock 5114 maps
  to line 227 instead of 250 under the old `/20 - 5` mapping.
- Known applied fixes: the JR displacement is fetched before the updated PC is
  read (so the tested E2 case decodes correctly instead of as G2), and a
  renderer-only character snapshot across foreground-disabled row rewrites
  reduces flicker and clipping (a compatibility experiment, not a verified
  hardware latch model).
- The shared 8048 core also gained a pending timer/counter interrupt fix: an
  enabled overflow that occurs during an active interrupt sets a pending flag
  instead of being lost, and is delivered after RETR without nesting. This
  affects ordinary games as well as the C7010 path.
- Detailed C7010/NSC800 tracing is off by default in Debug and Release; rebuild
  with `O2EM_C7010_TRACE=1` to re-enable it. Detection, firmware-failure,
  unsupported-instruction and HALT reports remain visible.

## Catalogue identity, media and UI

- Game Data has an editable Catalog ID (for example `54`, `54+`, `C7010`)
  stored as `user_catalog_id`. Saving normalizes the installed ROM filename and
  syncs the `rom_filename` key. The Library and Game Information views use the
  Catalog ID, and catalogue sorting places a `+` variant directly after its base
  number with hardware IDs after the numbered releases.
- Media identity no longer requires an official numeric ID. Resolution falls
  back in order: user/official Catalog ID, numeric Videopac number, installed
  ROM filename stem, stored ROM filename stem. Media is scanned after the
  database identity is restored, and the box-art cache key includes the
  resolved path, so custom IDs no longer lose covers, manuals or screenshots.
- Universal ROM import loads any `.bin`/`.rom` files, not just official
  catalogue entries; unknown ROMs get an editable database row.
- Safe delete and the Win95-style Settings/Quick Settings drop-downs
  (outside-click and Escape/controller-B cancel, installed-firmware filtering,
  visible selection highlight) were added here; the Contributors page was
  retired in favour of Credits and Special Thanks.
