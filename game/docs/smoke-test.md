# Automated smoke test and crash loop

After every successful Windows build, the `smoke-test` job in `.github/workflows/unreal-build.yml`
(`needs: build`) runs on the same self-hosted runner. It downloads the `MidnightHavoc-Win64` artifact,
launches the packaged `MidnightHavoc.exe` for 3 minutes, closes it, then reports back automatically.

Help links:
- Automation Test Framework: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine>
- CSV Profiler: <https://dev.epicgames.com/documentation/en-us/unreal-engine/csv-profiler-in-unreal-engine>
- Command-line arguments: <https://dev.epicgames.com/documentation/en-us/unreal-engine/command-line-arguments-in-unreal-engine>
- Crash Reporter: <https://dev.epicgames.com/documentation/en-us/unreal-engine/crash-reporting-in-unreal-engine>

## What it does

`game/scripts/smoke-run.ps1` launches the exe with `-log -abslog=... -csvprofile -ExecCmds="..."`
(windowed 1280x720, `-unattended -nosound`), passes `-HavocSmokeSeconds=SMOKE_SECONDS` (default 180) so the game exits by itself
(`UHavocSmokeExitSubsystem`, which flushes the CSV and log; force-kill only if it has not exited
120 s later), then:

- copies new `Crashes/*` folders and `Saved/Logs/*.log` written during the run,
- lists the **top 10 distinct errors** (`Error:`, `Fatal:`, ensure/assert lines, with counts),
- computes **avg/min FPS** from the CSV `FrameTime` column,
- writes `summary.md` and `result.json`.

Outputs: the `smoke-test` artifact (logs, crashes, CSV), the Actions job summary, and a comment on the
triggering PR (open PR for the branch) or, if none, on the issue number in a `bhaddru/<issue>-...` branch name.

## Pass / fail rules

Fail if any of: exe not found, game exits before the 3 minutes end, a crash report appears, a `Fatal`
log line appears, no FPS data is captured, or avg FPS is below 15 (`-MinFpsFloor`).
A failing comment lists the reasons first, then the top errors.

## Customising the walkthrough

- Console commands: change `-ExecCmds` (default `stat fps`) in the script call, `;`-separated.
  For a scripted walkthrough, add a console command or Automation test that drives the player and pass it here.
- Map: pass `-Map /Game/Maps/<Name>` to start in a specific level (default: project default map).
- Duration: set `SMOKE_SECONDS` in the workflow `env`.

## Running locally

```powershell
./game/scripts/smoke-run.ps1 -BuildDir C:\path\to\out\Windows -OutDir C:\temp\smoke -Seconds 60
```

## Known limits

Assumptions that need confirming on the runner (first run): packaged `Saved/` location, CSV output
path under `Profiling`, and that `-HavocSmokeSeconds` quits the game cleanly (closing the window cannot work with `-RenderOffscreen`). See the PR description.
