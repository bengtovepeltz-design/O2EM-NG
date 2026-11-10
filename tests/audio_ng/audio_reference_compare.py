#!/usr/bin/env python3
"""Objective NG-6 audio comparison (no external dependencies).

Modes:
  pulse : detect pulse starts and widths (synthetic shift-register fixture)
  tone  : estimate fundamental via zero crossings / autocorrelation
  rms   : compare peak/RMS only

The comparison is intentionally about hardware-visible timing: period,
pulse width, cadence and relative amplitude. It does not require sample-exact
phase alignment, because MAME and NG start their host WAV capture at slightly
different instruction/buffer boundaries.
"""
import argparse
import math
import struct
import wave


def read_wav(path):
    w = wave.open(path, "rb")
    channels = w.getnchannels()
    width = w.getsampwidth()
    rate = w.getframerate()
    raw = w.readframes(w.getnframes())
    w.close()
    if width != 2:
        raise SystemExit(f"{path}: only 16-bit PCM supported")
    n = len(raw) // 2
    s = struct.unpack("<%dh" % n, raw)
    if channels == 2:
        s = [(s[i] + s[i + 1]) // 2 for i in range(0, len(s) - 1, 2)]
    return rate, list(s)


def runs(samples, peak, frac=0.25, min_width=2):
    threshold = max(1, int(peak * frac))
    out = []
    i = 0
    n = len(samples)
    while i < n:
        if abs(samples[i]) >= threshold:
            j = i
            while j < n and abs(samples[j]) >= threshold:
                j += 1
            out.append((i, j - i))
            i = j
        else:
            i += 1
    return out


def median(values):
    values = sorted(values)
    if not values:
        return 0.0
    m = len(values) // 2
    return float(values[m]) if len(values) % 2 else (values[m - 1] + values[m]) / 2.0


def pulse_stats(samples, peak, frac, min_width):
    r = [x for x in runs(samples, peak, frac, min_width) if x[1] >= min_width]
    if len(r) < 2:
        return None
    starts = [x[0] for x in r]
    widths = [x[1] for x in r]
    periods = [b - a for a, b in zip(starts, starts[1:])]
    return {"count": len(r), "period": median(periods), "width": median(widths)}


def tone_frequency(samples, rate):
    # Autocorrelation over a 40..4000 Hz lag window, using a bounded middle
    # slice to keep the analysis cheap.
    n = len(samples)
    if n < rate // 10:
        return 0.0
    start = n // 4
    seg = samples[start:start + min(n - start, rate // 2)]
    mean = sum(seg) / len(seg)
    seg = [x - mean for x in seg]
    energy = sum(x * x for x in seg)
    if energy <= 0:
        return 0.0
    best_lag, best = 0, 0.0
    lo = max(1, rate // 4000)
    hi = min(len(seg) // 2, rate // 40)
    for lag in range(lo, hi):
        acc = 0
        for i in range(0, len(seg) - lag, 4):
            acc += seg[i] * seg[i + lag]
        if acc > best:
            best, best_lag = acc, lag
    return rate / best_lag if best_lag else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ng", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--mode", choices=("pulse", "tone", "rms"), default="pulse")
    ap.add_argument("--frac", type=float, default=0.25)
    ap.add_argument("--min-width", type=int, default=2)
    args = ap.parse_args()

    rate_a, a = read_wav(args.ng)
    rate_b, b = read_wav(args.ref)
    if rate_a != rate_b:
        raise SystemExit(f"sample-rate mismatch: {rate_a} vs {rate_b}")
    peak_a = max((abs(x) for x in a), default=0)
    peak_b = max((abs(x) for x in b), default=0)
    rms_a = math.sqrt(sum(x * x for x in a) / len(a)) if a else 0.0
    rms_b = math.sqrt(sum(x * x for x in b) / len(b)) if b else 0.0
    print(f"ng  rate={rate_a} n={len(a)} peak={peak_a} rms={rms_a:.1f}")
    print(f"ref rate={rate_b} n={len(b)} peak={peak_b} rms={rms_b:.1f}")

    ok = True
    if args.mode == "pulse":
        sa = pulse_stats(a, peak_a, args.frac, args.min_width)
        sb = pulse_stats(b, peak_b, args.frac, args.min_width)
        if not sa or not sb:
            print("pulse: not enough pulses detected")
            ok = False
        else:
            dp = abs(sa["period"] - sb["period"]) / sb["period"]
            dw = abs(sa["width"] - sb["width"]) / sb["width"]
            print(f"ng  pulses={sa['count']} period={sa['period']:.2f} width={sa['width']:.2f}")
            print(f"ref pulses={sb['count']} period={sb['period']:.2f} width={sb['width']:.2f}")
            print(f"period delta={dp * 100:.2f}%  width delta={dw * 100:.2f}%")
            ok = dp <= 0.01 and dw <= 0.25
    elif args.mode == "tone":
        fa = tone_frequency(a, rate_a)
        fb = tone_frequency(b, rate_b)
        print(f"fundamental ng={fa:.2f} Hz ref={fb:.2f} Hz")
        ok = fb > 0 and abs(fa - fb) / fb <= 0.02
    else:
        ratio = (rms_a / rms_b) if rms_b else 0.0
        print(f"rms ratio ng/ref={ratio:.4f} (MAME route gain 0.25 is expected)")
        ok = ratio > 0.05

    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
