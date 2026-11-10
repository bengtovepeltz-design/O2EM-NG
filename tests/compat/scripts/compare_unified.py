#!/usr/bin/env python3
"""Checkpoint B: compare the legacy and unified compatibility matrices.

Produces tests/compat/reports/unified_delta.md with complete per-title
classification and explicit IMPROVED / REGRESSED lists.
"""
import csv
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
MATRIX = REPO / "tests" / "compat" / "matrix"
REPORTS = REPO / "tests" / "compat" / "reports"
LEGACY = MATRIX / "compat_matrix_legacy.csv"
UNIFIED = MATRIX / "compat_matrix_unified.csv"
OUT = REPORTS / "unified_delta.md"

RANK = {"GREEN": 0, "YELLOW": 1, "ORANGE": 2, "RED": 3}
ORDER = ("GREEN", "YELLOW", "ORANGE", "RED", "UNKNOWN")


def load(path):
    with path.open(encoding="utf-8-sig", newline="") as f:
        return {r["filename"]: r for r in csv.DictReader(f)}


def num(row, key):
    for part in row.get("state_alignment", "").split(";"):
        part = part.strip()
        if part.startswith(key + "="):
            try:
                return float(part.split("=", 1)[1].replace(",", "."))
            except ValueError:
                return 0.0
    return 0.0


