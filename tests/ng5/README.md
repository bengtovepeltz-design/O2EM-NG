# NG-5 G7400 / Videopac+ conformance

Standalone device/machine suite for the Thomson EF9340/EF9341 VPP, its memory,
dynamic characters, composition with the i8245 VDC, save-state and the G7400
machine profile.

It builds only `src/core_ng` sources (CPU, scheduler, bus, mapper, VDC, VPP,
machine) plus the suite. It does not link legacy `cpu.cpp`, `vmachine.cpp`,
`vdc_stub.cpp`, `vpp_sdl.cpp`, SDL, the frontend or audio.

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\ng5\ng5_device_conformance.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\ng5\ng5_device_conformance.vcxproj" `
  /p:Configuration=Release /p:Platform=x64
```

Outputs:

```text
tests/ng5/bin/Debug/ng5_device_conformance.exe
tests/ng5/bin/Release/ng5_device_conformance.exe
```

## Run

```powershell
.\tests\ng5\bin\Debug\ng5_device_conformance.exe
.\tests\ng5\bin\Release\ng5_device_conformance.exe
```

Expected:

```text
checks: 105, failures: 0, skips: 0
RESULT: GREEN - NG-5 G7400 device conformance passed.
```

Exit code 0 = green, 1 = failures.

## Local assets

Real G7400 BIOS/cart checks locate assets without embedding them:

- BIOS: `BIOS/G7400.bin` (CRC `E20A9F41`).
- Carts: `O2EM_NG_ROM_DIR`, `D:\LaunchBox\Games\Philips Videopac+`, `Roms`.
- Root override: `O2EM_NG_ROOT`.

If optional real assets are absent, those specific checks print `SKIP`; the
synthetic device checks still run.

## Coverage

- G7400 profile values and both VBL phases
- EF9341 command/data protocol, A/B latch, cursor and wrap
- A/B display RAM and extended RAM
- dynamic character upload/readback/overwrite/reset
- charset size/CRC/provenance
- double width/height, inversion, service row, display-off
- VDC intensity preservation, LUM, B-pin masking, colour-key transparency
- mid-frame VPP update timing
- VPP save/load and resumed-frame hash
- machine save/restore and independent-run determinism
- real G7400 BIOS + VP60/VP59+/Tutankham execution
- standard G7000 cartridge on the G7400 machine
