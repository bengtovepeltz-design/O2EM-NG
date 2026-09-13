param(
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$TargetPath,
    [Parameter(Mandatory=$true)][string]$RedistDir
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($ProjectDir)
$manifest = Get-Content -LiteralPath (Join-Path $root 'tools/release-manifest.json') -Raw | ConvertFrom-Json
$dist = Join-Path $root 'dist'
if ($manifest.version -notmatch '^\d+\.\d+\.\d+-beta$') { throw 'Invalid release version' }
$name = 'O2EM-NG-v' + $manifest.version
$package = Join-Path $dist $name
$stage = Join-Path $dist ($name + '.building-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
function Destination([string]$relative) {
    $path = [IO.Path]::GetFullPath((Join-Path $stage $relative))
    if (-not $path.StartsWith($stage + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Destination outside package: $relative" }
    return $path
}
function Include-File([string]$source, [string]$relative) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required release file missing: $source" }
    $dest = Destination $relative
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $dest
}
Include-File $TargetPath 'O2EM-NG.exe'
foreach ($file in $manifest.files) {
    $source = $file.source
    if (-not [IO.Path]::IsPathRooted($source)) { $source = Join-Path $root $source }
    Include-File $source $file.destination
}
if (-not (Test-Path -LiteralPath (Join-Path $RedistDir 'x64'))) {
    $versions = @(Get-ChildItem -LiteralPath $RedistDir -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending)
    if (!$versions.Count) { throw "No C redistributable found in $RedistDir" }
    $RedistDir = $versions[0].FullName
}
$crt = @(Get-ChildItem -LiteralPath (Join-Path $RedistDir 'x64') -Directory | Where-Object { $_.Name -match '^Microsoft\.VC\d+\.CRT$' })
if ($crt.Count -ne 1) { throw "Expected one x64 CRT directory in $RedistDir" }
foreach ($dll in $manifest.runtimeDlls) { Include-File (Join-Path $crt[0].FullName $dll) $dll }
foreach ($folder in $manifest.emptyFolders) {
    $dest = Destination ($folder + '/README.txt')
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Set-Content -LiteralPath $dest -Encoding UTF8 -Value 'User content belongs here. No BIOS, firmware, game ROMs or personal media are included.'
}
Set-Content -LiteralPath (Join-Path $stage 'VERSION.txt') -Encoding UTF8 -Value ($name + "`r`nBeta 4 - build package; see Docs for testing status.")
Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = [BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($_.FullName))).Replace("-", "") } finally { $sha.Dispose() }
    $hash + '  ' + $_.FullName.Substring($stage.Length + 1)
} | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Encoding UTF8
# Keep the previous package intact. Only move the exact, validated output path.
$resolvedPackage = [IO.Path]::GetFullPath($package)
if (-not $resolvedPackage.StartsWith([IO.Path]::GetFullPath($dist) + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package path' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
if (Test-Path -LiteralPath $package) { Move-Item -LiteralPath $package -Destination ($package + '.previous-' + $stamp) }
Move-Item -LiteralPath $stage -Destination $package
$zip = $package + '.zip'
if (Test-Path -LiteralPath $zip) { Move-Item -LiteralPath $zip -Destination ($zip + '.previous-' + $stamp) }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($package, $zip, [IO.Compression.CompressionLevel]::Optimal, $true)
Write-Host "Release package: $package"
Write-Host "Release ZIP: $zip"

# Inno Setup reads the same version/package chosen by the manifest.
$innoInfo = '#define MyAppVersion "' + $manifest.version + '"' + "`r`n" + '#define PackageDirectory "..\dist\' + $name + '"'
Set-Content -LiteralPath (Join-Path $dist 'release-info.iss') -Value $innoInfo -Encoding UTF8
