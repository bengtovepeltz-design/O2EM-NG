#!/usr/bin/env python3
"""NG-6 MAME audio differential matrix.

Captures deterministic NG and MAME WAVs for a small set of hardware probes and
compares pulse period/width (or tone frequency) objectively. Writes
tests/compat/matrix/compat_matrix_ng6_audio.csv.
"""
import csv
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tests", "audio_ng"))
from audio_reference_compare import read_wav, pulse_stats, tone_frequency  # noqa: E402

MAME = r"C:\Users\bengt\AppData\Local\Temp\opencode\mame0289"
MAME_EXE = os.path.join(MAME, "mame.exe")
LB = r"D:\LaunchBox\Games\Philips Videopac+"
PROBE = os.path.join(REPO, "tests", "audio_ng", "bin", "Release",
                     "audio_render_probe.exe")
OUT = os.path.join(REPO, "tests", "audio_ng", "out")
MATRIX = os.path.join(REPO, "tests", "compat", "matrix",
                      "compat_matrix_ng6_audio.csv")

INPUT_LUA = '''local f=0
emu.register_frame_done(function()
 f=f+1
 if f==250 then emu.keypost("1") end
 if f==310 then emu.keypost("\\n") end
 if f>=800 then manager.machine:exit() end
end)
'''


def run_ng(case, rom, g7400, plan):
    out = os.path.join(OUT, f"ng6_{case}.wav")
    if os.path.exists(out):
        return out
    bios = os.path.join(REPO, "BIOS", "G7400.bin" if g7400 else "G7000.bin")
    args = [PROBE, "--bios", bios, "--cart", rom, "--region", "pal",
            "--frames", "750", "--plan", plan, "--wav", out, "--quiet"]
    if g7400:
        args.append("--g7400")
    subprocess.run(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   timeout=600)
    return out if os.path.exists(out) else None


def run_mame(case, software, g7400, input_script, raw=None):
    out = os.path.join(OUT, f"mame_{case}.wav")
    if os.path.exists(out):
        return out
    machine = "videopacp" if g7400 else "videopac"
    args = [MAME_EXE, machine, "-cart", raw or software,
            "-wavwrite", out, "-samplerate", "48000", "-sound", "none",
            "-nothrottle", "-skip_gameinfo", "-video", "none",
            "-seconds_to_run", "16"]
    if input_script:
        lua = os.path.join(MAME, f"ng6_{case}_input.lua")
        with open(lua, "w") as f:
            f.write(INPUT_LUA)
        args[2:2] = ["-autoboot_script", lua]
    subprocess.run(args, cwd=MAME, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=600)
    return out if os.path.exists(out) else None


def stats(path, mode):
    rate, s = read_wav(path)
    peak = max((abs(x) for x in s), default=0)
    if mode == "pulse":
        st = pulse_stats(s, peak, 0.1, 2)
        if st:
            return st["period"], st["width"], peak
        return 0.0, 0.0, peak
    return tone_frequency(s, rate), 0.0, peak


def main():
    os.makedirs(OUT, exist_ok=True)
    cases = [
        ("synth", "tests/audio_ng/fixtures/audio_synth_2k.bin", "", False,
         "none", "pulse", "synthetic 24-bit shift-register pulse train"),
        ("musician", os.path.join(LB, "vp_31.bin"), "musician", False,
         "none", "pulse", "VP31 Musician structured tones"),
        ("frogger", os.path.join(LB, "pb_frogger.bin"), "frogger", False,
         "standard", "pulse", "Frogger hop/collision sounds"),
        ("popeye", os.path.join(LB, "pb_popeye.bin"), "popeye", False,
         "standard", "pulse", "Popeye movement/collision sounds"),
        ("helicopter", os.path.join(LB, "vp_59_16.bin"), "helicopt", True,
         "standard", "pulse", "VP59+ Helicopter Rescue"),
        ("rally", os.path.join(LB, "vp_60_16.bin"), "rally", True,
         "standard", "pulse", "VP60 Rally"),
        ("qbert", os.path.join(LB, "pb_q-bert.bin"), "qbert", False,
         "standard", "pulse", "Q*bert rapid tone changes"),
        ("pickaxe", os.path.join(LB, "vp_43.bin"), "pickaxe", False,
         "standard", "pulse", "Pick Axe Pete noise/rock sounds"),
    ]
    rows = []
    for case, rom, software, g7400, plan, mode, note in cases:
        rom = os.path.abspath(rom)
        if not os.path.exists(rom):
            rows.append((case, rom, "SKIP", "", "", "", "", "", note + " (ROM missing)"))
            continue
        ng = run_ng(case, rom, g7400, plan)
        if case == "synth":
            mame = run_mame(case, software, g7400, False, raw=rom)
        else:
            mame = run_mame(case, software, g7400, plan != "none")
        if not ng or not mame:
            rows.append((case, rom, "ERROR", "", "", "", "", "", note))
            continue
        # For the synthetic fixture, MAME was captured earlier under this name.
        if case == "synth" and not os.path.exists(mame):
            alt = os.path.join(MAME, "synth_mame.wav")
            if os.path.exists(alt):
                mame = alt
        pa, wa, peaka = stats(ng, mode)
        pb, wb, peakb = stats(mame, mode)
        if mode == "pulse":
            dp = abs(pa - pb) / pb if pb else 0.0
            dw = abs(wa - wb) / wb if wb else 0.0
            result = "PASS" if pb and dp <= 0.01 and dw <= 0.25 else "FAIL"
            rows.append((case, rom, result, f"{pa:.2f}", f"{pb:.2f}",
                         f"{dp*100:.2f}%", f"{dw*100:.2f}%",
                         f"{peaka}/{peakb}", note))
        else:
            dp = abs(pa - pb) / pb if pb else 0.0
            result = "PASS" if pb and dp <= 0.02 else "FAIL"
            rows.append((case, rom, result, f"{pa:.2f}", f"{pb:.2f}",
                         f"{dp*100:.2f}%", "n/a", f"{peaka}/{peakb}", note))
        print(rows[-1], flush=True)

    with open(MATRIX, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["case", "rom", "result", "ng_period_or_freq",
                    "mame_period_or_freq", "delta", "width_delta",
                    "peak_ng_mame", "note"])
        w.writerows(rows)
    print(f"rows={len(rows)} pass={sum(1 for r in rows if r[2]=='PASS')}")


if __name__ == "__main__":
    main()
