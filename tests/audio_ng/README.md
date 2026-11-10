# NG-6 audio device conformance

Owned i8244/i8245 sound device for O2EM-NG. The device runs on the same NG
machine tick as CPU, VDC and VPP; SDL only consumes generated PCM.

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\audio_ng\audio_ng_conformance.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\Project\O2EM-NG\tests\audio_ng\audio_render_probe.vcxproj" `
  /p:Configuration=Release /p:Platform=x64
```

## Run

```powershell
.\tests\audio_ng\bin\Debug\audio_ng_conformance.exe
.\tests\audio_ng\bin\Release\audio_ng_conformance.exe
```

Expected:

```text
checks: 74, failures: 0, skips: 0
RESULT: GREEN - NG-6 audio conformance passed.
```

## Render a WAV

```powershell
.\tests\audio_ng\bin\Release\audio_render_probe.exe `
  --bios BIOS\G7000.bin `
  --cart "D:\LaunchBox\Games\Philips Videopac+\pb_popeye.bin" `
  --region pal --frames 750 --plan standard `
  --wav tests\audio_ng\out\ng6_popeye.wav
```

For G7400 titles add `--g7400` and use `BIOS\G7400.bin`.

The probe prints sample count, sample rate, start/end tick and a canonical PCM
hash. It never opens an SDL audio device.

## MAME comparison

MAME writes a WAV with:

```powershell
mame videopac -cart popeye -wavwrite mame_popeye.wav `
  -samplerate 48000 -sound none -nothrottle -video none -seconds_to_run 15
```

For scripted input use an `-autoboot_script` that calls `emu.keypost("1")` at
frame 250 and `emu.keypost("\n")` at frame 310.

Then:

```powershell
python tests\audio_ng\audio_reference_compare.py `
  --ng tests\audio_ng\out\ng6_popeye.wav --ref mame_popeye.wav `
  --mode pulse --frac 0.1
```

A synthetic 2K fixture is included at
`tests/audio_ng/fixtures/audio_synth_2k.bin`. It produces a deterministic
single-bit shift-register pulse train; NG and MAME match period and width
exactly with this fixture.

## Real hardware

See `HARDWARE_CAPTURE.md`. No hardware WAV was available during the NG-6
implementation run; the procedure and comparison tool are ready for a capture.

## Coverage

Reset, registers, prescaler, shift direction/loop, noise, amplitude, volume
duty, timestamped writes, CPU write tick, VDC HBLANK shift tick, PAL/NTSC,
48 kHz and 44.1 kHz generation, save/restore, independent-run determinism and
a golden PCM hash.
