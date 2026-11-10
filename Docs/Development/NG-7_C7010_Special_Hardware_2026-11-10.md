# NG-7 C7010 special hardware

## Result

The C7010 chess cartridge now runs on the **NG machine by default**. There is
no legacy-core fallback for the cartridge.

## Integration boundary

`src/core_ng/special/special_hardware.h` is the machine wiring point for
cartridge-required external hardware:

- P1 output
- MOVX read/write claim
- power-on/reset
- interleaved coprocessor cycles
- once-per-frame bookkeeping

`c7010_special.cpp` adapts the existing C7010 NSC800 implementation to that
interface. `MachineBus` consults the device before the normal cartridge/VDC/VPP
decode and forwards P1 writes. `Machine` owns the device lifetime and forwards
power-on, reset, per-instruction cycles and frame ticks.

The device is selected from the cartridge CRC (`77066338`) exactly as the
existing C7010 module already did. No title string, filename, button or
game-specific condition is used.

## Firmware

The NG host loads `BIOS/C7010/c7010_z80.bin` before power-on, relative to the
selected BIOS directory. Missing firmware disables coprocessor execution but
does not change the normal machine path.

## Verified path

Real Debug application, `vp_C7010.bin`, G7000 BIOS:

```text
ACTIVE CORE: NG
C7010 Chess cartridge detected (CRC 77066338)
C7010 NSC800 firmware loaded: ...\BIOS\C7010\c7010_z80.bin
[NG core] C7010 NSC800 special hardware attached
```

Entering `E2 E4` followed by ENTER produced C7010 latch traffic and the board
showed the moved pawn plus the module's reply. ENTER is therefore committed
through the NG special-hardware path, not through a key/PC special case.

## Host commands

The NG host now mirrors the legacy in-game commands from `handle_vbl()`:

| Input | Action |
|---|---|
| ESC | return to frontend |
| gamepad Back/View | return to frontend |
| F5 | reset machine |
| gamepad East/B | reset machine |
| gamepad North/Y | existing controller port swap |

Port-swap feedback and the Debug-only `ACTIVE CORE: NG` indicator reuse the
existing `VDCStub_ShowMessage` overlay. The NG host composites that overlay
after presenting its own texture, so no second notification UI was added.

## VPP horizontal orientation

The G7400 boot text exposed a real NG orientation defect: static EF9340
character-generator bytes were rendered least-significant-bit first. The
charset bytes are stored most-significant-bit first (the legacy O2EM renderer
reads them that way), so NG showed mirrored `SELECT GAME` text. ExtRAM/dynamic
character data is written by software in the opposite order and remains
rendered least-significant-bit first.

The fix reverses only static charset bytes before the existing renderer. It
does not touch the frame, composition, ExtRAM path or presentation. A synthetic
asymmetric-cell test in NG-5 locks left-to-right cell and bit orientation.

MAME 0.289 currently produces the same mirrored static charset as the old NG
path; the legacy O2EM renderer and the readable BIOS text are the orientation
reference here.
