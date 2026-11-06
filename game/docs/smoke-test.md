# Automated smoke test and crash loop

After every successful Windows build, the `smoke-test` job in `.github/workflows/unreal-build.yml`
(`needs: build`) runs on the same self-hosted runner. It downloads the `MidnightHavoc-Win64` artifact,
launches the packaged `MidnightHavoc.exe` once per playable episode (one after the other, 90 s each), closes it, then reports back
automatically with one row per episode.

Help links:
- Automation Test Framework: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine>
- CSV Profiler: <https://dev.epicgames.com/documentation/en-us/unreal-engine/csv-profiler-in-unreal-engine>
- Command-line arguments: <https://dev.epicgames.com/documentation/en-us/unreal-engine/command-line-arguments-in-unreal-engine>
- Crash Reporter: <https://dev.epicgames.com/documentation/en-us/unreal-engine/crash-reporting-in-unreal-engine>

## Per-episode results

`game/scripts/smoke-episodes.ps1` reads `game/scripts/playable-episodes.json` (the list of playable episodes; today only
`04`) and calls `smoke-run.ps1 -Episode <id>` for each one in turn (never two Unreal processes at once). The game starts that
episode with `-HavocSmokeEpisode=<id>`. The summary opens with a table (episode, PASS/FAIL, intro, outro, world visible,
FPS, crashes); each episode's logs, `ingame.png` and `summary.md` are under `ep-<id>/` in the `smoke-test` artifact.
**To add an episode:** add it to `playable-episodes.json` and give it an intro and outro cutscene
(`Source/MidnightHavoc/Cutscene`, see `game/README.md` "Intro and outro cutscenes"); until both play, its row fails.

## What it does

`game/scripts/smoke-run.ps1` launches the exe with `-log -abslog=... -csvprofile -ExecCmds="..."`
(windowed 1280x720, `-unattended -nosound`), waits `SMOKE_SECONDS` (default 180), closes the window
gracefully (so the CSV is flushed; force-kill after 30 s), then:

- copies new `Crashes/*` folders and `Saved/Logs/*.log` written during the run,
- lists the **top 10 distinct errors** (`Error:`, `Fatal:`, ensure/assert lines, with counts),
- computes **avg/min FPS** from the CSV `FrameTime` column,
- passes `-HavocSmokeStart`, so the game starts Episode 04 by itself and frames the view over Kharg (looking at the oil terminal) and takes a `HighResShot` at 30 s;
  the script measures how varied that picture is (`ingame.png` in the artifact) to check the world renders,
- reads the log for `HavocCutscene: <episode> intro|outro started|finished|skipped`; the intro plays when the episode starts,
  the outro is triggered 35 s in, and both must start and end,
- writes `summary.md` and `result.json`.

Outputs: the `smoke-test` artifact (logs, crashes, CSV), the Actions job summary, and a comment on the
triggering PR (open PR for the branch) or, if none, on the issue number in a `bhaddru/<issue>-...` branch name.

## Pass / fail rules

Fail if any of: exe not found, game exits before the 3 minutes end, a crash report appears, a `Fatal`
log line appears, no FPS data is captured, or avg FPS is below 15 (`-MinFpsFloor`), or the in-game screenshot is missing or one flat colour
(luminance spread below 8, `-MinScreenshotSpread`), or the intro or outro did not start and end. Only the screenshot and
cutscene checks fail the Actions job ("Fail if an episode world is not visible or a cutscene is missing"); the other rules show
in the summary and PR comment. Any episode failing those checks fails the job, and the table shows which one.
A failing comment lists the reasons first, then the top errors.

## Customising the walkthrough

- Console commands: change `-ExecCmds` (default `stat fps`) in the script call, `;`-separated.
  For a scripted walkthrough, add a console command or Automation test that drives the player and pass it here.
- Map: pass `-Map /Game/Maps/<Name>` to start in a specific level (default: project default map).
- Duration: set `SMOKE_SECONDS` (per episode, default 90; the outro runs at 35 s and ends by about 46 s) in the workflow `env`.

## Running locally

```powershell
./game/scripts/smoke-episodes.ps1 -BuildDir C:\path\to\out\Windows -OutDir C:\temp\smoke -Seconds 60
# or a single episode:
./game/scripts/smoke-run.ps1 -BuildDir C:\path\to\out\Windows -OutDir C:\temp\smoke -Seconds 60 -Episode 04
```

## Known limits

Assumptions that need confirming on the runner (first run): packaged `Saved/` location, CSV output
path under `Profiling`, and that `CloseMainWindow` quits the game cleanly. The cutscene lines are read from the game log, so
`-abslog` must be writable. Screenshots are picked by write time, so keep episodes at least a minute apart (the default 90 s does). See the PR description.
