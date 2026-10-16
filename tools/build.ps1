# REDcoLAB build script
#
# Compiles src/REDcoLAB.cpp against the CELSYS Triglav Plug-In SDK headers and
# emits both accepted plug-in file names into dist/:
#   dist/REDcoLAB.cpm  (community convention)
#   dist/REDcoLAB.dll  (what Celsys' own filters use)
# Both come from the same source; which one CSP actually loads is determined by
# real-machine testing, not by guessing.
#
# Requirements:
#   - MinGW-w64 g++ on PATH or installed via WinGet (WinLibs)
#   - sdk_official/*.h present (the CELSYS SDK headers, NOT redistributed here)
#
# ASCII ONLY: PowerShell 5.1 reads .ps1 using the system ANSI code page.

[CmdletBinding()]
param(
    [switch]$KeepStage,
    # Build the diagnostic probe instead of the product. The probe always logs,
    # never modifies pixels, and exists to produce evidence from a real CSP run.
    [switch]$Probe,
    # Build the module-kind probe: decides at runtime which module kind to
    # declare, via the REDCOLAB_KIND environment variable. Used to find out
    # which kind CLIP STUDIO PAINT accepts.
    [switch]$KindProbe
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$srcDir      = Join-Path $projectRoot 'src'
$sdkDir      = Join-Path $projectRoot 'sdk_official'
$distDir     = Join-Path $projectRoot 'dist'
$versionFile = Join-Path $projectRoot 'ai-toolkit\VERSION'

function Fail([string]$message) {
    Write-Host "BUILD FAILED: $message" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- preconditions
Write-Host "=== REDcoLAB build ==="

if (-not (Test-Path -LiteralPath $versionFile)) { Fail "missing $versionFile" }
$version = (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Fail "VERSION is not MAJOR.MINOR.PATCH: '$version'" }
Write-Host "version       : $version"

$source = Join-Path $srcDir 'REDcoLAB.cpp'
if (-not (Test-Path -LiteralPath $source)) { Fail "missing $source" }

if (-not (Test-Path -LiteralPath $sdkDir)) {
    Fail @"
missing sdk_official/ - the CELSYS Triglav Plug-In SDK headers are required.

They are deliberately NOT shipped with this repository (CELSYS holds the
copyright and the SDK licence forbids redistributing them). Obtain them from
the official page:

  https://www.clipstudio.net/ja/dl/cspsdk_term/

Accept the licence, download the archive, and copy the TriglavPlugIn*.h files
into: $sdkDir

Do NOT fetch these headers from a third-party mirror or another GitHub repo.
"@
}

$requiredHeaders = @(
    'TriglavPlugInDefine.h', 'TriglavPlugInExtern.h', 'TriglavPlugInMain.h',
    'TriglavPlugInRecord.h', 'TriglavPlugInRecordFunction.h', 'TriglavPlugInSDK.h',
    'TriglavPlugInServer.h', 'TriglavPlugInService.h', 'TriglavPlugInType.h'
)
$missing = @()
foreach ($h in $requiredHeaders) {
    if (-not (Test-Path -LiteralPath (Join-Path $sdkDir $h))) { $missing += $h }
}
if ($missing.Count -gt 0) {
    Fail ("sdk_official/ is incomplete, missing: " + ($missing -join ', '))
}
Write-Host "sdk headers   : $($requiredHeaders.Count) present in sdk_official/"

# ---------------------------------------------------------------- compiler
$gpp = $null
$fromPath = Get-Command g++ -ErrorAction SilentlyContinue
if ($fromPath) {
    $gpp = $fromPath.Source
}
else {
    $pkgRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    if (Test-Path -LiteralPath $pkgRoot) {
        $winlibs = Get-ChildItem -LiteralPath $pkgRoot -Directory -Filter 'BrechtSanders.WinLibs*' -ErrorAction SilentlyContinue |
                   Select-Object -First 1
        if ($winlibs) {
            $candidate = Join-Path $winlibs.FullName 'mingw64\bin\g++.exe'
            if (Test-Path -LiteralPath $candidate) { $gpp = $candidate }
        }
    }
}
if (-not $gpp) {
    Fail @"
g++ not found.

Install MinGW-w64 (WinLibs) either from PATH or with:

  winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT --scope user

Note: the official CELSYS sample projects use MSVC + MFC. This project uses
MinGW-w64 instead, which has been verified to produce a loadable-in-structure
x64 DLL exporting TriglavPluginCall. Real CSP loading is a separate test.
"@
}

Write-Host "compiler      : $gpp"
$compilerVersion = (& $gpp --version 2>&1 | Select-Object -First 1)
Write-Host "compiler ver  : $compilerVersion"

# ---------------------------------------------------------------- compile
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$commonArgs = @(
    '-O2', '-shared', '-s', '-std=c++17',
    '-D_WINDOWS',
    '-Wall', '-Wextra', '-Werror',
    # The CELSYS headers wrap pack(push,1)/pack(pop) in pragmas that GCC does not
    # recognise (they are MSVC-shaped). Treating that as an error would mean
    # editing third-party headers, which we must not do. The warning is therefore
    # suppressed globally; every other warning stays fatal.
    '-Wno-unknown-pragmas',
    '-isystem', $sdkDir,
    '-static-libgcc', '-static-libstdc++', '-static',
    '-lkernel32'
)

# Each build target: output file name -> source file.
if ($KindProbe) {
    $kindSource = Join-Path $srcDir 'probe\REDcoLAB_kindprobe.cpp'
    if (-not (Test-Path -LiteralPath $kindSource)) { Fail "missing $kindSource" }
    $targets = @{ 'REDcoLAB_kindprobe.dll' = $kindSource }
    Write-Host "build target  : MODULE-KIND PROBE (diagnostic, never modifies pixels)"
}
elseif ($Probe) {
    $probeSource = Join-Path $srcDir 'probe\REDcoLAB_probe.cpp'
    if (-not (Test-Path -LiteralPath $probeSource)) { Fail "missing $probeSource" }
    $targets = @{ 'REDcoLAB_probe.dll' = $probeSource }
    Write-Host "build target  : PROBE (diagnostic, never modifies pixels)"
}
else {
    $targets = @{
        'REDcoLAB.cpm' = $source
        'REDcoLAB.dll' = $source
    }
    Write-Host "build target  : product"
}
Write-Host ""

$built = @()
foreach ($name in $targets.Keys) {
    $out = Join-Path $distDir $name
    Write-Host "compiling -> $name"
    $compilerArgs = $commonArgs + @('-o', $out, $targets[$name])
    $compilerOutput = & $gpp @compilerArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host $compilerOutput
        Fail "compile failed with exit code $LASTEXITCODE"
    }
    if ($compilerOutput) {
        # -Werror should have turned anything real into a failure already
        Write-Host $compilerOutput
        Fail "compiler emitted output despite -Werror"
    }
    $built += $out
}

# ---------------------------------------------------------------- verify output
foreach ($out in $built) {
    if (-not (Test-Path -LiteralPath $out)) { Fail "expected output missing: $out" }
    $item = Get-Item -LiteralPath $out
    $bytes = [System.IO.File]::ReadAllBytes($out)

    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
        Fail "$($item.Name) is not a PE image"
    }

    # machine type lives at e_lfanew + 4
    $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3C)
    $machine = [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne 0x8664) { Fail "$($item.Name) is not x64 (machine=0x$('{0:X4}' -f $machine))" }

    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    if ($ascii.IndexOf('TriglavPluginCall') -lt 0) {
        Fail "$($item.Name) does not export TriglavPluginCall"
    }

    $hash = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash
    Write-Host ("built         : {0}  {1} bytes  x64  exports TriglavPluginCall  sha256={2}" -f `
                $item.Name, $item.Length, $hash.Substring(0, 16))
}

Write-Host ""
if ($KindProbe) {
    Write-Host "MODULE-KIND PROBE BUILD OK - dist\REDcoLAB_kindprobe.dll"
    Write-Host ""
    Write-Host "Logs to %TEMP%\REDcoLAB_kindprobe.log (always)."
    Write-Host "Declared module kind is chosen by the REDCOLAB_KIND environment variable:"
    Write-Host "  0x4380 (default) = Filter"
    Write-Host "  5530             = FilterActivation"
    Write-Host "  0                = do not call setModuleKind at all"
}
elseif ($Probe) {
    Write-Host "PROBE BUILD OK - dist\REDcoLAB_probe.dll"
    Write-Host ""
    Write-Host "The probe always logs to %TEMP%\REDcoLAB_probe.log and never changes pixels."
    Write-Host "Install it with:"
    Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_plugin.ps1 -Probe -Only 1 -Execute"
    Write-Host "Then fully restart CSP and look at both the Filter menu and that log file."
}
else {
    Write-Host "BUILD OK - dist\REDcoLAB.cpm and dist\REDcoLAB.dll"
    Write-Host "Next: powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1"
}
exit 0
