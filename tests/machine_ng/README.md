# NG-2 machine conformance target

Standalone NG-2 test target. It builds only:

```text
src/core_ng/cpu/mcs48_ng.cpp
src/core_ng/scheduler.cpp
src/core_ng/memory/{cartridge,mapper,bus}.cpp
src/core_ng/video/stub_vdc.cpp
src/core_ng/diagnostics/bus_trace.cpp
src/core_ng/machine.cpp
machine_ng_conformance.cpp
```

It does not link legacy `cpu.cpp`, `vmachine.cpp`, `vdc_stub.cpp`, SDL, the
frontend or audio.

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\machine_ng\machine_ng_conformance.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\machine_ng\machine_ng_conformance.vcxproj" `
  /p:Configuration=Release /p:Platform=x64
```

Outputs:

```text
tests/machine_ng/bin/Debug/machine_ng_conformance.exe
tests/machine_ng/bin/Release/machine_ng_conformance.exe
```

## Run

```powershell
.\tests\machine_ng\bin\Debug\machine_ng_conformance.exe
.\tests\machine_ng\bin\Release\machine_ng_conformance.exe
```

Expected:

```text
checks: 357, failures: 0, skips: 0
RESULT: GREEN - NG-2 machine conformance passed.
```

Exit code 0 = green, 1 = failures.

## Local assets

Real BIOS/cart tests locate assets without embedding them:

- BIOS: `BIOS/G7000.bin` under the project root.
- Carts: `O2EM_NG_ROM_DIR`, or
  `D:\LaunchBox\Games\Philips Videopac+`, or `Roms/`.
- Root override: `O2EM_NG_ROOT`.

The synthetic fixture is always present:

```text
tests/machine_ng/fixtures/synth_std_2k.bin
tests/machine_ng/fixtures/synth_std_4k_banks.bin
tests/machine_ng/fixtures/mame_synth_std_2k.txt
```

If optional real assets are absent, those specific checks print `SKIP`; the
synthetic, unit and MAME-fixture checks still run.

## Coverage

- timebase: 12 ticks/CPU cycle, PAL/NTSC line/frame/VBL phases
- scheduler: (tick, priority, sequence) order, pending snapshot/replay
- cartridge classification and standard/3K/Rally mappers
- BIOS/cartridge program decode
- external RAM and open bus at 0x00/0x7F/0x80/0xFF
- P1/P2 chip-select, keyboard and joystick decode
- MOVX routing and trace fields
- PAL/NTSC frame execution
- power-on vs reset
- IRQ device->CPU, DIS I/EN I hold, RETR, second VBL, timer IRQ
- T0/T1 branches, counter edges, overflow, scheduled transitions
- scheduled joystick/keyboard input
- full-machine save/restore, including a pending scheduled event
- bus trace filters and replay
- real G7000 BIOS + vp_01 deterministic prefix and MAME prefix differential
- five real cartridge layouts (2K/4K/8K/12K 3K-page/16K Rally)
- MAME 0.289 runtime differential (15,779-instruction 2K PC hash + final state)
- MAME 0.289 4KB bank-switching differential (15,767 PC hash + final state)

## MAME differential

`fixtures/mame_synth_std_2k.txt` records the MAME 0.289 commands, PC hash,
register checkpoint and the real-BIOS prefix hash. The NG-2 target compares
both without invoking MAME at test time.
