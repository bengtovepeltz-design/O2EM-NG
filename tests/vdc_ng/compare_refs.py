#!/usr/bin/env python3
"""NG-3 structural reference comparison (no external deps).

Compares an NG indexed PPM/raw frame against a MAME PNG using the same
lit-mask cell IoU + row/column correlation metric as
tests/compat/scripts/run_matrix.ps1.

Usage:
  python compare_refs.py --ng frame.ppm --mame shot.png [--grid 45,30]
                         [--thresh 60] [--cell-pct 8] [--dump-ng out_ng.ppm]
"""

import argparse
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: " + path)
    pos = 8
    width = height = bitdepth = colortype = None
    idat = b""
    palette = None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"PLTE":
            palette = chunk
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    if bitdepth != 8:
        raise ValueError("only 8-bit PNG supported (got %d)" % bitdepth)
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colortype]
    stride = width * channels
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        filt = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if filt == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        elif filt != 0:
            raise ValueError("bad PNG filter %d" % filt)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    # Normalize to RGB triples.
    rgb = [[None] * width for _ in range(height)]
    for y in range(height):
        base = y * stride
        for x in range(width):
            o = base + x * channels
            if colortype == 2:
                r, g, b = out[o], out[o + 1], out[o + 2]
            elif colortype == 6:
                r, g, b = out[o], out[o + 1], out[o + 2]
            elif colortype == 0:
                r = g = b = out[o]
            elif colortype == 4:
                r = g = b = out[o]
            elif colortype == 3:
                idx = out[o]
                r, g, b = palette[idx * 3:idx * 3 + 3]
            rgb[y][x] = (r, g, b)
    return width, height, rgb


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"P6"):
        raise ValueError("not a P6 PPM: " + path)
    fields = []
    pos = 2
    while len(fields) < 3:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if data[pos] == 0x23:  # comment
            while data[pos] not in b"\r\n":
                pos += 1
            continue
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        fields.append(int(data[start:pos]))
    pos += 1
    w, h, _maxv = fields
    pixels = data[pos:pos + w * h * 3]
    rgb = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * 3
            rgb[y][x] = (pixels[o], pixels[o + 1], pixels[o + 2])
    return w, h, rgb


