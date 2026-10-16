# REDcoLAB real-machine loading test
#
# Runs everything that can be automated, then hands the two steps that need a
# human (restart CSP, look at the Filter menu) back to you with exact
# instructions.
#
# Why the probe: the shipping plug-in only logs when REDCOLAB_LOG=1, and its
# log looks identical to a mock-host test run. The probe always logs, opens
# with an unmistakable banner, and dumps the host's record/service suite, so a
# FAILED real-machine test still produces actionable evidence instead of
# silence.
#
# ASCII ONLY: PowerShell 5.1 reads .ps1 using the system ANSI code page.

[CmdletBinding()]
param(
    # Which candidate directory to test. 1 = the recommended location.
    [int]$Only = 1,
    # Run without copying anything into CSP's folders.
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir     = Join-Path $projectRoot 'dist'

function Fail([string]$message) {
    Write-Host "REAL-MACHINE TEST SETUP FAILED: $message" -ForegroundColor Red
    exit 1
}
function Step([string]$message) {
    Write-Host ""
    Write-Host "=== $message ===" -ForegroundColor Cyan
}

Write-Host "REDcoLAB real-machine loading test" -ForegroundColor White
Write-Host ""

Step "preconditions"
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'sdk_official'))) {
    Fail "sdk_official/ is missing; the CELSYS SDK headers are required to build"
}
$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) {
    $pkgRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    $winlibs = Get-ChildItem -LiteralPath $pkgRoot -Directory -Filter 'BrechtSanders.WinLibs*' -ErrorAction SilentlyContinue |
               Select-Object -First 1
    if (-not $winlibs) { Fail "g++ not found; install MinGW-w64 first" }
}
Write-Host "compiler      : OK"
Write-Host "sdk headers   : OK"

Step "build the diagnostic probe"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1') -Probe
if ($LASTEXITCODE -ne 0) { Fail "probe build failed" }

Step "verify the probe locally before touching CSP's folders"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_tests.ps1')
if ($LASTEXITCODE -ne 0) { Fail "probe verification failed; not installing anything" }

$logPath = Join-Path $env:TEMP 'REDcoLAB_probe.log'
Step "clear any previous probe log"
if (Test-Path -LiteralPath $logPath) {
    Remove-Item -LiteralPath $logPath -Force
    Write-Host "removed old log"
}
else {
    Write-Host "no previous log"
}

Step "install the probe"
if ($DryRun) {
    Write-Host "DRY RUN: would run install_plugin.ps1 -Probe -Only $Only -Execute"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'install_plugin.ps1') -Probe -Only $Only
}
else {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'install_plugin.ps1') -Probe -Only $Only -Execute
    if ($LASTEXITCODE -ne 0) { Fail "probe installation failed" }
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host " NOW DO THESE TWO THINGS" -ForegroundColor Yellow
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host ""
Write-Host " 1. FULLY EXIT Clip Studio Paint (quit the program, not just the file)"
Write-Host " 2. Start it again, then open the Filter menu"
Write-Host ""
Write-Host " Look for a category named:  REDcoLAB PROBE" -ForegroundColor White
Write-Host " (the probe is named 'probe only - no pixel changes'; applying it changes nothing)"
Write-Host ""
Write-Host " Then report ONE of these three outcomes:"
Write-Host ""
Write-Host "   A) The menu entry appears"
Write-Host "      -> the install location works. We can then narrow the .dll vs .cpm"
Write-Host "         question and lock the location in."
Write-Host ""
Write-Host "   B) It does not appear, and this log file is EMPTY or missing:"
Write-Host "        $logPath"
Write-Host "      -> CSP never loaded the file. The location is wrong."
Write-Host "         Next: re-run with -Only 6 (the CSP program folder)."
Write-Host ""
Write-Host "   C) It does not appear, but the log file HAS content:"
Write-Host "      -> the file was loaded but registration did not complete."
Write-Host "         Send me the log; it lists exactly which host callbacks exist."
Write-Host ""
Write-Host " The log also proves loading on its own: its second line is"
Write-Host "   '*** CSP LOADED THE FILE ***'"
Write-Host " which the mock host cannot be confused with."
Write-Host ""
exit 0
