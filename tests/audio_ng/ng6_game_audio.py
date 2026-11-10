import subprocess,os
P=r'tests\audio_ng\bin\Release\audio_render_probe.exe'
LB=r'D:\LaunchBox\Games\Philips Videopac+'
cases=[('vp_31.bin','musician','none',None),
       ('pb_frogger.bin','frogger','standard',None),
       ('pb_popeye.bin','popeye','standard',None),
       ('vp_59_16.bin','helicopter','standard','--g7400'),
       ('vp_60_16.bin','rally','standard','--g7400')]
for fn,tag,plan,g in cases:
    rom=os.path.join(LB,fn)
    if not os.path.exists(rom):
        print('missing',fn); continue
    bios=r'BIOS\G7400.bin' if g else r'BIOS\G7000.bin'
    out=rf'tests\audio_ng\out\ng6_{tag}.wav'
    args=[P,'--bios',bios,'--cart',rom,'--region','pal','--frames','750','--plan',plan,'--wav',out]
    if g: args.append(g)
    r=subprocess.run(args,capture_output=True,text=True,timeout=300)
    print(tag,r.stdout.strip() or r.stderr.strip())
