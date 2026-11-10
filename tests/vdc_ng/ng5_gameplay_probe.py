import os
import subprocess
import sys

sys.path.insert(0, r"tests\vdc_ng")
from compare_refs import read_ppm, read_png, compare

MAME = r"C:\Users\bengt\AppData\Local\Temp\opencode\mame0289"
P = r"tests\vdc_ng\bin\Release\vdc_render_probe.exe"
LB = r"D:\LaunchBox\Games\Philips Videopac+"
cases = [("vp_59_16.bin", "helicopt", "h59g"),
         ("vp_60_16.bin", "rally", "r60g"),
         ("jo_moto-crash_pl.bin", "motocras", "motog")]

LUA = '''local n=0
emu.register_frame_done(function()
 n=n+1
 if n==250 then emu.keypost("1") end
 if n==310 then emu.keypost("\\n") end
 if n==FRAME then
  local s=nil
  for k,v in pairs(manager.machine.screens) do if s==nil then s=v end end
  if s then pcall(function() s:snapshot("TAG") end) end
 end
 if n>=FRAME then manager.machine:exit() end
end)
'''

for fn, sw, tag in cases:
    rom = os.path.join(LB, fn)
    if not os.path.exists(rom):
        print("missing", fn)
        continue
    for frame in (1000, 1500):
        st = f"{tag}_{frame}"
        snap = os.path.join(MAME, "snap", st)
        if not os.path.exists(snap):
            lua = LUA.replace("FRAME", str(frame)).replace("TAG", st)
            open(os.path.join(MAME, f"{st}.lua"), "w").write(lua)
            subprocess.run(
                [os.path.join(MAME, "mame.exe"), "videopacp", "-cart", sw,
                 "-autoboot_script", f"{st}.lua", "-nothrottle",
                 "-skip_gameinfo", "-video", "none", "-seconds_to_run", "90"],
                cwd=MAME, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, timeout=300)
        ppm = rf"tests\vdc_ng\out\ng5\play_{tag}_{frame}.ppm"
        subprocess.run(
            [P, "--bios", r"BIOS\G7400.bin", "--cart", rom, "--region", "pal",
             "--g7400", "--frames", str(frame), "--plan", "standard",
             "--dump", ppm, "--quiet"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
        if os.path.exists(snap) and os.path.exists(ppm):
            c = compare(read_ppm(ppm), read_png(snap), 45, 30, 80, 8)
            print(fn, frame, f"{c['iou']:.4f}", flush=True)
        else:
            print(fn, frame, "MISSING")
