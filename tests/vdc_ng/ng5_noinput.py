import csv, json, os, re, subprocess, sys, zlib
sys.path.insert(0, r'tests\vdc_ng')
from compare_refs import read_ppm, read_png, compare
MAME = r'C:\Users\bengt\AppData\Local\Temp\opencode\mame0289'
MAME_EXE = os.path.join(MAME,'mame.exe')
LB = r'D:\LaunchBox\Games\Philips Videopac+'
PROBE = r'tests\vdc_ng\bin\Release\vdc_render_probe.exe'
BIOS = r'BIOS\G7400.bin'
OUT = r'tests\vdc_ng\out\ng5'
inv=json.load(open(r'tests\compat\matrix\rom_inventory.json'))
by_crc={}
for x in inv:
    if x.get('crc32'): by_crc[int(x['crc32'],16)]=x
def crc32_file(p):
    return zlib.crc32(open(p,'rb').read())&0xFFFFFFFF
files=[]
for p in os.listdir(LB):
    fp=os.path.join(LB,p)
    if os.path.isfile(fp) and p.lower().endswith('.bin'):
        files.append((p,crc32_file(fp),fp))
rows=list(csv.DictReader(open(r'tests\compat\matrix\compat_matrix_ng5.csv',encoding='utf-8')))
res=[]
for r in rows:
    if r['status']!='OK': continue
    sw=r['mame_software']; fn=r['rom']
    ent=by_crc.get(int(by_crc and 0 or 0))
    # find entry by filename match in inventory
    cand=[x for x in inv if x.get('filename')==fn]
    crc=int(cand[0]['crc32'],16) if cand and cand[0].get('crc32') else 0
    rom=next((fp for (p,c,fp) in files if c==crc),None)
    if rom is None:
        print('NOLOCAL',fn); continue
    tag='ng5n_'+re.sub(r'[^A-Za-z0-9_]','_',fn)
    snap=os.path.join(MAME,'snap',tag)
    if not os.path.exists(snap):
        lua=f'''local f=0
emu.register_frame_done(function()
 f=f+1
 if f==500 then
  local s=nil
  for k,v in pairs(manager.machine.screens) do if s==nil then s=v end end
  if s then pcall(function() s:snapshot("{tag}") end) end
 end
 if f>=500 then manager.machine:exit() end
end)
'''
        lp=os.path.join(MAME,'ng5n.lua'); open(lp,'w').write(lua)
        subprocess.run([MAME_EXE,'videopacp','-cart',sw,'-autoboot_script',lp,'-nothrottle','-skip_gameinfo','-video','none','-seconds_to_run','60'],cwd=MAME,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=240)
    ppm=os.path.join(OUT,'nn_'+re.sub(r'[^A-Za-z0-9_.-]','_',os.path.basename(rom))+'.ppm')
    subprocess.run([PROBE,'--bios',BIOS,'--cart',rom,'--region','pal','--g7400','--frames','500','--plan','none','--dump',ppm,'--quiet'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=240)
    if not os.path.exists(snap) or not os.path.exists(ppm):
        res.append((fn,sw,'MISS','','')); continue
    c=compare(read_ppm(ppm),read_png(snap),45,30,80,8)
    res.append((fn,sw,f"{c['iou']:.4f}",f"{c['row_corr']:.4f}",f"{c['col_corr']:.4f}"))
    print(res[-1],flush=True)
with open(r'tests\vdc_ng\out\ng5_noinput.csv','w',newline='') as f:
    w=csv.writer(f); w.writerow(['rom','software','iou','row','col']); w.writerows(res)
