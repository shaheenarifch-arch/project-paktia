<#
  Smoke run of the packaged MidnightHavoc.exe: launch, play for N seconds, close, then
  collect crashes / log errors / FPS into <OutDir>\summary.md and <OutDir>\result.json.
  Used by the "smoke-test" job in .github/workflows/unreal-build.yml. See game/docs/smoke-test.md.
  Exit code is always 0 when a summary was written; pass/fail is in result.json.
#>
param(
  [Parameter(Mandatory)][string]$BuildDir,   # folder containing MidnightHavoc.exe
  [Parameter(Mandatory)][string]$OutDir,
  [int]$Seconds = 180,
  [string]$Map = '',                         # optional map to open, e.g. /Game/Maps/Kharg
  [string]$ExecCmds = 'stat fps',            # extra console commands for the walkthrough, ';' separated
  [double]$MinFpsFloor = 15
)
$ErrorActionPreference = 'Continue'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$exe = Get-ChildItem -Path $BuildDir -Recurse -Filter MidnightHavoc.exe -ErrorAction SilentlyContinue |
  Sort-Object { $_.FullName.Length } | Select-Object -First 1
$startedAt = Get-Date
$reasons = @()
$exitInfo = 'not launched'
$log = Join-Path $OutDir 'smoke.log'

if (-not $exe) {
  $reasons += "MidnightHavoc.exe not found under $BuildDir"
} else {
  $args = @()
  if ($Map) { $args += $Map }
  $args += @('-log', "-abslog=$log", '-csvprofile', '-windowed', '-ResX=1280', '-ResY=720', '-RenderOffscreen',
             '-nosplash', '-unattended', '-nosound', "-ExecCmds=$ExecCmds",
             # The game exits by itself after $Seconds s (UHavocSmokeExitSubsystem) so the CSV and log are flushed;
             # -RenderOffscreen has no window, so closing the window from here cannot work.
             "-HavocSmokeSeconds=$Seconds")
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $args -PassThru
  # Allow time for startup and shutdown on top of the play time.
  $exited = $p.WaitForExit(($Seconds + 120) * 1000)
  $elapsed = [int]((Get-Date) - $startedAt).TotalSeconds
  if (-not $exited) {
    $p.Kill(); $p.WaitForExit()
    $reasons += "Game did not exit by itself within $($Seconds + 120) s (killed); CSV may be missing"
    $exitInfo = "ran $elapsed s, killed"
  } elseif ($elapsed -lt $Seconds) {
    $exitInfo = "exited early with code $($p.ExitCode) after $elapsed s"
    $reasons += "Game exited before the $Seconds s run finished ($exitInfo)"
  } else {
    $exitInfo = "ran $elapsed s, exited with code $($p.ExitCode)"
    if ($p.ExitCode -ne 0) { $reasons += "Game exited with code $($p.ExitCode)" }
  }
}

# Collect artifacts written since launch (packaged Saved dir sits next to the exe tree).
$searchRoot = if ($exe) { $BuildDir } else { $OutDir }
$crashDirs = Get-ChildItem -Path $searchRoot -Recurse -Directory -Filter 'Crashes' -ErrorAction SilentlyContinue
$crashes = @()
foreach ($d in $crashDirs) {
  $crashes += Get-ChildItem -Path $d.FullName -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $startedAt.AddMinutes(-1) -and $_.Name -notmatch '^(Config|Logs)$' }
}
foreach ($c in $crashes) {
  $dest = Join-Path $OutDir "Crashes\$($c.Name)"
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  Copy-Item -Path "$($c.FullName)\*" -Destination $dest -Recurse -Force -ErrorAction SilentlyContinue
}
if ($crashes.Count -gt 0) { $reasons += "$($crashes.Count) crash report(s) found" }

$csvs = Get-ChildItem -Path $searchRoot -Recurse -Filter '*.csv' -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match 'Profiling' -and $_.LastWriteTime -ge $startedAt.AddMinutes(-1) }
$csvDir = Join-Path $OutDir 'csv'
if ($csvs) { New-Item -ItemType Directory -Force -Path $csvDir | Out-Null; $csvs | Copy-Item -Destination $csvDir -Force }

