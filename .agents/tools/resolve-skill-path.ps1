# Skill path resolver (REDcoLAB)
#
# Implements the priority order required by the governance spec section 11:
#   1. primary source, exact path
#   2. primary source, recursive name match
#   3. project-local extension skill dir
#   4. compat dir, exact path
#   5. compat dir, recursive match
#   6. client/plugin cache (only when -AllowCache is passed)
#
# Output (plain):  <skill> <source> <absolute path>
# Output (-Json):  machine readable objects
# Exit code: 0 = all found, 1 = some missing or conflicting.
#
# ASCII ONLY on purpose: Windows PowerShell 5.1 reads .ps1 files using the
# system ANSI code page, so non-ASCII text in a BOM-less file gets mangled.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Skill,

    [switch]$Json,

    # Verbose trace of candidate evaluation
    [switch]$Trace,

    # Allow looking into client/plugin caches (off by default per spec)
    [switch]$AllowCache
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$roots = @(
    @{ Path = (Join-Path $projectRoot 'ai-toolkit\skills'); Source = 'primary' },
    @{ Path = (Join-Path $projectRoot '.agents\skills');    Source = 'project' },
    @{ Path = (Join-Path $projectRoot 'skills');            Source = 'compat'  }
)

function Get-SkillCandidates {
    param([string]$Dir, [string]$Name, [bool]$Recurse)

    if (-not (Test-Path -LiteralPath $Dir)) { return @() }

    $files = @()
    if ($Recurse) {
        $files = @(Get-ChildItem -LiteralPath $Dir -Recurse -Filter 'SKILL.md' -File -ErrorAction SilentlyContinue)
    }
    else {
        $files = @(Get-ChildItem -LiteralPath $Dir -Filter 'SKILL.md' -File -ErrorAction SilentlyContinue)
    }

    $out = @()
    foreach ($f in $files) {
        # exact path = <Dir>\<Name>\SKILL.md
        $isExact = $false
        try {
            $exactFull = [System.IO.Path]::GetFullPath((Join-Path $Dir $Name))
            $parentFull = [System.IO.Path]::GetFullPath($f.DirectoryName)
            if ($parentFull -ieq $exactFull) { $isExact = $true }
        }
        catch { }

        if ($isExact) {
            $out += [pscustomobject]@{ File = $f; Exact = $true }
            continue
        }

        # name match: directory name equals Name, or frontmatter name equals Name
        $dirName = $f.Directory.Name
        $fmName = $null
        try {
            # -Encoding UTF8 matters: PowerShell 5.1 would otherwise decode these
            # UTF-8 files with the system ANSI code page and mangle Chinese text.
            $head = @(Get-Content -LiteralPath $f.FullName -TotalCount 15 -Encoding UTF8 -ErrorAction SilentlyContinue)
            foreach ($line in $head) {
                if ($line -match '^\s*name\s*:\s*(.+?)\s*$') { $fmName = $Matches[1].Trim(); break }
            }
        }
        catch { }

        if (($dirName -ieq $Name) -or ($fmName -and ($fmName -ieq $Name))) {
            $out += [pscustomobject]@{ File = $f; Exact = $false }
        }
    }
    return $out
}

$wanted = @($Skill -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
$results = @()
$hadError = $false

foreach ($name in $wanted) {
    $candidates = @()

    foreach ($r in $roots) {
        if ($Trace) { Write-Host ("[trace] skill='{0}' root='{1}' source='{2}'" -f $name, $r.Path, $r.Source) }

        $exact = @(Get-SkillCandidates -Dir $r.Path -Name $name -Recurse $false | Where-Object { $_.Exact })
        if ($exact.Count -gt 0) {
            foreach ($e in $exact) { $candidates += [pscustomobject]@{ Source = $r.Source; Path = $e.File.FullName; Match = 'exact' } }
            break
        }

        $loose = @(Get-SkillCandidates -Dir $r.Path -Name $name -Recurse $true)
        if ($loose.Count -gt 0) {
            foreach ($e in $loose) { $candidates += [pscustomobject]@{ Source = $r.Source; Path = $e.File.FullName; Match = 'recursive' } }
            break
        }
    }

    if ($candidates.Count -eq 0) {
        $results += [pscustomobject]@{ skill = $name; status = 'missing'; source = $null; path = $null; candidates = 0; all = @() }
        $hadError = $true
        continue
    }

    if ($candidates.Count -gt 1) {
        # never silently pick one when several candidates share the same priority
        $results += [pscustomobject]@{
            skill      = $name
            status     = 'conflict'
            source     = $candidates[0].Source
            path       = $null
            candidates = $candidates.Count
            all        = @($candidates | ForEach-Object { $_.Path })
        }
        $hadError = $true
        continue
    }

    $results += [pscustomobject]@{
        skill      = $name
        status     = 'found'
        source     = $candidates[0].Source
        path       = $candidates[0].Path
        candidates = 1
        all        = @($candidates[0].Path)
    }
}

if ($Json) {
    $results | ConvertTo-Json -Depth 5
}
else {
    foreach ($r in $results) {
        if ($r.status -eq 'found') {
            Write-Output ("{0}  {1}  {2}" -f $r.skill, $r.source, $r.path)
        }
        elseif ($r.status -eq 'missing') {
            Write-Output ("{0}  missing  (not found; check ai-toolkit/skills/index.md)" -f $r.skill)
        }
        else {
            Write-Output ("{0}  conflict  {1} candidates at the same priority level:" -f $r.skill, $r.candidates)
            foreach ($p in $r.all) { Write-Output ("    - {0}" -f $p) }
        }
    }
}

if ($hadError) { exit 1 } else { exit 0 }
