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
  [string]$Episode = '04',                   # playable episode to start (game/scripts/playable-episodes.json)
  [string]$Map = '',                         # optional map to open, e.g. /Game/Maps/Kharg
  [string]$ExecCmds = 'stat fps',            # extra console commands for the walkthrough, ';' separated
  [double]$MinFpsFloor = 15,
  [double]$MinScreenshotSpread = 8           # luminance std dev (0-255); below this the in-game view is one flat colour
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
             '-nosplash', '-unattended', '-nosound', "-ExecCmds=$ExecCmds", '-HavocSmokeStart', "-HavocSmokeEpisode=$Episode")
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $args -PassThru
  $exited = $p.WaitForExit($Seconds * 1000)
  if ($exited) {
    $exitInfo = "exited early with code $($p.ExitCode) after $([int]((Get-Date) - $startedAt).TotalSeconds)s"
    $reasons += "Game exited before the $Seconds s run finished ($exitInfo)"
  } else {
    # Graceful close so the CSV profile gets flushed; force-kill as fallback.
    [void]$p.CloseMainWindow()
    if (-not $p.WaitForExit(30000)) { $p.Kill(); $p.WaitForExit(); $reasons += 'Game did not close gracefully (killed); CSV may be missing' }
    $exitInfo = "ran $Seconds s, closed"
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

# Cutscenes: the game logs "HavocCutscene: <episode> intro|outro started|finished|skipped" (Cutscene/HavocCutscene.cpp).
# Both must have started and ended, or the episode has no intro/outro (or it hung).
$cutscenes = @{}
foreach ($kind in 'intro', 'outro') {
  $started = $false; $ended = $false; $missing = $false
  Get-ChildItem -Path $OutDir -Filter '*.log' -ErrorAction SilentlyContinue | ForEach-Object {
    $hits = Select-String -Path $_.FullName -SimpleMatch "HavocCutscene: $Episode $kind " -ErrorAction SilentlyContinue
    foreach ($h in $hits) {
      if ($h.Line -match "$Episode $kind started") { $started = $true }
      elseif ($h.Line -match "$Episode $kind (finished|skipped)") { $ended = $true }
      elseif ($h.Line -match "$Episode $kind missing") { $missing = $true }
    }
  }
  $cutscenes[$kind] = if ($started -and $ended) { 'ok' } elseif ($missing -or -not $started) { 'missing' } else { 'did not finish' }
  if ($cutscenes[$kind] -ne 'ok') { $reasons += "Cutscene: Episode $Episode $kind $($cutscenes[$kind])" }
}

# World visible? -HavocSmokeStart starts Episode 04 and takes a HighResShot 30 s in (HavocPlayerController).
# A flat single-colour picture means the level loaded without its terrain (yellow/blue screen, Nov 2026).
# Help: https://learn.microsoft.com/en-us/dotnet/api/system.drawing.bitmap
$shotSpread = $null
$shot = Get-ChildItem -Path $searchRoot -Recurse -Filter '*.png' -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match '\\Screenshots\\' -and $_.LastWriteTime -ge $startedAt.AddMinutes(-1) } |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $shot) {
  $reasons += 'No in-game screenshot was taken (the episode did not start or HighResShot failed)'
} else {
  Copy-Item -Path $shot.FullName -Destination (Join-Path $OutDir 'ingame.png') -Force
  try {
    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::FromFile($shot.FullName)
    $lum = @()
    # 40 x 40 grid over the middle 80% of the picture (skips most of the HUD text at the edges).
    for ($i = 0; $i -lt 40; $i++) { for ($j = 0; $j -lt 40; $j++) {
      $px = $bmp.GetPixel([int]($bmp.Width * (0.1 + 0.8 * $i / 39)), [int]($bmp.Height * (0.1 + 0.8 * $j / 39)))
      $lum += 0.2126 * $px.R + 0.7152 * $px.G + 0.0722 * $px.B
    } }
    $bmp.Dispose()
    $mean = ($lum | Measure-Object -Average).Average
    $shotSpread = [math]::Round([math]::Sqrt((($lum | ForEach-Object { ($_ - $mean) * ($_ - $mean) }) | Measure-Object -Average).Average), 1)
    if ($shotSpread -lt $MinScreenshotSpread) {
      $reasons += "In-game view is one flat colour (spread $shotSpread < $MinScreenshotSpread): the world is not rendering. See ingame.png"
    }
  } catch { $reasons += "Could not read the in-game screenshot: $_" }
}

$pass = ($reasons.Count -eq 0)
$fpsText = if ($null -ne $avg) { "avg **$avg** / min **$min**" } else { 'n/a' }
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("### Smoke test, Episode ${Episode}: $(if ($pass) { '✅ PASS' } else { '❌ FAIL' })")
[void]$sb.AppendLine("- Run: $exitInfo (target $Seconds s)")
[void]$sb.AppendLine("- FPS: $fpsText")
[void]$sb.AppendLine("- Crashes: $($crashes.Count)")
[void]$sb.AppendLine("- Cutscenes: intro $($cutscenes['intro']), outro $($cutscenes['outro'])")
[void]$sb.AppendLine("- World visible: $(if ($null -eq $shotSpread) { 'n/a (no screenshot)' } elseif ($shotSpread -ge $MinScreenshotSpread) { "yes (spread $shotSpread)" } else { "NO, flat colour (spread $shotSpread)" })")
if ($reasons) { [void]$sb.AppendLine(); [void]$sb.AppendLine('**Why it failed**'); $reasons | ForEach-Object { [void]$sb.AppendLine("- $_") } }
[void]$sb.AppendLine(); [void]$sb.AppendLine('**Top errors**')
if ($topErrors) {
  $topErrors | ForEach-Object { $t = $_.Name; if ($t.Length -gt 300) { $t = $t.Substring(0, 300) + '…' }; [void]$sb.AppendLine("- ×$($_.Count) ``$($t -replace '``', "'")``") }
} else { [void]$sb.AppendLine('- none') }
$sb.ToString() | Set-Content -Path (Join-Path $OutDir 'summary.md') -Encoding utf8
@{ pass = $pass; episode = $Episode; introCutscene = $cutscenes['intro']; outroCutscene = $cutscenes['outro']; avgFps = $avg; minFps = $min; crashes = $crashes.Count; screenshotSpread = $shotSpread; reasons = $reasons } |
  ConvertTo-Json | Set-Content -Path (Join-Path $OutDir 'result.json') -Encoding utf8
exit 0
