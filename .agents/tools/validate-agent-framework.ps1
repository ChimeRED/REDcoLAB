# Framework validator (REDcoLAB)
#
# Implements the checks required by the governance spec section 12.
# Exit code: 0 = all passed, 1 = must-fix errors, 2 = warnings only.
#
# ASCII ONLY on purpose: Windows PowerShell 5.1 reads .ps1 files using the
# system ANSI code page, so non-ASCII text in a BOM-less file gets mangled.

[CmdletBinding()]
param(
    [switch]$Json
)

$ErrorActionPreference = 'Continue'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$checks = @()

function Add-Check {
    param([string]$Name, [string]$Status, [string]$Detail)
    $script:checks += [pscustomobject]@{ name = $Name; status = $Status; detail = $Detail }
}

function Test-Rel {
    param([string]$Rel)
    return (Test-Path -LiteralPath (Join-Path $projectRoot $Rel))
}

# IMPORTANT: Windows PowerShell 5.1 decodes files using the system ANSI code page
# unless told otherwise. Our Markdown/JSON files are UTF-8, and Chinese text read
# through the wrong code page produces byte sequences that even break parsing.
# Always read file content through this helper.
function Read-Utf8Text {
    param([string]$LiteralPath)
    try {
        return [System.IO.File]::ReadAllText($LiteralPath, (New-Object System.Text.UTF8Encoding($false)))
    }
    catch {
        return $null
    }
}

# 1. root entry
if (Test-Rel 'AGENTS.md') { Add-Check 'root-entry' 'PASS' 'AGENTS.md' }
else { Add-Check 'root-entry' 'FAIL' 'AGENTS.md is missing' }

# 2. project rules referenced by the root entry
if (Test-Rel 'ai-toolkit\AGENTS.md') { Add-Check 'toolkit-rules' 'PASS' 'ai-toolkit\AGENTS.md' }
else { Add-Check 'toolkit-rules' 'FAIL' 'ai-toolkit\AGENTS.md is missing' }

# 3. skill index referenced by the project rules
$skillIndex = 'ai-toolkit\skills\index.md'
if (Test-Rel $skillIndex) { Add-Check 'skill-index' 'PASS' $skillIndex }
else { Add-Check 'skill-index' 'FAIL' "$skillIndex is missing" }

# 4. version file format
if (Test-Rel 'ai-toolkit\VERSION') {
    $ver = (Get-Content -LiteralPath (Join-Path $projectRoot 'ai-toolkit\VERSION') -TotalCount 1).Trim()
    if ($ver -match '^\d+\.\d+\.\d+$') { Add-Check 'version-format' 'PASS' "VERSION=$ver" }
    else { Add-Check 'version-format' 'FAIL' "VERSION is not MAJOR.MINOR.PATCH: '$ver'" }
}
else { Add-Check 'version-format' 'FAIL' 'ai-toolkit\VERSION is missing' }

