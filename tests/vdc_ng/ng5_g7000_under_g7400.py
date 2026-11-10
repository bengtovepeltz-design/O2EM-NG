import csv, os, re, subprocess, sys
sys.path.insert(0,r'tests\vdc_ng')
from compare_refs import read_ppm, read_png, compare
MAME=r'C:\Users\bengt\AppData\Local\Temp\opencode\mame0289'
LB=r'D:\LaunchBox\Games\Philips Videopac+'
P=r'tests\vdc_ng\bin\Release\vdc_render_probe.exe'
rows=list(csv.DictReader(open(r'tests\compat\matrix\compat_matrix_ng4.csv',encoding='utf-8')))
want={'vp_01.bin','Popeye (1983) (Parker Brothers) (EU).bin','Q-bert (1983) (Parker Brothers) (EU).bin','br_frogger.bin','Atlantis (1983) (Imagic) (EU-US).bin','Type & Tell (1984) (Philips) (EU).bin'}
sel={}
for r in rows:
    if r['status']!='OK': continue
    if r['rom'] in want or any(k in r['title'] for k in ('Popeye','Q*bert','Frogger','Atlantis','Type & Tell')):
        sel.setdefault(r['title'],r)
for title,r in list(sel.items())[:8]:
    fn=r['rom']; sw=r['mame_software']
    # locate local by inventory crc
    import json,zlib
    inv=json.load(open(r'tests\compat\matrix\rom_inventory.json'))
    ent=next((x for x in inv if x.get('filename')==fn),None)
    crc=int(ent['crc32'],16) if ent and ent.get('crc32') else 0
    rom=None
    for p in os.listdir(LB):
        fp=os.path.join(LB,p)
        if os.path.isfile(fp) and p.lower().endswith('.bin') and zlib.crc32(open(fp,'rb').read())&0xffffffff==crc:
            rom=fp; break
    if not rom: print('NOLOCAL',title); continue
    tag='ng4_'+re.sub(r'[^A-Za-z0-9_]','_',fn)
    snap=os.path.join(MAME,'snap',tag)
    if not os.path.exists(snap):
        lua=f'''local n=0
emu.register_frame_done(function()
 n=n+1
 if n==500 then local s=nil for k,v in pairs(manager.machine.screens) do if s==nil then s=v end end if s then pcall(function() s:snapshot("{tag}") end) end end
 if n>=500 then manager.machine:exit() end
end)
'''
        open(os.path.join(MAME,'m5.lua'),'w').write(lua)
        subprocess.run([os.path.join(MAME,'mame.exe'),'videopac','-cart',sw,'-autoboot_script','m5.lua','-nothrottle','-skip_gameinfo','-video','none','-seconds_to_run','60'],cwd=MAME,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=240)
    ppm=rf'tests\vdc_ng\out\ng5\g7_{re.sub(r"[^A-Za-z0-9_.-]","_",os.path.basename(rom))}.ppm'
    subprocess.run([P,'--bios',r'BIOS\G7400.bin','--cart',rom,'--region','pal','--g7400','--frames','500','--plan','none','--dump',ppm,'--quiet'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    if os.path.exists(snap) and os.path.exists(ppm):
        c=compare(read_ppm(ppm),read_png(snap),45,30,80,8)
        print(f'{title[:40]:42s} {fn[:22]:24s} {c["iou"]:.4f}')