# Fallback: copy any engine logs written during the run.
Get-ChildItem -Path $searchRoot -Recurse -Filter '*.log' -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match '\\Saved\\Logs\\' -and $_.LastWriteTime -ge $startedAt.AddMinutes(-1) } |
  Copy-Item -Destination $OutDir -Force -ErrorAction SilentlyContinue

# Log errors: top 10 distinct, with counts (timestamps/frame counters stripped).
$errLines = @()
Get-ChildItem -Path $OutDir -Filter '*.log' -ErrorAction SilentlyContinue | ForEach-Object {
  $errLines += Select-String -Path $_.FullName -Pattern ': (Error|Fatal):|Ensure condition failed|Assertion failed' -ErrorAction SilentlyContinue |
    ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' -replace '0x[0-9A-Fa-f]+', '0x…').Trim() }
}
$topErrors = $errLines | Group-Object | Sort-Object Count -Descending | Select-Object -First 10
if ($errLines | Where-Object { $_ -match 'Fatal:' }) { $reasons += 'Fatal log entries present' }

# FPS from CSV: prefer FrameTime (ms) column; FPS = 1000/ms.
$avg = $null; $min = $null
$csv = $csvs | Sort-Object Length -Descending | Select-Object -First 1
if ($csv) {
  try {
    $rows = Import-Csv -Path $csv.FullName
    $col = ($rows[0].PSObject.Properties.Name | Where-Object { $_ -eq 'FrameTime' } | Select-Object -First 1)
    if ($col) {
      $fps = $rows | ForEach-Object { $v = [double]$_.$col; if ($v -gt 0) { 1000.0 / $v } } | Where-Object { $_ }
      if ($fps) {
        $avg = [math]::Round(($fps | Measure-Object -Average).Average, 1)
        $min = [math]::Round(($fps | Measure-Object -Minimum).Minimum, 1)
      }
    }
  } catch { Write-Warning "CSV parse failed: $_" }
}
if ($null -eq $avg) { $reasons += 'No FPS data captured (CSV missing or no FrameTime column)' }
elseif ($avg -lt $MinFpsFloor) { $reasons += "Average FPS $avg is below floor $MinFpsFloor" }

$pass = ($reasons.Count -eq 0)
$fpsText = if ($null -ne $avg) { "avg **$avg** / min **$min**" } else { 'n/a' }
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("### Smoke test: $(if ($pass) { '✅ PASS' } else { '❌ FAIL' })")
[void]$sb.AppendLine("- Run: $exitInfo (target $Seconds s)")
[void]$sb.AppendLine("- FPS: $fpsText")
[void]$sb.AppendLine("- Crashes: $($crashes.Count)")
if ($reasons) { [void]$sb.AppendLine(); [void]$sb.AppendLine('**Why it failed**'); $reasons | ForEach-Object { [void]$sb.AppendLine("- $_") } }
[void]$sb.AppendLine(); [void]$sb.AppendLine('**Top errors**')
if ($topErrors) {
  $topErrors | ForEach-Object { $t = $_.Name; if ($t.Length -gt 300) { $t = $t.Substring(0, 300) + '…' }; [void]$sb.AppendLine("- ×$($_.Count) ``$($t -replace '``', "'")``") }
} else { [void]$sb.AppendLine('- none') }
$sb.ToString() | Set-Content -Path (Join-Path $OutDir 'summary.md') -Encoding utf8
@{ pass = $pass; avgFps = $avg; minFps = $min; crashes = $crashes.Count; reasons = $reasons } |
  ConvertTo-Json | Set-Content -Path (Join-Path $OutDir 'result.json') -Encoding utf8
# Also print the result and key game log lines into the run log (readable without downloading artifacts).
Write-Host $sb.ToString()
Get-ChildItem -Path $OutDir -Filter '*.log' -ErrorAction SilentlyContinue | ForEach-Object {
  Select-String -Path $_.FullName -Pattern 'LogHavocData|LogHavocSmoke|LogHavocMission|LogHavocWorld' -ErrorAction SilentlyContinue |
    Select-Object -First 40 | ForEach-Object { Write-Host $_.Line }
}
exit 0
