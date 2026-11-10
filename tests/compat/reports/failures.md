# Failure / difference probe gallery (Checkpoint A)

These are the automated ORANGE/UNKNOWN probes from the initial matrix. Each
entry includes the side-by-side image (O2EM-NG left, MAME 0.289 right). Some
are input/state offsets, not renderer defects; they are listed so the next
architecture checkpoint can re-run them with the correct system/input.

| Probe | Matrix | Image | Observation | Provisional class |
|---|---|---|---|---|
| VP15 Samurai | ORANGE | `tests/compat/diff/vp_15.bin_side.png` | O2EM full 8×8 grid; MAME full grid **plus the top status row** (`02 / yP / 1-3?`). O2EM top labels absent/above the visible origin. | Visible-area / vertical-origin + state offset |
| VP16 Depth Charge/Marksman | ORANGE | `tests/compat/diff/vp_16.bin_side.png` | O2EM loses the upper blue sky band and flying objects; the sea starts at the top. MAME keeps the blue sky and objects. | Visible-area / vertical-origin; also a `setvideomode(1)+evblclk=8000` title |
| VP07 Mathematician/Echo | ORANGE | `tests/compat/diff/vp_07.bin_side.png` | Multi-game cart; automated `1+Enter` does not reach the same in-cart menu state. | NEEDS INPUT |
| VP19 Catch the Ball | ORANGE | `tests/compat/diff/vp_19.bin_side.png` | Different animation/scroll state; manually the game screen is complete. | STATE OFFSET |
| VP31 Musician | ORANGE | `tests/compat/diff/vp_31.bin_side.png` | O2EM capture uniform grey/blank; MAME shows the music staff. Musician also has `tweakedaudio` and a special video path. | FAIL/NEEDS INPUT — investigate audio/video start |
| VP57 | ORANGE | `tests/compat/diff/vp_57.bin_side.png` | O2EM still on BIOS `SELECT GAME`; MAME in gameplay. Automated input did not select the title. | NEEDS INPUT |
| VP60 Rally | ORANGE | `tests/compat/diff/vp_60.bin_side.png` | Both near-black at the automated checkpoint; tiny red fragments only. Not yet gameplay evidence. | NEEDS GAMEPLAY INPUT — the Phase 4 probe |
| Moto-Crash (France) | ORANGE | `tests/compat/diff/Moto-Crash__France_.bin_side.png` | O2EM blank/grey; MAME shows gameplay. This is a Jopac/G7400 variant; the matrix used the G7000 BIOS. | WRONG SYSTEM/BIOS — re-run on G7400/Jopac |
| VP59+ | UNKNOWN | `tests/compat/captures/o2em/vp_59+.bin_800.png` | MAME equivalent unavailable locally (no G7400 BIOS/EF9340 charset); O2EM gameplay captured manually. | MAME reference gap |
| C7010 | UNKNOWN | `tests/compat/captures/o2em/vp_C7010.bin_800.png` | MAME equivalent unavailable locally; O2EM board + E2-E4 verified manually. | MAME reference gap |

## Re-run rules before promoting a probe to a core defect

1. Use the correct system/BIOS (G7000 vs G7400/Jopac).
2. Use a documented start sequence or an in-cart menu key.
3. Compare the same game state (frame offset, animation phase, score/time).
4. Ignore palette; compare geometry/lit masks and visible-area origin.
5. Only then assign a subsystem and a severity.

## Matrix source

`tests/compat/matrix/compat_matrix.csv` (85 local ROMs).

Severity counts at this checkpoint:

```text
GREEN      18
YELLOW     39
ORANGE      9
UNKNOWN    19
Total      85
```
