param(
    [Parameter(Mandatory=$true)]
    [string]$DestinationDirectory
)

$ErrorActionPreference = 'Stop'
$version = 'chromium/7961'
$url = 'https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F7961/pdfium-win-x64.tgz'
$expectedSha256 = 'NOT_PINNED'

New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
$dllPath = Join-Path $DestinationDirectory 'pdfium.dll'
if (Test-Path $dllPath) {
    exit 0
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('o2em-ng-pdfium-' + [Guid]::NewGuid().ToString('N'))
$archivePath = Join-Path $tempRoot 'pdfium-win-x64.tgz'
$extractPath = Join-Path $tempRoot 'extract'

try {
    New-Item -ItemType Directory -Force -Path $extractPath | Out-Null
    Write-Host "O2EM-NG: downloading PDFium $version..."
    Invoke-WebRequest -Uri $url -OutFile $archivePath -UseBasicParsing

    tar.exe -xzf $archivePath -C $extractPath
    $sourceDll = Get-ChildItem -Path $extractPath -Filter 'pdfium.dll' -Recurse | Select-Object -First 1
    if (-not $sourceDll) {
        throw 'pdfium.dll was not found in the downloaded archive.'
    }

    Copy-Item $sourceDll.FullName $dllPath -Force

    $license = Get-ChildItem -Path $extractPath -Filter 'LICENSE' -Recurse | Select-Object -First 1
    if ($license) {
        Copy-Item $license.FullName (Join-Path $DestinationDirectory 'LICENSE.pdfium.txt') -Force
    }

    Write-Host "O2EM-NG: PDFium installed in $DestinationDirectory"
}
finally {
    Remove-Item -Path $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
