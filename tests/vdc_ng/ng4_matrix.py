#!/usr/bin/env python3
"""NG-4 G7000/Odyssey2 convergence matrix.

For every locally available standard PAL G7000 cartridge with a MAME software
entry, run:
  - MAME 0.289 with NO input, snapshot at frame 500
  - the NG machine probe with NO input, 500 frames
and compare structurally. No input is the cleanest deterministic reference;
titles that require input are reported at their menu/attract state.

Writes tests/compat/matrix/compat_matrix_ng4.csv.
"""
import csv
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAME = r"C:\Users\bengt\AppData\Local\Temp\opencode\mame0289"
MAME_EXE = os.path.join(MAME, "mame.exe")
LAUNCHBOX = r"D:\LaunchBox\Games\Philips Videopac+"
PROBE = os.path.join(REPO, "tests", "vdc_ng", "bin", "Release", "vdc_render_probe.exe")
BIOS = os.path.join(REPO, "BIOS", "G7000.bin")
OUT = os.path.join(REPO, "tests", "vdc_ng", "out", "ng4")
MATRIX = os.path.join(REPO, "tests", "compat", "matrix", "compat_matrix_ng4.csv")
FRAME = 500

sys.path.insert(0, os.path.join(REPO, "tests", "vdc_ng"))
from compare_refs import read_ppm, read_png, compare  # noqa: E402


def run_mame(software, tag):
    snap = os.path.join(MAME, "snap", tag)
    if os.path.exists(snap):
        os.remove(snap)
    lua = f'''local frame=0
emu.register_frame_done(function()
  frame=frame+1
  if frame=={FRAME} then
    local s=nil
    for k,v in pairs(manager.machine.screens) do if s==nil then s=v end end
    if s then pcall(function() s:snapshot("{tag}") end) end
  end
  if frame>={FRAME} then manager.machine:exit() end
end)
'''
    lua_path = os.path.join(MAME, f"ng4_{tag}.lua")
    with open(lua_path, "w") as f:
        f.write(lua)
    subprocess.run(
        [MAME_EXE, "videopac", "-cart", software,
         "-autoboot_script", lua_path, "-nothrottle", "-skip_gameinfo",
         "-video", "none", "-seconds_to_run", "60"],
        cwd=MAME, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        timeout=120)
    return snap if os.path.exists(snap) else None


