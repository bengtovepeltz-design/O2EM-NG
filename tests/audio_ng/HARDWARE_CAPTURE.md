# Real G7000 audio capture procedure (NG-6)

The objective is to settle hardware behaviour that MAME and the NG device
cannot prove alone: exact tone pitch, pulse length, cadence and the
CPU/event-to-audio relationship.

No real-hardware WAV was supplied during the NG-6 implementation run. The
procedure and comparison tool are complete; when a capture exists, run the
comparison command below and record the result in the NG-6 matrix.

## Equipment

- Working Philips G7000 (PAL).
- TV/RF output or a suitable audio tap.
- Focusrite Scarlett Studio interface.
- Cubase 10.5 (or any recorder that writes uncompressed PCM).
- Optional: phone camera recording the screen for event-to-audio alignment.

## Recording

- Sample rate: 48 000 Hz.
- Bit depth: 24-bit or 16-bit PCM; convert to 16-bit PCM before comparison.
- Mono.
- Disable EQ, compression, noise reduction, reverb, normalization and
  automatic gain.
- Set a conservative input level; do not clip.
- Record 10–20 seconds of each case.
- Keep a short visual/verbal start marker (screen flash or controller press)
  in the recording for event timing.

## Cases

1. Musician (VP31): a single note, a scale or a repeated pattern.
2. Frogger: hop/jump and collision sounds.
3. Popeye: repeated movement and collision sounds.
4. One simple early G7000 title with a long steady tone.
5. One noise/explosion-heavy title.
6. One rapid pitch-change title.
7. Helicopter Rescue+ (VP59) and Rally (VP60) on a G7400, if a G7400 is
   available. Otherwise these remain MAME/G7400-machine references.

## Conversion

Convert the recording to `hardware_<case>.wav` (mono, 48 kHz, 16-bit PCM).
Trim the start so the first analysed event is inside the file; do not
normalize or filter.

## Comparison

```powershell
python tests\audio_ng\audio_reference_compare.py `
  --ng tests\audio_ng\out\ng6_<case>.wav `
  --ref hardware_<case>.wav --mode pulse --frac 0.1
```

For a continuous tone:

```powershell
python tests\audio_ng\audio_reference_compare.py `
  --ng tests\audio_ng\out\ng6_<case>.wav `
  --ref hardware_<case>.wav --mode tone
```

`pulse` compares median pulse period and width; `tone` compares the
autocorrelation fundamental. RMS/peak comparison is separate because the
G7000 RF/TV path and MAME's 0.25 device route gain are not calibrated.

## Acceptance for a hardware case

- Tone frequency within 2 %.
- Pulse period within 1 %.
- Pulse width within 25 % (one 48 kHz sample at the shortest pulses is
  already ~7 %).
- Cadence/order of events matches the visual marker.

Record the file name, capture date, game, ROM CRC, input actions and measured
values. Do not tune the NG device to a single recording; if hardware and MAME
disagree, treat the measured hardware as authority and document the
difference.
