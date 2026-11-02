# Package the Windows x64 Release build into dist/windows/KOF98-Windows-x64.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
#   powershell ... -BuildDir C:\path\build-windows -OutDir C:\path\out
#
# The folder contains the player, the ROM decrypt helper, SDL2, the VC++
# runtime DLLs it needs, and the licenses. It contains no ROM data.

param(
    [string]$BuildDir = "",
    [string]$OutDir = "",
    [string]$VCRedistDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $BuildDir) { $BuildDir = Join-Path $root "build-windows" }
if (-not $OutDir) { $OutDir = Join-Path $root "dist\windows\KOF98-Windows-x64" }

$exe = Join-Path $BuildDir "Release\KOF98.exe"
$dec = Join-Path $BuildDir "Release\kof98_decrypt.exe"
if (-not (Test-Path $exe)) { throw "missing $exe; build the Release target first" }
if (-not (Test-Path $dec)) { throw "missing $dec; build the kof98_decrypt target first" }

New-Item -ItemType Directory -Force $OutDir | Out-Null
$lic = Join-Path $OutDir "LICENSES"
New-Item -ItemType Directory -Force $lic | Out-Null

Copy-Item $exe (Join-Path $OutDir "KOF98.exe") -Force
Copy-Item $dec (Join-Path $OutDir "kof98_decrypt.exe") -Force
Copy-Item (Join-Path $root "packaging\windows\README.txt") (Join-Path $OutDir "README.txt") -Force

# SDL2.dll is copied next to the executable by the build's POST_BUILD step.
$sdl = Join-Path $BuildDir "Release\SDL2.dll"
if (Test-Path $sdl) { Copy-Item $sdl (Join-Path $OutDir "SDL2.dll") -Force }
else { Write-Warning "SDL2.dll not found; the player will not open a window" }

# VC++ runtime: vcruntime140 / vcruntime140_1 are the only MSVC DLLs the
# Release executable imports (checked with dumpbin /dependents).
if (-not $VCRedistDir) {
    $candidates = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" `
        -Recurse -Filter vcruntime140.dll -ErrorAction SilentlyContinue |
        Where-Object {
            $_.DirectoryName -match '\\x64\\Microsoft\.VC[0-9]+\.CRT$' -and
            $_.DirectoryName -notmatch 'onecore|debug'
        }
    if ($candidates) { $VCRedistDir = $candidates[-1].DirectoryName }
}
if ($VCRedistDir -and (Test-Path $VCRedistDir)) {
    foreach ($dll in "vcruntime140.dll", "vcruntime140_1.dll") {
        $p = Join-Path $VCRedistDir $dll
        if (Test-Path $p) { Copy-Item $p (Join-Path $OutDir $dll) -Force }
    }
} else {
    Write-Warning "VC++ redistributable directory not found; install the x64 VC++ runtime on the target"
}

Copy-Item (Join-Path $root "LICENSE") (Join-Path $lic "LICENSE.txt") -Force
Copy-Item (Join-Path $root "LICENSE-STATUS.md") (Join-Path $lic "LICENSE-STATUS.txt") -Force
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") (Join-Path $lic "THIRD_PARTY_NOTICES.txt") -Force
Copy-Item (Join-Path $root "packaging\windows\SDL2.txt") (Join-Path $lic "SDL2.txt") -Force
Copy-Item (Join-Path $root "neogeorecomp\LICENSE") (Join-Path $lic "neogeorecomp.txt") -Force
Copy-Item (Join-Path $root "neogeorecomp\NOTICE") (Join-Path $lic "neogeorecomp-NOTICE.txt") -Force
foreach ($f in "Musashi.txt", "ymfm.txt", "z80.txt") {
    Copy-Item (Join-Path $root "neogeorecomp\LICENSES\$f") (Join-Path $lic $f) -Force
}

$files = Get-ChildItem $OutDir -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" }
$files | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
} | Set-Content (Join-Path $OutDir "SHA256SUMS.txt") -Encoding ascii

Write-Host "Wrote $OutDir"
Get-ChildItem $OutDir | Select-Object Name, Length | Format-Table -AutoSize