# 5+6. resolver exists, and every skill named in the index resolves uniquely
$resolver = Join-Path $projectRoot '.agents\tools\resolve-skill-path.ps1'
$declared = @()
if (Test-Path -LiteralPath (Join-Path $projectRoot $skillIndex)) {
    $idx = Read-Utf8Text (Join-Path $projectRoot $skillIndex)
    $declared = @([regex]::Matches($idx, '(?m)^-\s+`([a-z0-9\-]+)`\s*$') | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
}

if (Test-Path -LiteralPath $resolver) {
    Add-Check 'resolver-exists' 'PASS' '.agents\tools\resolve-skill-path.ps1'

    if ($declared.Count -eq 0) {
        Add-Check 'skills-resolvable' 'FAIL' 'no skill entries parsed from the index'
    }
    else {
        $bad = @()
        foreach ($s in $declared) {
            $null = & powershell -NoProfile -ExecutionPolicy Bypass -File $resolver -Skill $s 2>&1
            if ($LASTEXITCODE -ne 0) { $bad += $s }
        }
        if ($bad.Count -eq 0) {
            Add-Check 'skills-resolvable' 'PASS' ("{0} skill(s) resolved: {1}" -f $declared.Count, ($declared -join ', '))
        }
        else {
            Add-Check 'skills-resolvable' 'FAIL' ("unresolved: {0}" -f ($bad -join ', '))
        }
    }
}
else {
    Add-Check 'resolver-exists' 'FAIL' '.agents\tools\resolve-skill-path.ps1 is missing'
}

# 7. skill directory names are unique
$skillsDir = Join-Path $projectRoot 'ai-toolkit\skills'
$skillFiles = @()
if (Test-Path -LiteralPath $skillsDir) {
    $dirs = @(Get-ChildItem -LiteralPath $skillsDir -Directory -ErrorAction SilentlyContinue)
    $dup = @($dirs | Group-Object { $_.Name.ToLower() } | Where-Object { $_.Count -gt 1 })
    if ($dup.Count -gt 0) {
        Add-Check 'skill-name-unique' 'FAIL' ("duplicate: {0}" -f (($dup | ForEach-Object { $_.Name }) -join ', '))
    }
    else {
        Add-Check 'skill-name-unique' 'PASS' ("{0} skill director(ies)" -f $dirs.Count)
    }
    $skillFiles = @(Get-ChildItem -LiteralPath $skillsDir -Recurse -Filter 'SKILL.md' -File -ErrorAction SilentlyContinue)
}
else {
    Add-Check 'skill-name-unique' 'FAIL' 'ai-toolkit\skills is missing'
}

# 8. SKILL.md frontmatter has name and description
$fmProblems = @()
foreach ($f in $skillFiles) {
    $text = Read-Utf8Text $f.FullName
    # anchor at the very start of the file and stop at the FIRST closing '---'
    if ($text -notmatch '(?s)\A---\s*\r?\n(.*?)\r?\n---') { $fmProblems += "$($f.Directory.Name): no frontmatter"; continue }
    $fm = $Matches[1]
    if ($fm -notmatch '(?m)^name\s*:') { $fmProblems += "$($f.Directory.Name): no name" }
    if ($fm -notmatch '(?m)^description\s*:') { $fmProblems += "$($f.Directory.Name): no description" }
}
if ($fmProblems.Count -eq 0) { Add-Check 'skill-frontmatter' 'PASS' 'all SKILL.md have name/description' }
else { Add-Check 'skill-frontmatter' 'FAIL' ($fmProblems -join '; ') }

# 9. SKILL.md contains the 7 required sections (matched by their English markers
#    so this file can stay pure ASCII; the markers exist in every SKILL.md)
$required = @(
    'Trigger conditions',
    'Not applicable',
    'Core invariants',
    'Standard workflow',
    'Read on demand',
    'Verification',
    'Error handling'
)
$secProblems = @()
foreach ($f in $skillFiles) {
    $text = Read-Utf8Text $f.FullName
    foreach ($sec in $required) {
        if ($text -notmatch [regex]::Escape($sec)) { $secProblems += "$($f.Directory.Name): missing section marker [$sec]" }
    }
}
if ($secProblems.Count -eq 0) { Add-Check 'skill-sections' 'PASS' 'all SKILL.md contain the 7 required sections' }
else { Add-Check 'skill-sections' 'FAIL' ($secProblems -join '; ') }

# 10. error directory exists and is writable
$errDir = Join-Path $projectRoot '.agents\skill-error-records'
if (Test-Path -LiteralPath $errDir) {
    $probe = Join-Path $errDir ('.write-probe-' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        Set-Content -LiteralPath $probe -Value 'probe' -Encoding ASCII -ErrorAction Stop
        Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue
        Add-Check 'error-dir-writable' 'PASS' '.agents\skill-error-records is writable'
    }
    catch {
        Add-Check 'error-dir-writable' 'FAIL' ("not writable: {0}" -f $_.Exception.Message)
    }
}
else { Add-Check 'error-dir-writable' 'FAIL' '.agents\skill-error-records is missing' }

# 11. error record template exists
if (Test-Rel 'ai-toolkit\templates\skill-error-record.md') { Add-Check 'error-template' 'PASS' 'ai-toolkit\templates\skill-error-record.md' }
else { Add-Check 'error-template' 'FAIL' 'error record template is missing' }

# 12. broken reference check for `references/...` style links inside SKILL.md
$refProblems = @()
foreach ($f in $skillFiles) {
    $text = Read-Utf8Text $f.FullName
    foreach ($m in [regex]::Matches($text, '`(references/[^`]+)`')) {
        $rel = $m.Groups[1].Value
        $target = Join-Path $f.Directory.FullName $rel
        if (-not (Test-Path -LiteralPath $target)) { $refProblems += "$($f.Directory.Name) -> $rel" }
    }
}
if ($refProblems.Count -eq 0) { Add-Check 'references-exist' 'PASS' 'no broken references' }
else { Add-Check 'references-exist' 'WARN' ("declared but not built yet: {0}" -f ($refProblems -join '; ')) }

# 13. credential scan over framework files
$scanTargets = @()
foreach ($rel in @('AGENTS.md', 'ai-toolkit', '.agents')) {
    $p = Join-Path $projectRoot $rel
    if (Test-Path -LiteralPath $p) {
        $scanTargets += @(Get-ChildItem -LiteralPath $p -Recurse -File -Include *.md, *.ps1, *.json, *.yml, *.yaml -ErrorAction SilentlyContinue)
    }
}
$secretPattern = '(ghp_[A-Za-z0-9]{20,})|(gho_[A-Za-z0-9]{20,})|(github_pat_[A-Za-z0-9_]{20,})|(AKIA[0-9A-Z]{16})|(BEGIN [A-Z ]*PRIVATE KEY)'
$secretHits = @()
foreach ($f in $scanTargets) {
    $text = Read-Utf8Text $f.FullName
    if ($text -and ($text -match $secretPattern)) { $secretHits += $f.FullName }
}
if ($secretHits.Count -eq 0) { Add-Check 'no-secrets' 'PASS' ("scanned {0} framework file(s)" -f $scanTargets.Count) }
else { Add-Check 'no-secrets' 'FAIL' ("suspected credential in: {0}" -f ($secretHits -join ', ')) }

# 14. safety invariant: the forbidden screen-capture approach must not reappear
$forbidden = @()
$srcDir = Join-Path $projectRoot 'src'
if (Test-Path -LiteralPath $srcDir) {
    foreach ($f in Get-ChildItem -LiteralPath $srcDir -Recurse -File -ErrorAction SilentlyContinue) {
        $text = Read-Utf8Text $f.FullName
        if (-not $text) { continue }
        if ($text -match 'BitBlt|PrintWindow|GetDesktopWindow|CopyFromScreen|CreateCompatibleDC') {
            $forbidden += "$($f.Name): screen capture API"
        }
    }
}
if ($forbidden.Count -eq 0) { Add-Check 'no-screen-capture' 'PASS' 'no screen capture API found under src/' }
else { Add-Check 'no-screen-capture' 'FAIL' ($forbidden -join '; ') }

# 15. safety invariant: CELSYS SDK headers must never be tracked or packaged
$tracked = @()
$gitExe = Join-Path $env:LOCALAPPDATA 'redkolab-tools\git\cmd\git.exe'
if (Test-Path -LiteralPath $gitExe) {
    $out = & $gitExe -C $projectRoot ls-files 2>&1
    if ($LASTEXITCODE -eq 0 -and $out) {
        $tracked = @($out | Where-Object { $_ -match 'TriglavPlugIn.*\.h$' })
    }
}
if ($tracked.Count -eq 0) { Add-Check 'sdk-not-tracked' 'PASS' 'no CELSYS SDK header is tracked by git' }
else { Add-Check 'sdk-not-tracked' 'FAIL' ("SDK headers tracked: {0}" -f ($tracked -join ', ')) }

# ---- summary ----
$passed = @($checks | Where-Object { $_.status -eq 'PASS' }).Count
$failed = @($checks | Where-Object { $_.status -eq 'FAIL' }).Count
$warned = @($checks | Where-Object { $_.status -eq 'WARN' }).Count

if ($Json) {
    [pscustomobject]@{ checks = $checks; passed = $passed; failed = $failed; warnings = $warned } | ConvertTo-Json -Depth 5
}
else {
    foreach ($c in $checks) {
        $detail = ''
        if ($c.detail) { $detail = ": $($c.detail)" }
        Write-Output ("[{0}] {1}{2}" -f $c.status, $c.name, $detail)
    }
    Write-Output ''
    Write-Output ("Summary: {0} passed, {1} failed, {2} warning" -f $passed, $failed, $warned)
}

if ($failed -gt 0) { exit 1 }
elseif ($warned -gt 0) { exit 2 }
else { exit 0 }
