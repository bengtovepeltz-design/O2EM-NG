#!/usr/bin/env python3
"""Summarize the Checkpoint A compatibility matrix into Markdown."""
import csv
import os
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
MATRIX = REPO / "tests" / "compat" / "matrix" / "compat_matrix.csv"
OUT = REPO / "tests" / "compat" / "reports" / "compat_summary.md"


def main():
    rows = list(csv.DictReader(MATRIX.open(encoding="utf-8-sig", newline="")))
    counts = Counter(r["severity"] for r in rows)
    lines = []
    lines.append("# O2EM-NG ↔ MAME 0.289 compatibility matrix (Checkpoint A)\n")
    lines.append("Generated from `tests/compat/matrix/compat_matrix.csv`.\n")
    lines.append("Automated triage only. PIXEL-PERFECT PASS is not claimed; palette, "
                 "animation phase, input timing and BIOS selection are separate. "
                 "ORANGE/UNKNOWN entries are probes for the architecture audit.\n")
    lines.append("## Severity summary\n")
    lines.append("| Severity | Count |")
    lines.append("|---|---:|")
    for k in ("GREEN", "YELLOW", "ORANGE", "UNKNOWN"):
        lines.append(f"| {k} | {counts.get(k, 0)} |")
    lines.append(f"| **Total** | **{len(rows)}** |\n")

    def table(sev):
        sel = [r for r in rows if r["severity"] == sev]
        if not sel:
            return
        lines.append(f"## {sev}\n")
        lines.append("| VP | Title | ROM | CRC | System | Region | MAME | O2EM | Geometry metric |")
        lines.append("|---|---|---|---|---|---|---|---|---|")
        for r in sel:
            g = r["state_alignment"].replace(";", "<br>")
            lines.append(f"| {r['vp'] or '—'} | {r['title']} | {r['filename']} | {r['crc']} | "
                         f"{r['system']} | {r['region']} | {r['mame_software'] or '—'} | "
                         f"{r['o2em_boot']} | {g} |")
        lines.append("")

    for sev in ("GREEN", "YELLOW", "ORANGE", "UNKNOWN"):
        table(sev)

    lines.append("## Notes on the ORANGE/UNKNOWN probes\n")
    lines.append("- ORANGE can be caused by a different animation/game state (input and BIOS "
                 "frame timing differ between MAME and O2EM-NG), not only by a renderer defect. "
                 "Always inspect `tests/compat/diff/<rom>_side.png` before promoting an ORANGE "
                 "item to a core defect.")
    lines.append("- UNKNOWN is expected where MAME requires the G7400/VPP BIOS/charset "
                 "(not present locally), where the ROM is a multi-game cart requiring a "
                 "specific in-cart menu key, or where the default `1` + `Enter` BIOS sequence "
                 "does not select the game.")
    lines.append("- Moto-Crash (France) and similar Jopac/G7400-only variants need the correct "
                 "system/BIOS, not the G7000 default.")
    lines.append("- Palette differences are deliberately excluded from the geometry metric "
                 "(lit-mask IoU + row/column profile correlation).\n")
    lines.append("## Reproduce\n")
    lines.append("```text")
    lines.append("python tests/compat/scripts/inventory_roms.py")
    lines.append("powershell -NoProfile -ExecutionPolicy Bypass -File tests/compat/scripts/run_matrix.ps1 -All")
    lines.append("python tests/compat/scripts/summarize_matrix.py")
    lines.append("```")
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
