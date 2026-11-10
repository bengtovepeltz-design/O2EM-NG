param(
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($ProjectDir)
function Get-Sha256([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($Path)))).Replace('-', '') }
    finally { $sha.Dispose() }
}
$manifest = Get-Content -LiteralPath (Join-Path $root 'tools/release-manifest.json') -Raw | ConvertFrom-Json
$source = Join-Path $root 'Gamedata/o2em-ng.db'
$destinationDir = Join-Path $OutDir 'GAMEDATA'
$destination = Join-Path $destinationDir 'o2em-ng.db'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Approved seed missing: $source" }
if ([string]::IsNullOrWhiteSpace($manifest.seedSha256)) { throw 'Manifest seedSha256 is required' }
$sourceHash = Get-Sha256 $source
if ($sourceHash -ne $manifest.seedSha256) { throw "Approved seed hash mismatch: $sourceHash" }
New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null
if (Test-Path -LiteralPath $destination -PathType Leaf) {
    $destinationHash = Get-Sha256 $destination
    if ($destinationHash -ne $sourceHash) {
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
        Copy-Item -LiteralPath $destination -Destination (Join-Path $destinationDir ("o2em-ng.runtime-backup-" + $stamp + '.db')) -Force
    }
}
Copy-Item -LiteralPath $source -Destination $destination -Force
$copiedHash = Get-Sha256 $destination
if ($copiedHash -ne $sourceHash) { throw "Release staging seed verification failed: $copiedHash" }
Write-Host "Release staging seed: $destination ($copiedHash)"
