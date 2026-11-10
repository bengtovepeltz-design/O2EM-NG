# Checkpoint A/B differential matrix runner.
# Runs a local ROM in MAME 0.289 and O2EM-NG Debug with equivalent inputs and
# emits compact screenshots + structural metrics.
# Checkpoint B adds -Variant legacy|unified (O2EM_UNIFIED_VIDEO_TIMEBASE_TEST)
# and -SkipMame to reuse the same MAME reference captures for both runs.
param(
    [string]$Titles = "",
    [int]$Limit = 0,
    [switch]$All,
    [string]$Frames = "500,800",
    [string]$MameRoot = "C:\Users\bengt\AppData\Local\Temp\opencode\mame0289",
    [string]$RomDir = "D:\Project\O2EM-NG\x64\Debug\ROMS",
    [string]$BiosDir = "D:\Project\O2EM-NG\x64\Debug\BIOS",
    [string]$O2Exe = "D:\Project\O2EM-NG\x64\Debug\O2EM-NG.exe",
    # Checkpoint B A/B: "legacy" runs the production path, "unified" enables
    # O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1. Both use the same ROM/BIOS/region/
    # input/frame checkpoints and the same MAME reference captures.
    [ValidateSet("legacy","unified")]
    [string]$Variant = "legacy",
    [string]$OutCsv = "",
    # Reuse the MAME screenshots already captured by the Checkpoint A run.
    # This keeps the reference byte-identical between legacy and unified.
    [switch]$SkipMame
)