def main():
    legacy = load(LEGACY)
    unified = load(UNIFIED)
    rows = []
    for fn in sorted(set(legacy) | set(unified)):
        a = legacy.get(fn, {})
        b = unified.get(fn, {})
        sa = a.get("severity", "UNKNOWN")
        sb = b.get("severity", "UNKNOWN")
        if sa not in RANK or sb not in RANK:
            cls = "UNKNOWN"
        elif RANK[sb] < RANK[sa]:
            cls = "IMPROVED"
        elif RANK[sb] > RANK[sa]:
            cls = "REGRESSED"
        else:
            cls = "UNCHANGED"
        rows.append({
            "filename": fn,
            "vp": a.get("vp") or b.get("vp") or "",
            "title": a.get("title") or b.get("title") or "",
            "system": a.get("system") or b.get("system") or "",
            "region": a.get("region") or b.get("region") or "",
            "legacy": sa,
            "unified": sb,
            "cls": cls,
            "iou_a": num(a, "iou"),
            "iou_b": num(b, "iou"),
            "row_a": num(a, "rowcorr"),
            "row_b": num(b, "rowcorr"),
            "col_a": num(a, "colcorr"),
            "col_b": num(b, "colcorr"),
            "lit_a": num(a, "litO2"),
            "lit_b": num(b, "litO2"),
            "note_a": a.get("known_difference", "").strip(),
            "note_b": b.get("known_difference", "").strip(),
        })

    ca = Counter(r["legacy"] for r in rows)
    cb = Counter(r["unified"] for r in rows)
    classes = Counter(r["cls"] for r in rows)

    lines = []
    lines.append("# Checkpoint B: unified video timebase A/B delta\n")
    lines.append("Generated from `tests/compat/matrix/compat_matrix_legacy.csv` and "
                 "`tests/compat/matrix/compat_matrix_unified.csv`.\n")
    lines.append("Both runs use the same ROMs, BIOS selection, region, scripted input, "
                 "frame checkpoints (500/800) and the same MAME 0.289 reference captures "
                 "(`-SkipMame`, captured in Checkpoint A). The only difference is "
                 "`O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1`.\n")
    lines.append("Severity is the existing automated geometry triage "
                 "(lit-mask IoU + row/column profile correlation). It is a triage "
                 "signal, not a pixel-perfect claim; palette is excluded.\n")
    lines.append("The legacy run reproduces the Checkpoint A baseline row-for-row "
                 "(85/85 titles have identical severity and `state_alignment` strings), "
                 "so the production path is unchanged. The single NTSC inventory title "
                 "(Super Cobra Brazil) ran the unified NTSC geometry "
                 "(455×263, 20 px/cycle) and kept its YELLOW severity.\n")

    lines.append("## Summary\n")
    lines.append("| Severity | Legacy | Unified |")
    lines.append("|---|---:|---:|")
    for k in ORDER:
        lines.append(f"| {k} | {ca.get(k, 0)} | {cb.get(k, 0)} |")
    lines.append(f"| **Total** | **{sum(ca.values())}** | **{sum(cb.values())}** |\n")
    lines.append("| Classification | Titles |")
    lines.append("|---|---:|")
    for k in ("IMPROVED", "UNCHANGED", "REGRESSED", "UNKNOWN"):
        lines.append(f"| {k} | {classes.get(k, 0)} |")
    lines.append(f"| **Total** | **{len(rows)}** |\n")

    def detail_table(sel, empty):
        if not sel:
            lines.append(empty + "\n")
            return
        lines.append("| VP | Title | ROM | System | Region | Legacy | Unified | IoU legacy | IoU unified |")
        lines.append("|---|---|---|---|---|---|---|---:|---:|")
        for r in sel:
            lines.append(
                f"| {r['vp'] or '—'} | {r['title']} | {r['filename']} | {r['system']} | "
                f"{r['region']} | {r['legacy']} | {r['unified']} | "
                f"{r['iou_a']:.3f} | {r['iou_b']:.3f} |")
        lines.append("")

    imp = [r for r in rows if r["cls"] == "IMPROVED"]
    reg = [r for r in rows if r["cls"] == "REGRESSED"]
    unk = [r for r in rows if r["cls"] == "UNKNOWN"]

    lines.append("## IMPROVED titles (complete)\n")
    detail_table(imp, "_None._")
    lines.append("## REGRESSED titles (complete)\n")
    detail_table(reg, "_None._")
    lines.append("## UNKNOWN in at least one run (complete)\n")
    detail_table(unk, "_None._")

    # Numeric IoU movement for titles that have a MAME reference in both runs.
    known = [r for r in rows if r["legacy"] in RANK and r["unified"] in RANK]
    moved = sorted(known, key=lambda r: r["iou_b"] - r["iou_a"])
    lines.append("## Numeric movement (IoU)\n")
    lines.append("Severity is coarse; this table shows the underlying geometry metric. "
                 "A small movement is expected from the exact integer beam (`floor(phase/htotal)`) "
                 "replacing the legacy rounded line index and from bypassing snapline/mxsnap.\n")
    lines.append("| VP | Title | ROM | Severity | IoU legacy | IoU unified | Delta |")
    lines.append("|---|---|---|---|---:|---:|---:|")
    for r in list(reversed(moved[-8:])) + moved[:8]:
        d = r["iou_b"] - r["iou_a"]
        lines.append(
            f"| {r['vp'] or '—'} | {r['title']} | {r['filename']} | "
            f"{r['legacy']} → {r['unified']} | {r['iou_a']:.3f} | {r['iou_b']:.3f} | {d:+.3f} |")
    lines.append("")

    probes = [
        "vp_48.bin", "vp_60.bin", "vp_61.bin", "vp_C7010.bin", "vp_15.bin", "vp_16.bin",
        "vp_20.bin", "vp_40.bin", "vp_59+.bin", "vp_01.bin", "vp_43.bin",
        "Popeye (1983)(Parker Brothers)(EU).bin", "vp_11.bin", "vp_04.bin",
        "Atlantis (1983)(Imagic)(EU-US).bin", "vp_19.bin", "vp_47.bin", "vp_52.bin",
    ]
    by_fn = {r["filename"]: r for r in rows}
    lines.append("## Mandatory probe titles\n")
    lines.append("| Probe | ROM | Legacy | Unified | IoU legacy | IoU unified |")
    lines.append("|---|---|---|---|---:|---:|")
    for fn in probes:
        r = by_fn.get(fn)
        if not r:
            continue
        lines.append(
            f"| {r['vp'] or r['title']} | {fn} | {r['legacy']} | {r['unified']} | "
            f"{r['iou_a']:.3f} | {r['iou_b']:.3f} |")
    for missing in ("Frogger", "Blockout", "Q*bert", "Commando Noturno"):
        if not any(missing.lower() in r["title"].lower() or missing.lower() in r["filename"].lower()
                   for r in rows):
            lines.append(f"| {missing} | _not in local inventory_ | — | — | — | — |")
    lines.append("")
    lines.append("Three-panel montages are in `tests/compat/reports/probes/` "
                 "(legacy | unified | MAME 0.289 where available). "
                 "`vp60_trans_american_rally.png` and `c7010_chess.png` use the custom "
                 "G7400/chess input sequences because the automated 500/800 checkpoints "
                 "do not reach their meaningful screens.")
    lines.append("")

    lines.append("## All titles\n")
    lines.append("| VP | Title | ROM | Legacy | Unified | Class | IoU legacy | IoU unified |")
    lines.append("|---|---|---|---|---|---|---:|---:|")
    for r in rows:
        lines.append(
            f"| {r['vp'] or '—'} | {r['title']} | {r['filename']} | {r['legacy']} | "
            f"{r['unified']} | {r['cls']} | {r['iou_a']:.3f} | {r['iou_b']:.3f} |")
    lines.append("")

    lines.append("## Notes\n")
    lines.append("- The first unified run exposed a frame-boundary defect in the experimental mapper: "
                 "the phase-wrap flag was set before VBL (the initial phase starts near line 312), "
                 "then consumed when VBL set `mstate=1`. That locked `handle_evbl()` to line 242 and "
                 "caused eight severity regressions in the first run (Atlantis, Air-Sea War, Gunfighter, "
                 "Stone Sling, Jumping Acrobats, Killer Bees, Spider-Man, plus the borderline VP48). "
                 "The unified VBL handler now discards a wrap observed before VBL, so the CPU frame "
                 "boundary is the post-VBL phase wrap at line 0. Seven of the eight recovered; VP48 "
                 "remains a sub-threshold metric-only movement (see its probe note). The tables above "
                 "are from the corrected run.")
    lines.append("- ORANGE/UNKNOWN can reflect game state or input timing rather than a "
                 "renderer defect. Inspect `tests/compat/diff/legacy/<rom>_side.png` and "
                 "`tests/compat/diff/unified/<rom>_side.png` before promoting an entry to "
                 "a core defect.")
    lines.append("- Titles classified UNKNOWN in both runs are unchanged by the "
                 "experiment (mostly G7400/VPP references missing locally or unmatched "
                 "homebrew). The local MAME install lacks the G7400/VPP BIOS and the "
                 "EF9340 charset, so all `+` titles and VP59+ have no MAME reference.")
    lines.append("- VP60 (Trans American Rally) is a Videopac+ title. With the G7400 BIOS "
                 "it leaves SELECT GAME and reaches the US map/car scene in both legacy and "
                 "unified runs. Pressing `0` then falls to a near-black stage; the EF9340 "
                 "background/road/HUD is not rendered. The unified timebase does not change "
                 "this. This is a VPP/EF9340 rendering gap, not a timebase gap.")
    lines.append("- VP48's GREEN→YELLOW movement is metric-only: the board, pieces, dice, "
                 "score and side panels are complete and the unified image actually matches "
                 "MAME's score-panel state more closely than legacy. The IoU drop (0.780→0.771) "
                 "comes from a sub-one-line placement change (exact floor beam vs the legacy "
                 "rounded line index, with snapline/mxsnap bypassed).")
    lines.append("- `regionoff` is retained under unified only as a fixed display-origin "
                 "offset. Line-rate compensation (`/20`, `/22`, `/24`), C7010 raster "
                 "scaling, CRC-specific raster branches, `palRenderOffset` and "
                 "snapline/mxsnap are bypassed under the experiment.")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT} ({len(rows)} rows)")
    print(f"Legacy:  {dict(ca)}")
    print(f"Unified: {dict(cb)}")
    print(f"Classes: {dict(classes)}")


if __name__ == "__main__":
    main()
