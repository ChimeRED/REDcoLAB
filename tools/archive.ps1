# REDcoLAB archive / release script
#
# Pipeline: verify -> build -> test -> package -> scan package -> commit -> tag
#           -> (only with -Push, after your confirmation) push -> Release
#
# Safety rules baked in:
#   * the archive is scanned and ABORTED if any CELSYS header leaked in
#   * no --force / --force-with-lease anywhere
#   * pushing is opt-in via -Push; without it nothing leaves the machine
#   * the version is single-sourced from ai-toolkit/VERSION and cross-checked
#     against the version constant compiled into the plug-in
#
# ASCII ONLY: PowerShell 5.1 reads .ps1 using the system ANSI code page.

[CmdletBinding()]
param(
    [string]$Message,
    [switch]$Push,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir     = Join-Path $projectRoot 'dist'
$versionFile = Join-Path $projectRoot 'ai-toolkit\VERSION'
$sourceFile  = Join-Path $projectRoot 'src\REDcoLAB.cpp'
$changelog   = Join-Path $projectRoot 'ai-toolkit\CHANGELOG.md'

function Fail([string]$message) {
    Write-Host "ARCHIVE FAILED: $message" -ForegroundColor Red
    exit 1
}
function Step([string]$message) {
    Write-Host ""
    Write-Host "=== $message ===" -ForegroundColor Cyan
}

$git = Join-Path $env:LOCALAPPDATA 'redkolab-tools\git\cmd\git.exe'
$gh  = Join-Path $env:LOCALAPPDATA 'redkolab-tools\gh\bin\gh.exe'
if (-not (Test-Path -LiteralPath $git)) {
    $onPath = Get-Command git -ErrorAction SilentlyContinue
    if ($onPath) { $git = $onPath.Source } else { Fail "git not found" }
}

Step "preflight"
$version = (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Fail "bad VERSION: '$version'" }
Write-Host "version       : $version"

if (-not $Message) { Fail "pass -Message 'what changed and why' (commit message must be meaningful)" }

# version single-sourcing: the constant compiled into the plug-in must match
$sourceText = [System.IO.File]::ReadAllText($sourceFile, (New-Object System.Text.UTF8Encoding($false)))
if ($sourceText -notmatch 'kVersionString\s*=\s*"([0-9.]+)"') {
    Fail "could not find kVersionString in src\REDcoLAB.cpp"
}
$sourceVersion = $Matches[1]
if ($sourceVersion -ne $version) {
    Fail "version mismatch: VERSION=$version but src\REDcoLAB.cpp kVersionString=$sourceVersion"
}
Write-Host "source version: $sourceVersion (matches)"

# the SDK must never be tracked
$tracked = & $git -C $projectRoot ls-files 2>&1
if ($tracked) {
    $leaked = @($tracked | Where-Object { $_ -match 'TriglavPlugIn.*\.h$' -or $_ -match '^reference/' })
    if ($leaked.Count -gt 0) {
        Fail ("CELSYS files are tracked by git: " + ($leaked -join ', '))
    }
}
Write-Host "sdk leak check: clean"

Step "build"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE -ne 0) { Fail "build failed" }

if (-not $SkipTests) {
    Step "tests"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_tests.ps1')
    if ($LASTEXITCODE -ne 0) { Fail "tests failed" }
}

Step "framework self-check"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot '.agents\tools\validate-agent-framework.ps1')
$frameworkCode = $LASTEXITCODE
if ($frameworkCode -eq 1) { Fail "framework validator reported must-fix errors" }
if ($frameworkCode -eq 2) { Write-Host "framework validator: warnings only (acceptable)" }

Step "package"
$zipName = "REDcoLAB-$version.zip"
$zipPath = Join-Path $distDir $zipName
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }

$stage = Join-Path $env:TEMP ("redcolab_pack_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $stage | Out-Null

$packItems = @(
    @{ Source = (Join-Path $distDir 'REDcoLAB.cpm'); Name = 'REDcoLAB.cpm' },
    @{ Source = (Join-Path $distDir 'REDcoLAB.dll'); Name = 'REDcoLAB.dll' },
    @{ Source = (Join-Path $projectRoot 'README.md'); Name = 'README.md' }
)
foreach ($item in $packItems) {
    if (-not (Test-Path -LiteralPath $item.Source)) { Fail ("missing pack input: " + $item.Source) }
    Copy-Item -LiteralPath $item.Source -Destination (Join-Path $stage $item.Name) -Force
}
Copy-Item -LiteralPath $changelog -Destination (Join-Path $stage 'CHANGELOG.md') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\用户手册.md') -Destination (Join-Path $stage '用户手册.md') -Force

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force
Remove-Item -LiteralPath $stage -Recurse -Force

$zipItem = Get-Item -LiteralPath $zipPath
Write-Host ("package       : {0}  {1} bytes" -f $zipItem.Name, $zipItem.Length)

Step "package content scan (must contain no CELSYS material)"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $names = @($archive.Entries | ForEach-Object { $_.FullName })
    Write-Host "entries:"
    foreach ($n in $names) { Write-Host ("  - {0}" -f $n) }

    $bad = @($names | Where-Object { $_ -match 'TriglavPlugIn' -or $_ -match '\.h$' -or $_ -match 'sdk' })
    if ($bad.Count -gt 0) {
        $archive.Dispose()
        Remove-Item -LiteralPath $zipPath -Force
        Fail ("package contains CELSYS/SDK material, deleted: " + ($bad -join ', '))
    }

    # deeper scan: read every text entry looking for the SDK header banner
    foreach ($entry in $archive.Entries) {
        if ($entry.Length -eq 0 -or $entry.Length -gt 2MB) { continue }
        if ($entry.FullName -match '\.(cpm|dll)$') { continue }
        $reader = New-Object System.IO.StreamReader($entry.Open())
        $content = $reader.ReadToEnd()
        $reader.Close()
        if ($content -match 'TriglavPlugIn SDK') {
            $archive.Dispose()
            Remove-Item -LiteralPath $zipPath -Force
            Fail ("package entry '$($entry.FullName)' mentions the TriglavPlugIn SDK, deleted")
        }
    }
}
finally {
    if ($archive) { try { $archive.Dispose() } catch { } }
}
Write-Host "scan          : clean, no CELSYS material in the package"

Step "git status"
& $git -C $projectRoot status --short

Step "commit"
& $git -C $projectRoot add -A
if ($LASTEXITCODE -ne 0) { Fail "git add failed" }

# refuse to commit if anything CELSYS-shaped got staged
$staged = & $git -C $projectRoot diff --cached --name-only
$stagedLeak = @($staged | Where-Object { $_ -match 'TriglavPlugIn.*\.h$' -or $_ -match '^reference/' -or $_ -match '^sdk_official/' })
if ($stagedLeak.Count -gt 0) {
    & $git -C $projectRoot reset | Out-Null
    Fail ("refusing to commit CELSYS files: " + ($stagedLeak -join ', '))
}

& $git -C $projectRoot commit -m $Message
if ($LASTEXITCODE -ne 0) { Fail "git commit failed (nothing to commit?)" }

$commit = (& $git -C $projectRoot rev-parse --short HEAD).Trim()
Write-Host "commit        : $commit"

Step "tag"
$tag = "v$version"
$exists = & $git -C $projectRoot tag -l $tag
if ($exists) { Fail "tag $tag already exists; bump ai-toolkit/VERSION first" }
& $git -C $projectRoot tag -a $tag -m "REDcoLAB $version"
if ($LASTEXITCODE -ne 0) { Fail "git tag failed" }
Write-Host "tag           : $tag"

if (-not $Push) {
    Write-Host ""
    Write-Host "LOCAL ONLY - nothing was pushed." -ForegroundColor Yellow
    Write-Host "To publish (this REPLACES the current version on GitHub):"
    Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File tools\archive.ps1 -Message '...' -Push"
    Write-Host ""
    Write-Host "push would run:"
    Write-Host "  git push origin main"
    Write-Host "  git push origin $tag"
    Write-Host "  gh release create $tag $zipName"
    exit 0
}

Step "push (user-confirmed)"
& $git -C $projectRoot push origin main
if ($LASTEXITCODE -ne 0) { Fail "git push main failed" }

& $git -C $projectRoot push origin $tag
if ($LASTEXITCODE -ne 0) { Fail "git push tag failed" }

Step "GitHub Release"
if (Test-Path -LiteralPath $gh) {
    & $gh release create $tag $zipPath --title "REDcoLAB $version" --notes "See CHANGELOG.md. Build artefacts: REDcoLAB.cpm, REDcoLAB.dll."
    if ($LASTEXITCODE -ne 0) { Write-Host "gh release failed (tag is already pushed, release can be added later)" -ForegroundColor Yellow }
    else { Write-Host "release       : created" -ForegroundColor Green }
}
else {
    Write-Host "gh not found; tag was pushed but no Release was created" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "ARCHIVE OK - $version published" -ForegroundColor Green
exit 0