$ErrorActionPreference = "Stop"
$repo = "D:\Project\O2EM-NG"
$compat = Join-Path $repo "tests\compat"
$matrixDir = Join-Path $compat "matrix"
$capM = Join-Path $compat "captures\mame"
$capO = Join-Path $compat ("captures\o2em_" + $Variant)
$diffDir = Join-Path $compat ("diff\" + $Variant)
$traceDir = Join-Path $compat ("traces\" + $Variant)
if (-not $OutCsv) { $OutCsv = Join-Path $matrixDir ("compat_matrix_" + $Variant + ".csv") }
foreach ($d in @($matrixDir,$capM,$capO,$diffDir,$traceDir)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class CompatImg
{
    static Bitmap Crop(string path, int x, int y, int w, int h)
    {
        using (var src = new Bitmap(path))
        {
            var dst = new Bitmap(w, h, PixelFormat.Format24bppRgb);
            using (var g = Graphics.FromImage(dst))
            {
                g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
                g.DrawImage(src, new Rectangle(0, 0, w, h), new Rectangle(x, y, w, h), GraphicsUnit.Pixel);
            }
            return dst;
        }
    }

    static int Dominant(Bitmap b)
    {
        var counts = new System.Collections.Generic.Dictionary<int,int>();
        int step = Math.Max(1, Math.Min(b.Width, b.Height) / 80);
        for (int y = 0; y < b.Height; y += step)
        for (int x = 0; x < b.Width; x += step)
        {
            Color c = b.GetPixel(x, y);
            int q = ((c.R >> 4) << 8) | ((c.G >> 4) << 4) | (c.B >> 4);
            int n; counts.TryGetValue(q, out n); counts[q] = n + 1;
        }
        int best = 0, bestN = -1;
        foreach (var kv in counts) if (kv.Value > bestN) { bestN = kv.Value; best = kv.Key; }
        int r = ((best >> 8) & 0xF) * 17, g = ((best >> 4) & 0xF) * 17, bb = (best & 0xF) * 17;
        return (r << 16) | (g << 8) | bb;
    }

    static bool Lit(Color c, int bg, int thresh)
    {
        int r = (bg >> 16) & 0xFF, g = (bg >> 8) & 0xFF, b = bg & 0xFF;
        return Math.Abs(c.R - r) + Math.Abs(c.G - g) + Math.Abs(c.B - b) >= thresh;
    }

    public static string Compare(string apath, int ax, int ay, string bpath, int bx, int by, int w, int h, int gw, int gh)
    {
        using (var a = Crop(apath, ax, ay, w, h))
        using (var b = Crop(bpath, bx, by, w, h))
        {
            int bga = Dominant(a), bgb = Dominant(b);
            int cw = (w + gw - 1) / gw, ch = (h + gh - 1) / gh;
            bool[] ma = new bool[gw * gh], mb = new bool[gw * gh];
            int litA = 0, litB = 0;
            int minAx = w, minAy = h, maxAx = -1, maxAy = -1;
            int minBx = w, minBy = h, maxBx = -1, maxBy = -1;
            for (int cy = 0; cy < gh; cy++)
            for (int cx = 0; cx < gw; cx++)
            {
                int ca = 0, cb = 0, total = 0;
                for (int yy = cy * ch; yy < Math.Min(h, (cy + 1) * ch); yy++)
                for (int xx = cx * cw; xx < Math.Min(w, (cx + 1) * cw); xx++)
                {
                    total++;
                    if (Lit(a.GetPixel(xx, yy), bga, 60)) { ca++; if (xx < minAx) minAx = xx; if (xx > maxAx) maxAx = xx; if (yy < minAy) minAy = yy; if (yy > maxAy) maxAy = yy; }
                    if (Lit(b.GetPixel(xx, yy), bgb, 60)) { cb++; if (xx < minBx) minBx = xx; if (xx > maxBx) maxBx = xx; if (yy < minBy) minBy = yy; if (yy > maxBy) maxBy = yy; }
                }
                ma[cy * gw + cx] = total > 0 && ca * 100 >= total * 8;
                mb[cy * gw + cx] = total > 0 && cb * 100 >= total * 8;
                if (ma[cy * gw + cx]) litA++;
                if (mb[cy * gw + cx]) litB++;
            }
            int inter = 0, uni = 0;
            for (int i = 0; i < ma.Length; i++) { if (ma[i] && mb[i]) inter++; if (ma[i] || mb[i]) uni++; }
            double iou = uni == 0 ? 0.0 : (double)inter / uni;

            double[] ra = new double[gh], rb = new double[gh], ca2 = new double[gw], cb2 = new double[gw];
            for (int cy = 0; cy < gh; cy++) { for (int cx = 0; cx < gw; cx++) { if (ma[cy*gw+cx]) ra[cy] += 1; if (mb[cy*gw+cx]) rb[cy] += 1; } }
            for (int cx = 0; cx < gw; cx++) { for (int cy = 0; cy < gh; cy++) { if (ma[cy*gw+cx]) ca2[cx] += 1; if (mb[cy*gw+cx]) cb2[cx] += 1; } }
            double corrR = Corr(ra, rb), corrC = Corr(ca2, cb2);
            double fracA = (double)litA / (gw * gh), fracB = (double)litB / (gw * gh);
            return string.Format(System.Globalization.CultureInfo.InvariantCulture,
                "{0:F4},{1:F4},{2:F4},{3:F4},{4:F4},{5},{6},{7},{8},{9},{10},{11},{12}",
                iou, corrR, corrC, fracA, fracB,
                minAx, minAy, maxAx, maxAy, minBx, minBy, maxBx, maxBy);
        }
    }

    static double Corr(double[] a, double[] b)
    {
        int n = Math.Min(a.Length, b.Length);
        double ma = 0, mb = 0;
        for (int i = 0; i < n; i++) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        double num = 0, da = 0, db = 0;
        for (int i = 0; i < n; i++) { double x = a[i]-ma, y = b[i]-mb; num += x*y; da += x*x; db += y*y; }
        if (da <= 1e-9 || db <= 1e-9) return 0.0;
        return num / Math.Sqrt(da * db);
    }
}
'@ -ReferencedAssemblies System.Drawing

function Safe([string]$s) {
    $r = [regex]::Replace($s, "[^A-Za-z0-9_.+-]", "_")
    if ($r.Length -gt 60) { $r = $r.Substring(0,60) }
    return $r
}

function Convert-BmpToPng([string]$bmp, [string]$png) {
    $img = [System.Drawing.Bitmap]::FromFile($bmp)
    $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
}

function Get-KeyPlan([string]$filename) {
    $f = $filename.ToLower()
    if ($f -like "*vp_48*") {
        return @{ mame=@(@(250,"6"),@(310,"N")); o2em="69:33:6,109:14:6" }
    }
    if ($f -like "*c7010*") {
        return @{ mame=@(@(250,"1"),@(310,"Y"),@(370,"2")); o2em="120:28:4,180:25:4,240:29:4" }
    }
    if ($f -like "*vp_59*") {
        return @{ mame=@(@(250,"1"),@(310,"ENTER")); o2em="120:28:4,150:38:4" }
    }
    return @{ mame=@(@(250,"1"),@(310,"ENTER")); o2em="120:28:4,150:38:4" }
}

$inventory = Get-Content (Join-Path $matrixDir "rom_inventory.json") -Raw | ConvertFrom-Json
$list = @($inventory)
if (-not $All) {
    if ($Titles) {
        $wanted = $Titles.Split(",") | ForEach-Object { $_.Trim().ToLower() }
        $list = $list | Where-Object { $wanted -contains $_.vp.ToLower() -or $wanted -contains $_.filename.ToLower() }
    } elseif ($Limit -gt 0) {
        $list = $list | Where-Object { $_.mame_software -and $_.mame_system -eq "videopac" } | Select-Object -First $Limit
    }
}

$rows = @()
$frameNums = $Frames.Split(",") | ForEach-Object { [int]$_.Trim() }
$maxFrame = ($frameNums | Measure-Object -Maximum).Maximum
$mameRoms = Join-Path $MameRoot "roms\videopac"
New-Item -ItemType Directory -Force -Path $mameRoms | Out-Null

foreach ($e in $list) {
    $safe = Safe $e.filename
    $t = Join-Path $traceDir $safe
    New-Item -ItemType Directory -Force -Path $t | Out-Null
    $plan = Get-KeyPlan $e.filename
    $note = ""
    $mameOk = $false; $o2Ok = $false
    $mameShot = ""; $o2Shot = ""
    $stats = ""

    # --- O2EM-NG run -----------------------------------------------------
    $o2bios = if ($e.region -eq "NTSC") { "o2rom.bin" } elseif ($e.system -eq "G7400/VPP" -or $e.system -eq "Jopac") { "G7400.bin" } else { "G7000.bin" }
    $o2dump = Join-Path $t "o2em"
    New-Item -ItemType Directory -Force -Path $o2dump | Out-Null
    $o2keys = $plan.o2em
    $env:O2EM_VIDEO_PROBE="1"; $env:O2EM_VIDEO_ROM=(Join-Path $RomDir $e.filename); $env:O2EM_VIDEO_BIOS=(Join-Path $BiosDir $o2bios)
    $env:O2EM_VIDEO_FRAMES=($maxFrame+5).ToString(); $env:O2EM_VIDEO_REGION=$e.region.ToLower(); $env:O2EM_VIDEO_KEY_SEQ=$o2keys
    $env:O2EM_VIDEO_DUMP_DIR=$o2dump; $env:O2EM_VIDEO_DUMP_FRAME=($frameNums -join ","); $env:O2EM_VIDEO_DUMP_COUNT="0"
    if ($Variant -eq "unified") { $env:O2EM_UNIFIED_VIDEO_TIMEBASE_TEST="1" }
    else { Remove-Item Env:\O2EM_UNIFIED_VIDEO_TIMEBASE_TEST -ErrorAction SilentlyContinue }
    try {
        $p = Start-Process -FilePath $O2Exe -WorkingDirectory (Split-Path $O2Exe) -PassThru -RedirectStandardOutput (Join-Path $t "o2_stdout.txt") -RedirectStandardError (Join-Path $t "o2_stderr.txt")
        $p | Wait-Process -Timeout 120 -ErrorAction SilentlyContinue
        if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
        $o2Ok = ($p.HasExited -and $p.ExitCode -eq 0)
    } catch { $note += "O2EM launch failed; " }
    Remove-Item Env:\O2EM_VIDEO_PROBE,Env:\O2EM_VIDEO_ROM,Env:\O2EM_VIDEO_BIOS,Env:\O2EM_VIDEO_FRAMES,Env:\O2EM_VIDEO_REGION,Env:\O2EM_VIDEO_KEY_SEQ,Env:\O2EM_VIDEO_DUMP_DIR,Env:\O2EM_VIDEO_DUMP_FRAME,Env:\O2EM_VIDEO_DUMP_COUNT,Env:\O2EM_UNIFIED_VIDEO_TIMEBASE_TEST -ErrorAction SilentlyContinue
    $o2files = @{}
    foreach ($n in $frameNums) {
        $bmp = Join-Path $o2dump ("frame_{0:D4}.bmp" -f $n)
        if (Test-Path $bmp) {
            $png = Join-Path $capO ("{0}_{1}.png" -f $safe,$n)
            Convert-BmpToPng $bmp $png
            $o2files[$n] = $png
        }
    }
    if ($o2files.Count -gt 0) { $o2Shot = $o2files[$frameNums[$frameNums.Count-1]] }

    # --- MAME run --------------------------------------------------------
    if ($SkipMame) {
        foreach ($n in $frameNums) {
            $png = Join-Path $capM ("{0}_{1}.png" -f $safe,$n)
            if (Test-Path $png) {
                if ($n -eq $frameNums[$frameNums.Count-1]) { $mameShot = $png }
            }
        }
        if (-not $mameShot) { $note += "MAME reference capture unavailable; " }
        $mameOk = [bool]$mameShot
    }
    elseif ($e.mame_software -and $e.mame_system -eq "videopac" -and $e.mame_rom) {
        $zip = Join-Path $mameRoms ($e.mame_software + ".zip")
        if (-not (Test-Path $zip)) {
            $tmp = Join-Path $t "mame_zip"
            New-Item -ItemType Directory -Force -Path $tmp | Out-Null
            Copy-Item (Join-Path $RomDir $e.filename) (Join-Path $tmp $e.mame_rom) -Force
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [System.IO.Compression.ZipFile]::CreateFromDirectory($tmp, $zip)
            Remove-Item $tmp -Recurse -Force
        }
        $lua = Join-Path $t "mame.lua"
        $shotLines = ($frameNums | ForEach-Object { "  [{0}]=""{1}_{0}"";" -f $_,$safe }) -join "`r`n"
        if ($frameNums.Count -eq 1) { $shotLines = "  [{0}]=""{1}_{0}"";" -f $frameNums[0],$safe }
        $keyLines = ($plan.mame | ForEach-Object { "  if frame=={0} then emu.keypost(""{1}"") end" -f $_[0],$_[1] }) -join "`r`n"
        $luaText = @"
local frame = 0
local shots = {
$shotLines
}
emu.register_frame_done(function()
  frame = frame + 1
$keyLines
  local n = shots[frame]
  if n then
    local s = nil
    for k,v in pairs(manager.machine.screens) do if s == nil then s = v end end
    if s == nil then s = manager.machine.devices[":screen"] end
    if s then pcall(function() s:snapshot(n) end) end
  end
  if frame >= $maxFrame then manager.machine:exit() end
end)
"@
        Set-Content -Path $lua -Value $luaText -Encoding UTF8
        $snapDir = Join-Path $MameRoot "snap"
        New-Item -ItemType Directory -Force -Path $snapDir | Out-Null
        $mameArgs = @("videopac","-cart",$e.mame_software,"-autoboot_script",$lua,"-nothrottle","-skip_gameinfo","-video","none","-seconds_to_run","60")
        try {
            $mp = Start-Process -FilePath (Join-Path $MameRoot "mame.exe") -ArgumentList $mameArgs -WorkingDirectory $MameRoot -PassThru -RedirectStandardOutput (Join-Path $t "mame_stdout.txt") -RedirectStandardError (Join-Path $t "mame_stderr.txt")
            $mp | Wait-Process -Timeout 120 -ErrorAction SilentlyContinue
            if (-not $mp.HasExited) { Stop-Process -Id $mp.Id -Force }
            $mameOk = ($mp.HasExited -and $mp.ExitCode -eq 0)
        } catch { $note += "MAME launch failed; " }
        foreach ($n in $frameNums) {
            $snap = Join-Path $snapDir ("{0}_{1}" -f $safe,$n)
            if (Test-Path $snap) {
                $png = Join-Path $capM ("{0}_{1}.png" -f $safe,$n)
                Copy-Item $snap $png -Force
                if ($n -eq $frameNums[$frameNums.Count-1]) { $mameShot = $png }
            }
        }
    } else {
        $note += "MAME equivalent unavailable (no software match/BIOS); "
    }

    # --- compare ---------------------------------------------------------
    $visual = "UNKNOWN"; $iou = 0; $corrR = 0; $corrC = 0; $fracA = 0; $fracB = 0
    if ($mameShot -and $o2Shot) {
        # O2EM visible area: x=7,y=2; MAME visible area: x=20,y=0; 320x240.
        $stats = [CompatImg]::Compare($o2Shot,7,2,$mameShot,20,0,320,240,40,30)
        $parts = $stats.Split(",")
        $iou=[double]$parts[0]; $corrR=[double]$parts[1]; $corrC=[double]$parts[2]; $fracA=[double]$parts[3]; $fracB=[double]$parts[4]
        if ($fracA -lt 0.005) { $visual = "ORANGE" }
        elseif ($fracB -lt 0.005) { $visual = "UNKNOWN" }
        elseif ($iou -ge 0.55 -and (($corrR+$corrC)/2) -ge 0.72) { $visual = "GREEN" }
        elseif ($iou -ge 0.30 -or (($corrR+$corrC)/2) -ge 0.55) { $visual = "YELLOW" }
        else { $visual = "ORANGE" }
        # side-by-side diff
        $a = [System.Drawing.Bitmap]::FromFile($o2Shot); $b = [System.Drawing.Bitmap]::FromFile($mameShot)
        $n = New-Object System.Drawing.Bitmap(650,250); $g=[System.Drawing.Graphics]::FromImage($n); $g.Clear([System.Drawing.Color]::Magenta)
        $g.DrawImage($a,0,0); $g.DrawImage($b,330,0); $g.Dispose()
        $n.Save((Join-Path $diffDir ("{0}_side.png" -f $safe)), [System.Drawing.Imaging.ImageFormat]::Png); $n.Dispose(); $a.Dispose(); $b.Dispose()
    } elseif ($o2Shot -and -not $mameShot) { $visual = "UNKNOWN" }

    $mameBoot = if ($mameOk) { "YES" } else { "NO" }
    $o2Boot = if ($o2Ok) { "YES" } else { "NO" }
    $mameGame = if ($mameShot) { "CAPTURED" } else { "UNKNOWN" }
    $o2Game = if ($o2Shot) { "CAPTURED" } else { "UNKNOWN" }
    $align = ("iou={0:F3}; rowcorr={1:F3}; colcorr={2:F3}; litO2={3:F3}; litM={4:F3}" -f $iou,$corrR,$corrC,$fracA,$fracB)
    $rows += [pscustomobject]@{
        vp = $e.vp; title = $e.title; filename = $e.filename; crc = $e.crc32
        system = $e.system; region = $e.region; mame_system = $e.mame_system; mame_software = $e.mame_software
        mame_boot = $mameBoot
        o2em_boot = $o2Boot
        mame_gameplay = $mameGame
        o2em_gameplay = $o2Game
        visual_geometry = $visual
        state_alignment = $align
        known_difference = $note
        suspected_subsystem = ""
        severity = $visual
        mame_screenshot = $mameShot; o2em_screenshot = $o2Shot
        notes = ""
    }
}

$out = $OutCsv
$rows | Export-Csv -Path $out -NoTypeInformation -Encoding UTF8
Write-Host ("Matrix rows: {0}" -f $rows.Count)
Write-Host ("Variant: {0}" -f $Variant)
Write-Host ("CSV: {0}" -f $out)
$rows | Group-Object severity | Select-Object Name,Count | Format-Table -AutoSize | Out-String | Write-Host
