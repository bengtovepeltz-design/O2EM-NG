# O2EM-NG differential test lab (Checkpoint A)

Local MAME 0.289 ↔ O2EM-NG Debug comparison station for the Videopac /
Odyssey² library.

Checkpoint A scope: **test station, inventory, matrix, debt inventory and
architecture notes only — no production core changes.**

## Layout

```text
tests/compat/
  scripts/
    inventory_roms.py       ROM/CRC/MAME software inventory
    run_matrix.ps1           MAME ↔ O2EM-NG differential runner
    summarize_matrix.py      Markdown summary
  matrix/
    rom_inventory.csv/.json  ROM inventory
    compat_matrix.csv        compatibility matrix (the baseline)
  captures/mame/             MAME screenshots (generated)
  captures/o2em/             O2EM-NG screenshots (generated)
  diff/                      side-by-side O2EM | MAME images (generated)
  traces/                    per-title MAME/O2EM logs (generated)
  reports/
    compat_summary.md        matrix summary
    legacy_debt.md           legacy O2EM compatibility debt inventory
    architecture.md          MAME vs O2EM-NG time/data flow + ranked mismatches
```

Generated captures/logs are ignored by `tests/compat/.gitignore`; the matrix
and reports are intended reference output.

## Prerequisites

- O2EM-NG Debug build at `x64\Debug\O2EM-NG.exe`
- ROMs at `x64\Debug\ROMS`, BIOS at `x64\Debug\BIOS`
- MAME 0.289 with the `videopac` software list (default:
  `C:\Users\bengt\AppData\Local\Temp\opencode\mame0289`)
- Windows PowerShell 5.1+ / .NET `System.Drawing`
- Python 3 for the inventory/summary scripts

## Run

```powershell
# 1. Inventory ROMs, CRCs and MAME software matches
python tests\compat\scripts\inventory_roms.py

# 2. Differential matrix (all local ROMs; use -Titles for a subset)
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tests\compat\scripts\run_matrix.ps1 -All -Frames "500,800"

# 3. Markdown summary
python tests\compat\scripts\summarize_matrix.py
```

Useful overrides:

```powershell
-Frames "500,800,1200"
-Titles "vp_01.bin,vp_48.bin,vp_C7010.bin,vp_59+.bin"
-MameRoot "D:\path\to\mame"
-RomDir   "D:\path\to\roms"
-BiosDir  "D:\path\to\bios"
-O2Exe    "D:\path\to\O2EM-NG.exe"
```

## Start/input sequences

The runner uses these defaults:

- standard single-game cart: BIOS `1` + `Enter` (MAME frames 250/310;
  O2EM `120:28:4,150:38:4`)
- VP48: `6` + `N` (`69:33:6,109:14:6`)
- C7010: `1` + `Y` + `2` (`120:28:4,180:25:4,240:29:4`)
- VP59+: `1` + `Enter` (`120:28:4,150:38:4`)

Add a title-specific plan in `Get-KeyPlan` in `run_matrix.ps1` when a manual or
documented sequence is known. Multi-game carts and titles that remain on the
BIOS `SELECT GAME` screen are reported as `UNKNOWN / NEEDS INPUT`, not as
renderer failures.

## Severity

- **GREEN** — lit-mask IoU ≥ 0.55 and row/column correlation ≥ 0.72
- **YELLOW** — IoU ≥ 0.30 or correlation ≥ 0.55
- **ORANGE** — below those thresholds (inspect the side-by-side image; may be
  state/input offset)
- **UNKNOWN** — no MAME equivalent/BIOS, no input start, or one side blank

The geometry metric deliberately ignores palette: it compares a quantized
background mask, row/column lit profiles and a 40×30 lit-cell IoU.

## Known limitations

- The local MAME set lacks the G7400/VPP BIOS and EF9340 charset, so all `+`
  titles are UNKNOWN for MAME. O2EM-NG is still exercised with `G7400.bin`.
- MAME and O2EM-NG boot/input frame counts differ, so some ORANGE entries are
  animation/game-state offsets rather than renderer defects. Always inspect
  `diff/<rom>_side.png`.
- Palette is excluded by design. Audio is out of scope.
- The matrix is an initial baseline, not a pixel-perfect acceptance suite.
