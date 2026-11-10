#!/usr/bin/env python3
"""Create missing MAME videopac software zips from local ROM files by CRC.

The local compatibility lab only materialised zips for the inventory subset.
For NG-4 we compare the broad local library, so build any missing standard
(videopac) software zip whose ROM CRCs are present locally. This only uses the
local user ROM library and MAME's public software-list metadata; it does not
download anything.
"""
import os
import sys
import xml.etree.ElementTree as ET
import zipfile

MAME = r"C:\Users\bengt\AppData\Local\Temp\opencode\mame0289"
LAUNCHBOX = r"D:\LaunchBox\Games\Philips Videopac+"
ROMS = os.path.join(MAME, "roms", "videopac")
XML = os.path.join(MAME, "hash", "videopac.xml")


def crc32_file(path):
    import zlib
    with open(path, "rb") as f:
        return zlib.crc32(f.read()) & 0xFFFFFFFF


def local_map():
    m = {}
    for p in os.listdir(LAUNCHBOX):
        full = os.path.join(LAUNCHBOX, p)
        if not os.path.isfile(full) or not p.lower().endswith(".bin"):
            continue
        try:
            m.setdefault(crc32_file(full), full)
        except OSError:
            pass
    return m


def main():
    os.makedirs(ROMS, exist_ok=True)
    lm = local_map()
    root = ET.parse(XML).getroot()
    made = 0
    for sw in root.findall("software"):
        name = sw.get("name", "")
        zpath = os.path.join(ROMS, name + ".zip")
        if not name or os.path.exists(zpath):
            continue
        roms = []
        for rom in sw.iter("rom"):
            crc = rom.get("crc", "")
            rn = rom.get("name", "")
            if not crc or not rn:
                roms = []
                break
            src = lm.get(int(crc, 16))
            if not src:
                roms = []
                break
            roms.append((rn, src))
        if not roms:
            continue
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
            for rn, src in roms:
                z.write(src, rn)
        made += 1
        print("made", name, [r[0] for r in roms])
    print("created", made, "zips")


if __name__ == "__main__":
    main()
