# REDcoLAB test runner
#
# Runs the mock-host lifecycle test against the built artefacts. This proves the
# SDK calling convention and the filter maths. It does NOT prove that CSP loads
# the plug-in - that requires a real CSP restart (see tools/install_plugin.ps1).
#
# ASCII ONLY: PowerShell 5.1 reads .ps1 using the system ANSI code page.

[CmdletBinding()]
param(
    [string]$Python,
    [switch]$KeepGoing
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir     = Join-Path $projectRoot 'dist'

function Fail([string]$message) {
    Write-Host "TESTS FAILED: $message" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- python
if (-not $Python) {
    $record = Join-Path $env:LOCALAPPDATA 'redkolab-tools\python_path.txt'
    if (Test-Path -LiteralPath $record) {
        $recorded = (Get-Content -LiteralPath $record -TotalCount 1).Trim()
        if ($recorded -and (Test-Path -LiteralPath $recorded)) { $Python = $recorded }
    }
}
if (-not $Python) {
    foreach ($candidate in @(
        (Join-Path $env:USERPROFILE '.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe'),
        'C:\Python312\python.exe'
    )) {
        if (Test-Path -LiteralPath $candidate) { $Python = $candidate; break }
    }
}
if (-not $Python) {
    $cmd = Get-Command python -ErrorAction SilentlyContinue
    if ($cmd) { $Python = $cmd.Source }
}
if (-not $Python) { Fail "python not found; pass -Python <path>" }

Write-Host "=== REDcoLAB tests ==="
Write-Host "python        : $Python"
Write-Host ""

$artefacts = @(
    (Join-Path $distDir 'REDcoLAB.cpm'),
    (Join-Path $distDir 'REDcoLAB.dll')
)

$existing = @()
foreach ($a in $artefacts) { if (Test-Path -LiteralPath $a) { $existing += $a } }
if ($existing.Count -eq 0) { Fail "no artefacts in dist/; run tools\build.ps1 first" }

$mockHost = Join-Path $PSScriptRoot 'mockhost.py'
$failures = 0

foreach ($artefact in $existing) {
    # Two host contracts per artefact:
    #   normal  - the plug-in's full expected contract
    #   minimal - optional host callbacks stripped; must still work, never crash
    foreach ($mode in @('normal', 'minimal')) {
        $label = Split-Path -Leaf $artefact
        if ($mode -eq 'minimal') { $label = "$label (minimal host)" }
        Write-Host "--- mock host lifecycle test: $label ---"

        if ($mode -eq 'minimal') {
            & $Python $mockHost $artefact --minimal
        }
        else {
            & $Python $mockHost $artefact
        }
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            $failures++
            Write-Host "  -> FAILED (exit $code)" -ForegroundColor Red
            if (-not $KeepGoing) { Fail "mock host test failed for $label" }
        }
        else {
            Write-Host "  -> PASS" -ForegroundColor Green
        }
        Write-Host ""
    }
}

if ($failures -gt 0) { Fail "$failures artefact(s) failed" }

# Algorithm/strength matrix: run once, against the canonical artefact. The
# reference formulas in algorithm_test.py are written independently of
# src/grayscale.h so a shared mistake cannot hide.
$matrixTarget = Join-Path $distDir 'REDcoLAB.cpm'
if (Test-Path -LiteralPath $matrixTarget) {
    Write-Host "--- algorithm / strength matrix test ---"
    & $Python (Join-Path $PSScriptRoot 'algorithm_test.py') $matrixTarget
    if ($LASTEXITCODE -ne 0) {
        Fail "algorithm matrix test failed (exit $LASTEXITCODE)"
    }
    Write-Host "  -> PASS" -ForegroundColor Green
    Write-Host ""
}

Write-Host "All mock-host tests PASS."
Write-Host ""
Write-Host "REMINDER: this does not prove CSP loads the plug-in."
Write-Host "Real-machine step:"
Write-Host "  1. powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_plugin.ps1"
Write-Host "  2. fully restart CLIP STUDIO PAINT"
Write-Host "  3. check Filter menu for 'REDcoLAB / yi jian hui du' and run it"
exit 0
