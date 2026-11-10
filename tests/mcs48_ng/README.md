# MCS48-NG standalone conformance target

This is the NG-1 CPU test target. It builds **only**:

- `src/core_ng/cpu/mcs48_ng.cpp`
- `mcs48_ng_conformance.cpp`
- the deterministic `mcs48_ng_stub_bus.h`

It does **not** link the legacy `cpu.cpp`, the frontend, SDL, the VDC or audio.

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\mcs48_ng\mcs48_ng_conformance.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\mcs48_ng\mcs48_ng_conformance.vcxproj" `
  /p:Configuration=Release /p:Platform=x64
```

Outputs:

```text
tests/mcs48_ng/bin/Debug/mcs48_ng_conformance.exe
tests/mcs48_ng/bin/Release/mcs48_ng_conformance.exe
```

## Run

```powershell
.\tests\mcs48_ng\bin\Debug\mcs48_ng_conformance.exe
```

Expected:

```text
checks: 667, failures: 0
RESULT: GREEN - MCS48-NG conformance checks passed.
```

Exit code 0 = green, 1 = failures.

## Cycle-table dump

```powershell
.\tests\mcs48_ng\bin\Debug\mcs48_ng_conformance.exe --dump-cycles
```

Format:

```text
opcode,cycles,illegal
```

This is used to diff NG's table against MAME 0.289 `mcs48.cpp`.

## Coverage

- reset/power-on state
- PC 11-bit wrap with A11 preservation
- ORL/ANL BUS immediate and operand consumption
- timer 32-cycle cadence
- STRT T prescaler clear
- STRT T / STRT CNT exclusivity
- IRQ entry, EN I/DIS I, vector, RETR
- RET/RETR bank and PC-bit-11 behavior
- CALL/RET across all 8 stack depths
- RB0/RB1 banking and PSW
- MOVX, ports, BUS, 8243 expander
- JT0/JNT0/JT1/JNT1, ENT0 CLK
- ADD/ADDC exhaustive arithmetic and flags
- all 230 legal opcodes' cycles
- all 26 illegal opcodes' explicit policy
- save/restore replay determinism
- deterministic trace ring

## Notes

- The stub bus is deterministic and has no wall-clock dependencies.
- T0/T1 and /INT are CPU input methods (`SetT0`, `SetT1`, `SetIrqLevel`,
  `AcknowledgeIrq`), not bus reads.
- This target is not part of the production emulator build.
