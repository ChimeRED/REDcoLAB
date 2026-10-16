# REDcoLAB plug-in installer
#
# The single most important open question for this project is WHICH directory
# and WHICH file extension CLIP STUDIO PAINT 5.0 actually loads a side-loaded
# plug-in from. Rather than guess, this script installs the artefact into every
# plausible candidate location so one CSP restart gives a definitive answer.
#
# Evidence behind the candidate list:
#   - CSP 5.0.4 main binary contains the literal path fragments
#     "CLIPStudioModule" and "PlugIn" (found by byte scan).
#   - CSP ships its own filters as PlugIn\PAINT\*.dll next to the executable.
#   - %APPDATA%\...\CELSYS\CLIPStudioPlugIn\PAINT exists (empty) - CSP itself
#     creates it, which is a strong hint that it is scanned.
#   - %APPDATA%\...\CELSYS\CLIPStudioModule\PlugIn\PAINT\win was created by an
#     older attempt; keep it in the list because it may still be valid.
#   - The .cpm literal does NOT appear anywhere in the CSP binary.
#
# Default is a DRY RUN. Pass -Execute to actually copy files.
#
# ASCII ONLY: PowerShell 5.1 reads .ps1 using the system ANSI code page.

[CmdletBinding()]
param(
    [switch]$Execute,
    [switch]$Clean,
    # Install into exactly ONE candidate (by its number in the list) instead of
    # all of them. Use this once the load location is known, or to isolate
    # which location actually works by testing them one at a time.
    [int]$Only = 0,
    # Install the diagnostic probe instead of the product. The probe always
    # logs and never changes pixels, so a failed real-machine test still yields
    # evidence.
    [switch]$Probe
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir     = Join-Path $projectRoot 'dist'

function Fail([string]$message) {
    Write-Host "INSTALL FAILED: $message" -ForegroundColor Red
    exit 1
}

Write-Host "=== REDcoLAB installer ==="
Write-Host "mode          : $(if ($Execute) { 'EXECUTE (will write files)' } else { 'DRY RUN (pass -Execute to write)' })"
Write-Host ""

# ---------------------------------------------------------------- artefacts
if ($Probe) {
    $probePath = Join-Path $distDir 'REDcoLAB_probe.dll'
    if (-not (Test-Path -LiteralPath $probePath)) {
        Fail "probe not built; run: tools\build.ps1 -Probe"
    }
    $artefacts = @($probePath)
    Write-Host "artefact set  : PROBE build (diagnostic, changes no pixels)" -ForegroundColor Yellow
}
else {
    $artefacts = @()
    foreach ($name in @('REDcoLAB.cpm', 'REDcoLAB.dll')) {
        $p = Join-Path $distDir $name
        if (Test-Path -LiteralPath $p) { $artefacts += $p }
    }
    if ($artefacts.Count -eq 0) { Fail "no artefacts in dist\; run tools\build.ps1 first" }
}

foreach ($a in $artefacts) {
    $item = Get-Item -LiteralPath $a
    Write-Host ("artefact      : {0}  {1} bytes" -f $item.Name, $item.Length)
}
Write-Host ""

# ---------------------------------------------------------------- candidates
# Each entry: target directory + the file names to place there.
$appData = $env:APPDATA
$documents = [Environment]::GetFolderPath('MyDocuments')
$cspProgramPlugIn = 'C:\build\csp\CLIP STUDIO 1.5\CLIP STUDIO PAINT\PlugIn\PAINT'

$candidates = @(
    @{
        Path = $cspProgramPlugIn
        Note = 'CONFIRMED WORKING (2026-10-16): CSP loads .dll from here. Recommended.'
    },
    @{
        Path = Join-Path $appData 'CELSYSUserData\CELSYS\CLIPStudioPlugIn\PAINT'
        Note = 'unconfirmed; earlier test was invalidated by a TEMP-path mistake'
    },
    @{
        Path = Join-Path $appData 'CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT\win'
        Note = 'unconfirmed; used by the previous attempt'
    },
    @{
        Path = Join-Path $appData 'CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT'
        Note = 'unconfirmed; same tree without the win subfolder'
    },
    @{
        Path = Join-Path $documents 'CELSYS\CLIPStudioModule\Plugin\PAINT'
        Note = 'documented plug-in location for the filter-plug-in SDK'
    },
    @{
        Path = Join-Path $documents 'CELSYS\CLIPStudioPlugIn\PAINT'
        Note = 'documented location, alternate spelling'
    }
)

Write-Host "candidate install locations:"
$index = 1
foreach ($c in $candidates) {
    $exists = Test-Path -LiteralPath $c.Path
    $marker = ''
    if ($Only -gt 0 -and $index -eq $Only) { $marker = '   <== selected by -Only' }
    Write-Host ("  [{0}] {1}{2}" -f $index, $c.Path, $marker)
    Write-Host ("      exists: {0}   note: {1}" -f $exists, $c.Note)
    $index++
}
Write-Host ""

if ($Only -gt 0) {
    if ($Only -gt $candidates.Count) { Fail "-Only $Only is out of range (1..$($candidates.Count))" }
    $candidates = @($candidates[$Only - 1])
    Write-Host ("restricted to candidate [{0}]: {1}" -f $Only, $candidates[0].Path)
    Write-Host ""
}

if (-not $Execute) {
    Write-Host "DRY RUN - nothing was written."
    Write-Host "Review the list above, then re-run with -Execute."
    Write-Host ""
    Write-Host "After installing:"
    Write-Host "  1. FULLY EXIT CLIP STUDIO PAINT (not just close the document)"
    Write-Host "  2. start it again"
    Write-Host "  3. open the Filter menu and look for the REDcoLAB category"
    Write-Host "  4. write down which of the locations above existed, and whether"
    Write-Host "     the menu entry appeared - that tells us which one works"
    exit 0
}

# ---------------------------------------------------------------- execute
Write-Host "=== writing files ==="
$written = 0
$failed = @()

foreach ($c in $candidates) {
    try {
        New-Item -ItemType Directory -Force -Path $c.Path | Out-Null
    }
    catch {
        $failed += ("{0} (cannot create: {1})" -f $c.Path, $_.Exception.Message)
        continue
    }

    foreach ($a in $artefacts) {
        $name = Split-Path -Leaf $a
        $target = Join-Path $c.Path $name
        if ($Clean -and (Test-Path -LiteralPath $target)) {
            Remove-Item -LiteralPath $target -Force
        }
        try {
            Copy-Item -LiteralPath $a -Destination $target -Force
            Write-Host ("  wrote {0}" -f $target) -ForegroundColor Green
            $written++
        }
        catch {
            $failed += ("{0} (copy failed: {1})" -f $target, $_.Exception.Message)
        }
    }
}

Write-Host ""
Write-Host ("files written : {0}" -f $written)
if ($failed.Count -gt 0) {
    Write-Host "could not write:"
    foreach ($f in $failed) { Write-Host ("  - {0}" -f $f) -ForegroundColor Yellow }
}

Write-Host ""
Write-Host "NEXT STEP (this is the test that actually matters):"
Write-Host "  1. FULLY EXIT CLIP STUDIO PAINT"
Write-Host "  2. start it again"
Write-Host "  3. Filter menu -> look for the REDcoLAB category -> 'yi jian hui du'"
Write-Host "  4. apply it to a raster layer and press Ctrl+Z afterwards to check undo"
Write-Host ""
Write-Host "If the entry does NOT appear, record which locations existed and report"
Write-Host "back: the next step is to narrow down the location/extension by removing"
Write-Host "them one at a time, not to guess."

if ($failed.Count -gt 0 -and $written -eq 0) { exit 1 }
exit 0
