# NG host input integration conformance

Regression test for the NG host input path:

```text
SDL gamepad/keyboard
    ↓
existing input_sdl / input_manager
    ↓
JoyState
    ↓
NG adapter
    ↓
MachineInput
    ↓
MachineBus BUS joystick pins
```

It uses a real SDL gamepad if one is connected, plus SDL virtual gamepads, and
synthetic SDL events. No frontend mapping is reimplemented.

The real app additionally uses an XInput backend for Xbox-class gamepads in
`input_sdl.cpp`; this suite sets `O2EM_INPUT_DISABLE_XINPUT=1` so its synthetic
SDL event states stay deterministic. The XInput runtime path is verified by the
physical-controller trace described in
`Docs/Development/NG-Host_Input_Controller_Regression_2026-11-10.md`.

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\input_ng\input_ng_conformance.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\input_ng\input_ng_conformance.vcxproj" `
  /p:Configuration=Release /p:Platform=x64
```

The project copies `SDL3.dll` next to the executable after build.

## Run

```powershell
.\tests\input_ng\bin\Debug\input_ng_conformance.exe
.\tests\input_ng\bin\Release\input_ng_conformance.exe
```

Expected:

```text
checks: 43, failures: 0
RESULT: GREEN - NG host input integration passed.
```

## Coverage

- neutral
- up, down, left, right, fire
- diagonal up+right
- left-stick axis through the per-frame refresh path
- player 1 and player 2 independent routing
- keyboard + joystick coexistence
- repeated per-frame state refresh
- session reset and relaunch
- BUS joystick pin values (`0xFE` up, `0xFB` down, `0xF7` left, `0xFD`
  right, `0xEF` fire, `0xFF` neutral)

The test is headless and does not open an SDL window or audio device.