def dominant(rgb, w, h):
    counts = {}
    step = max(1, min(w, h) // 80)
    for y in range(0, h, step):
        for x in range(0, w, step):
            r, g, b = rgb[y][x]
            q = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
            counts[q] = counts.get(q, 0) + 1
    best = max(counts.items(), key=lambda kv: kv[1])[0]
    r = ((best >> 8) & 0xF) * 17
    g = ((best >> 4) & 0xF) * 17
    b = (best & 0xF) * 17
    return r, g, b


def lit(px, bg, thresh):
    return abs(px[0] - bg[0]) + abs(px[1] - bg[1]) + abs(px[2] - bg[2]) >= thresh


def corr(a, b):
    n = len(a)
    ma = sum(a) / n
    mb = sum(b) / n
    num = da = db = 0.0
    for x, y in zip(a, b):
        dx, dy = x - ma, y - mb
        num += dx * dy
        da += dx * dx
        db += dy * dy
    if da <= 1e-9 or db <= 1e-9:
        return 0.0
    return num / (da * db) ** 0.5


def compare(ng, mame, gw, gh, thresh, cell_pct):
    nw, nh, nrgb = ng
    mw, mh, mrgb = mame
    w, h = min(nw, mw), min(nh, mh)
    bga = dominant(nrgb, nw, nh)
    bgb = dominant(mrgb, mw, mh)
    cw = (w + gw - 1) // gw
    ch = (h + gh - 1) // gh
    ma = [False] * (gw * gh)
    mb = [False] * (gw * gh)
    lit_a = lit_b = 0
    bbox_a = [w, h, -1, -1]
    bbox_b = [w, h, -1, -1]
    for cy in range(gh):
        for cx in range(gw):
            ca = cb = total = 0
            for yy in range(cy * ch, min(h, (cy + 1) * ch)):
                for xx in range(cx * cw, min(w, (cx + 1) * cw)):
                    total += 1
                    if lit(nrgb[yy][xx], bga, thresh):
                        ca += 1
                        bbox_a[0] = min(bbox_a[0], xx)
                        bbox_a[1] = min(bbox_a[1], yy)
                        bbox_a[2] = max(bbox_a[2], xx)
                        bbox_a[3] = max(bbox_a[3], yy)
                    if lit(mrgb[yy][xx], bgb, thresh):
                        cb += 1
                        bbox_b[0] = min(bbox_b[0], xx)
                        bbox_b[1] = min(bbox_b[1], yy)
                        bbox_b[2] = max(bbox_b[2], xx)
                        bbox_b[3] = max(bbox_b[3], yy)
            ma[cy * gw + cx] = total > 0 and ca * 100 >= total * cell_pct
            mb[cy * gw + cx] = total > 0 and cb * 100 >= total * cell_pct
            if ma[cy * gw + cx]:
                lit_a += 1
            if mb[cy * gw + cx]:
                lit_b += 1
    inter = sum(1 for i in range(len(ma)) if ma[i] and mb[i])
    uni = sum(1 for i in range(len(ma)) if ma[i] or mb[i])
    iou = 0.0 if uni == 0 else inter / uni
    ra = [0.0] * gh
    rb = [0.0] * gh
    ca2 = [0.0] * gw
    cb2 = [0.0] * gw
    for cy in range(gh):
        for cx in range(gw):
            if ma[cy * gw + cx]:
                ra[cy] += 1
            if mb[cy * gw + cx]:
                rb[cy] += 1
    for cx in range(gw):
        for cy in range(gh):
            if ma[cy * gw + cx]:
                ca2[cx] += 1
            if mb[cy * gw + cx]:
                cb2[cx] += 1
    return {
        "iou": iou,
        "row_corr": corr(ra, rb),
        "col_corr": corr(ca2, cb2),
        "lit_ng": lit_a,
        "lit_mame": lit_b,
        "frac_ng": lit_a / (gw * gh),
        "frac_mame": lit_b / (gw * gh),
        "bbox_ng": bbox_a,
        "bbox_mame": bbox_b,
        "bg_ng": bga,
        "bg_mame": bgb,
        "cells": (ma, mb, gw, gh),
    }


def dump_ascii(result, width=90):
    ma, mb, gw, gh = result["cells"]
    lines = []
    for cy in range(gh):
        row = ""
        for cx in range(gw):
            a = ma[cy * gw + cx]
            b = mb[cy * gw + cx]
            row += "#" if a and b else ("n" if a else ("m" if b else "."))
        lines.append(row)
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ng", required=True)
    ap.add_argument("--mame", required=True)
    ap.add_argument("--grid", default="45,30")
    ap.add_argument("--thresh", type=int, default=60)
    ap.add_argument("--cell-pct", type=int, default=8)
    ap.add_argument("--ascii", action="store_true")
    args = ap.parse_args()
    gw, gh = (int(v) for v in args.grid.split(","))
    ng = read_ppm(args.ng) if args.ng.lower().endswith(".ppm") else read_png(args.ng)
    mame = read_png(args.mame)
    r = compare(ng, mame, gw, gh, args.thresh, args.cell_pct)
    print("ng   %dx%d bg=%s bbox=%s lit=%d (%.3f)" %
          (ng[0], ng[1], r["bg_ng"], r["bbox_ng"], r["lit_ng"], r["frac_ng"]))
    print("mame %dx%d bg=%s bbox=%s lit=%d (%.3f)" %
          (mame[0], mame[1], r["bg_mame"], r["bbox_mame"], r["lit_mame"],
           r["frac_mame"]))
    print("grid %dx%d  IoU=%.4f row_corr=%.4f col_corr=%.4f" %
          (gw, gh, r["iou"], r["row_corr"], r["col_corr"]))
    if args.ascii:
        print(dump_ascii(r))


if __name__ == "__main__":
    main()
