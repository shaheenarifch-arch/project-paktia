<#
  Runs smoke-run.ps1 once per playable episode (game/scripts/playable-episodes.json), one after the other
  (one runner, never two Unreal processes at once), and writes one combined report:
    <OutDir>\summary.md   a table with one row per episode, then each episode's details
    <OutDir>\result.json  { pass, episodes: [ { episode, pass, ... } ], reasons: [ "[04] ..." ] }
    <OutDir>\ep-<id>\...  each episode's own logs, screenshot and summary
  Used by the "smoke-test" job in .github/workflows/unreal-build.yml. See game/docs/smoke-test.md.
  An episode that fails or whose run produced no result still gets a row (FAIL), never silently dropped.
#>
param(
  [Parameter(Mandatory)][string]$BuildDir,
  [Parameter(Mandatory)][string]$OutDir,
  [int]$Seconds = 90,                        # per episode; the intro, screenshot (30 s) and outro (35 s) fit in 60 s
  [string]$EpisodesFile = (Join-Path $PSScriptRoot 'playable-episodes.json')
)
$ErrorActionPreference = 'Continue'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$episodes = @((Get-Content $EpisodesFile -Raw | ConvertFrom-Json).episodes)
$results = @(); $allReasons = @(); $details = ''
foreach ($ep in $episodes) {
  $epOut = Join-Path $OutDir "ep-$($ep.id)"
  $map = if ($ep.map) { $ep.map } else { '' }
  & (Join-Path $PSScriptRoot 'smoke-run.ps1') -BuildDir $BuildDir -OutDir $epOut -Seconds $Seconds -Episode $ep.id -Map $map
  $rf = Join-Path $epOut 'result.json'
  if (Test-Path $rf) {
    $r = Get-Content $rf -Raw | ConvertFrom-Json
  } else {
    $r = [pscustomobject]@{ pass = $false; episode = $ep.id; introCutscene = 'n/a'; outroCutscene = 'n/a'; avgFps = $null; crashes = 0
                            screenshotSpread = $null; reasons = @('Smoke script produced no result') }
  }
  $results += [pscustomobject]@{ id = $ep.id; name = $ep.name; r = $r }
  foreach ($why in @($r.reasons)) { if ($why) { $allReasons += "[$($ep.id)] $why" } }
  $sf = Join-Path $epOut 'summary.md'
  if (Test-Path $sf) { $details += "`n" + (Get-Content $sf -Raw) }
}

$ok = @($results | Where-Object { $_.r.pass }).Count
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("### Smoke test per episode: $ok of $($results.Count) passed")
[void]$sb.AppendLine()
[void]$sb.AppendLine('| Episode | Result | Intro | Outro | World visible | Avg FPS | Crashes |')
[void]$sb.AppendLine('|---|---|---|---|---|---|---|')
foreach ($e in $results) {
  $r = $e.r
  $visible = if ($null -eq $r.screenshotSpread) { 'n/a' } else { "spread $($r.screenshotSpread)" }
  $fps = if ($null -ne $r.avgFps) { $r.avgFps } else { 'n/a' }
  [void]$sb.AppendLine("| $($e.id) $($e.name) | $(if ($r.pass) { '✅ PASS' } else { '❌ FAIL' }) | $($r.introCutscene) | $($r.outroCutscene) | $visible | $fps | $($r.crashes) |")
}
[void]$sb.AppendLine($details)
$sb.ToString() | Set-Content -Path (Join-Path $OutDir 'summary.md') -Encoding utf8
@{ pass = ($ok -eq $results.Count); episodes = @($results | ForEach-Object { $_.r }); reasons = $allReasons } |
  ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir 'result.json') -Encoding utf8
exit 0