def run_ng(rom, region="pal"):
    ppm = os.path.join(OUT, "ng_" + re.sub(r"[^A-Za-z0-9_.-]", "_", os.path.basename(rom)) + ".ppm")
    subprocess.run(
        [PROBE, "--bios", BIOS, "--cart", rom, "--region", region,
         "--frames", str(FRAME), "--plan", "none", "--dump", ppm,
         "--quiet"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    return ppm if os.path.exists(ppm) else None


def crc32_file(path):
    import zlib
    with open(path, "rb") as f:
        return zlib.crc32(f.read()) & 0xFFFFFFFF


def parse_software_crcs():
    """CRC -> (software, description) from MAME's videopac software list."""
    import xml.etree.ElementTree as ET
    path = os.path.join(MAME, "hash", "videopac.xml")
    out = {}
    try:
        root = ET.parse(path).getroot()
    except Exception:
        return out
    for sw in root.findall("software"):
        name = sw.get("name", "")
        desc = ""
        d = sw.find("description")
        if d is not None and d.text:
            desc = d.text
        for rom in sw.iter("rom"):
            crc = rom.get("crc", "")
            if crc:
                out[int(crc, 16)] = (name, desc)
    return out


def find_local(entry):
    for key in ("filename", "mame_rom"):
        p = os.path.join(LAUNCHBOX, entry.get(key, ""))
        if entry.get(key) and os.path.exists(p):
            return p
    want = int(entry.get("crc32", "0"), 16)
    if want:
        for p in os.listdir(LAUNCHBOX):
            full = os.path.join(LAUNCHBOX, p)
            if not os.path.isfile(full) or not p.lower().endswith(".bin"):
                continue
            try:
                if crc32_file(full) == want:
                    return full
            except OSError:
                pass
    return None


def main():
    os.makedirs(OUT, exist_ok=True)
    inv = json.load(open(os.path.join(REPO, "tests", "compat", "matrix",
                                      "rom_inventory.json")))
    xml_crcs = parse_software_crcs()
    plus_crcs = set()
    for x in inv:
        if x.get("mame_system") == "videopacp" and x.get("crc32"):
            plus_crcs.add(int(x["crc32"], 16))
    entries = []
    covered = set()
    only = set(filter(None, os.environ.get("NG4_ONLY", "").split(",")))
    for x in inv:
        fn = x.get("filename", "")
        if only and fn not in only:
            continue
        if "C7010" in fn:
            continue
        if x.get("mame_system") != "videopac":
            continue
        if x.get("region") != "PAL":
            continue
        if x.get("special"):
            continue
        if not x.get("mame_software"):
            continue
        entries.append(x)
        if x.get("crc32"):
            covered.add(int(x["crc32"], 16))
    # Add any other local standard cartridge whose CRC is in MAME's videopac
    # list (covers publisher-prefixed filenames absent from the inventory).
    if not only:
        for p in sorted(os.listdir(LAUNCHBOX)):
            full = os.path.join(LAUNCHBOX, p)
            if not os.path.isfile(full) or not p.lower().endswith(".bin"):
                continue
            if "C7010" in p:
                continue
            try:
                crc = crc32_file(full)
            except OSError:
                continue
            if crc in covered or crc not in xml_crcs:
                continue
            if crc in plus_crcs:
                continue
            software, desc = xml_crcs[crc]
            entries.append({
                "filename": p, "title": desc, "mame_software": software,
                "region": "PAL", "special": "", "mame_system": "videopac",
                "crc32": f"{crc:08X}",
            })
            covered.add(crc)

    rows = []
    for x in entries:
        fn = x.get("filename", "")
        rom = find_local(x)
        zip_path = os.path.join(MAME, "roms", "videopac",
                                x["mame_software"] + ".zip")
        if rom is None:
            rows.append((fn, x.get("title", ""), x.get("mame_software", ""),
                         "SKIP", "", "", "", "rom missing"))
            continue
        if not os.path.exists(zip_path):
            rows.append((fn, x.get("title", ""), x.get("mame_software", ""),
                         "SKIP", "", "", "", "mame zip missing"))
            continue
        tag = "ng4_" + re.sub(r"[^A-Za-z0-9_]", "_", fn)
        snap = run_mame(x["mame_software"], tag)
        ng_ppm = run_ng(rom)
        if not snap or not ng_ppm:
            rows.append((fn, x.get("title", ""), x.get("mame_software", ""),
                         "ERROR", "", "", "", "capture failed"))
            continue
        try:
            ng = read_ppm(ng_ppm)
            mame = read_png(snap)
            r = compare(ng, mame, 45, 30, 80, 8)
            rows.append((fn, x.get("title", ""), x.get("mame_software", ""),
                         "OK", f"{r['iou']:.4f}", f"{r['row_corr']:.4f}",
                         f"{r['col_corr']:.4f}", ""))
        except Exception as e:  # pragma: no cover
            rows.append((fn, x.get("title", ""), x.get("mame_software", ""),
                         "ERROR", "", "", "", str(e)[:60]))
        print(rows[-1], flush=True)

    with open(MATRIX, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["rom", "title", "mame_software", "status", "iou",
                    "row_corr", "col_corr", "note"])
        w.writerows(rows)
    ok = [r for r in rows if r[3] == "OK"]
    green = [r for r in ok if float(r[4]) >= 0.95]
    yellow = [r for r in ok if 0.80 <= float(r[4]) < 0.95]
    orange = [r for r in ok if 0.50 <= float(r[4]) < 0.80]
    red = [r for r in ok if float(r[4]) < 0.50]
    print(f"\nrows={len(rows)} ok={len(ok)} green={len(green)} "
          f"yellow={len(yellow)} orange={len(orange)} red={len(red)}")


if __name__ == "__main__":
    main()
