#!/usr/bin/env python3
"""Checkpoint A: inventory the local Videopac/Odyssey2 ROM set.

Reads:
  <repo>/x64/Debug/ROMS/*.bin
  <repo>/x64/Debug/ROMS/Gamelist.txt
  MAME hash/videopac.xml (path via O2EM_COMPAT_MAME_HASH or a local default)

Writes:
  tests/compat/matrix/rom_inventory.csv
  tests/compat/matrix/rom_inventory.json

No ROM or BIOS bytes are copied into the repository.
"""
import csv
import hashlib
import json
import os
import re
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
ROM_DIR = Path(os.environ.get("O2EM_COMPAT_ROM_DIR", REPO / "x64" / "Debug" / "ROMS"))
GAMELIST = ROM_DIR / "Gamelist.txt"
MAME_HASH = Path(os.environ.get(
    "O2EM_COMPAT_MAME_HASH",
    r"C:\Users\bengt\AppData\Local\Temp\opencode\mame0289\hash\videopac.xml"))
OUT_DIR = REPO / "tests" / "compat" / "matrix"


def parse_gamelist(path):
    titles = {}
    if not path.exists():
        return titles
    for line in path.read_text(errors="replace").splitlines():
        m = re.match(r"^(\S+\.bin)\s+(.+?)\s*$", line.strip())
        if m:
            titles[m.group(1)] = m.group(2).strip()
    return titles


def parse_mame_hash(path):
    """Return {crc_int: [entries]} and {rom_name_lower: entry}."""
    by_crc = {}
    by_rom = {}
    if not path.exists():
        return by_crc, by_rom
    text = path.read_text(errors="replace")
    # Split into software blocks; tolerate the XML comments/DOCTYPE.
    for sm in re.finditer(r'<software\s+name="([^"]+)"([^>]*)>(.*?)</software>', text, re.S):
        name = sm.group(1)
        body = sm.group(3)
        dm = re.search(r"<description>(.*?)</description>", body, re.S)
        desc = re.sub(r"\s+", " ", dm.group(1)).strip() if dm else name
        for rm in re.finditer(r'<rom\s+name="([^"]+)"\s+size="([^"]+)"\s+crc="([0-9a-fA-F]+)"', body):
            rom_name, size, crc = rm.group(1), int(rm.group(2), 0), int(rm.group(3), 16)
            entry = {
                "software": name,
                "description": desc,
                "rom_name": rom_name,
                "size": size,
                "crc": f"{crc:08X}",
            }
            by_crc.setdefault(crc, []).append(entry)
            by_rom.setdefault(rom_name.lower(), entry)
    return by_crc, by_rom


def infer(file_name, title, mame_entry):
    low = file_name.lower()
    plus = "+" in low or "g7400" in low or low.startswith("jo_")
    if low.startswith("ntsc_") or low.startswith("br_") or "(br)" in low:
        region = "NTSC"
    elif low.startswith("jo_") or low.startswith("vp_") or "(eu" in low or "(europe" in low:
        region = "PAL"
    elif low.startswith("o2_") or low.startswith("ntsc"):
        region = "NTSC"
    else:
        region = "PAL"  # Videopac catalogue default
    if plus:
        system = "G7400/VPP"
    elif "jopac" in low:
        system = "Jopac"
    else:
        system = "G7000"
    special = ""
    if "c7010" in low or "chess" in (title or "").lower():
        special = "C7010 NSC800"
    if plus:
        special = (special + "; " if special else "") + "VPP"
    mame_system = "videopacp" if (plus or low.startswith("jo_")) else "videopac"
    return region, system, special, mame_system


def vp_sort_key(name):
    m = re.search(r"vp_(\d+)", name, re.I)
    return (int(m.group(1)) if m else 999, name.lower())


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    titles = parse_gamelist(GAMELIST)
    by_crc, by_rom = parse_mame_hash(MAME_HASH)

    rows = []
    for path in sorted(ROM_DIR.glob("*.bin"), key=lambda p: vp_sort_key(p.name)):
        data = path.read_bytes()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        entry = (by_crc.get(crc) or [None])[0]
        if entry is None:
            entry = by_rom.get(path.name.lower())
        title = titles.get(path.name, "")
        if not title and entry:
            title = entry["description"]
        region, system, special, mame_system = infer(path.name, title, entry)
        m = re.search(r"vp_(\d+)", path.name, re.I)
        vp = int(m.group(1)) if m else ""
        if "+" in path.name:
            vp_label = f"VP{vp:02d}+" if vp != "" else path.stem
        elif path.name.lower().startswith("vp_"):
            vp_label = f"VP{vp:02d}" if vp != "" else path.stem
        else:
            vp_label = ""
        rows.append({
            "vp": vp_label,
            "filename": path.name,
            "title": title,
            "crc32": f"{crc:08X}",
            "size": len(data),
            "region": region,
            "system": system,
            "special": special,
            "mame_software": entry["software"] if entry else "",
            "mame_description": entry["description"] if entry else "",
            "mame_rom": entry["rom_name"] if entry else "",
            "mame_system": mame_system if entry else "",
            "mame_match": "crc" if entry and entry["crc"] == f"{crc:08X}" else ("name" if entry else "none"),
            "known_issue": "",
        })

    # Known historical issues from the project notes.
    known = {
        "vp_48.bin": "VP48 Backgammon: historically required T1/VBL/phase and i8244 update-on-write fixes.",
        "vp_59+.bin": "VP59+ Helicopter Rescue: historical freeze/IRQ timing investigation.",
        "vp_C7010.bin": "C7010 Chess: NSC800 module; historically sensitive to raster/beam timing.",
    }
    for r in rows:
        if r["filename"].lower() in known:
            r["known_issue"] = known[r["filename"].lower()]

    fields = ["vp", "filename", "title", "crc32", "size", "region", "system", "special",
              "mame_software", "mame_description", "mame_rom", "mame_system", "mame_match", "known_issue"]
    csv_path = OUT_DIR / "rom_inventory.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    (OUT_DIR / "rom_inventory.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")

    matched = sum(1 for r in rows if r["mame_software"])
    print(f"ROMs scanned: {len(rows)}")
    print(f"MAME software matches: {matched}/{len(rows)}")
    print(f"CSV: {csv_path}")
    for r in rows:
        print(f"{r['vp'] or '-':7} {r['filename']:38} {r['crc32']} {r['size']:6} {r['region']:4} "
              f"{r['system']:10} {r['mame_software'] or '-':18} {r['title'][:40]}")


if __name__ == "__main__":
    sys.exit(main())
