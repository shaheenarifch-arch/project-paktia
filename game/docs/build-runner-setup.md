# Build runner setup (Windows PC -> automatic Unreal build)

Every push to `main` or a `bhaddru/*` branch runs `.github/workflows/unreal-build.yml` on **your PC**:
it compiles the UE 5.5 project, cooks and packages a Win64 build, runs the automation tests and uploads
`MidnightHavoc-Win64.zip` plus logs. Compile errors then show up in the Actions log for Bhaddru.

Help links:
- Registering a runner: <https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners>
- Packaging UE projects: <https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects>

## One-time setup

1. **Install Unreal Engine 5.5** via the Epic Games Launcher (default `C:\Program Files\Epic Games\UE_5.5`).
2. **Check Visual Studio 2022** (Visual Studio Installer > Modify) has these workloads/components:
   - *Game development with C++*
   - *.NET desktop development*
   - Windows 10/11 SDK and MSVC v143 build tools (and .NET Framework 4.8 targeting pack)
   See <https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cpp-projects-in-unreal-engine>.
3. **Register the runner**: GitHub repo > Settings > Actions > Runners > **New self-hosted runner** > Windows.
   Run the shown download and `config.cmd` commands in an admin PowerShell, e.g. in `C:\actions-runner`.
   - When asked for extra labels enter: `windows,unreal` (the `self-hosted` label is added automatically).
   - When asked "Run runner as a service?" answer **Y** (so it survives reboots and logouts).
     Or later: `.\svc.cmd install` then `.\svc.cmd start`.
4. **Set `UE_ROOT`** (only if UE is not in the default location): repo > Settings > Secrets and variables >
   Actions > **Variables** > New repository variable `UE_ROOT` = e.g. `D:\UE\UE_5.5`.
   Default if unset: `C:\Program Files\Epic Games\UE_5.5`.
5. **Test it**: repo > Actions > *Unreal build (Win64)* > **Run workflow**. The first run is slow (shader/DDC warm-up).

Keep the PC on and the runner service running; builds queue while it is offline.

## Download and play the build

1. Repo > Actions > open the finished run > **Artifacts** > download `MidnightHavoc-Win64`.
2. Unzip it, then run `MidnightHavoc.exe` (inside the unzipped folder).
3. The `data/` JSON is packaged next to the game folder (`DirectoriesToAlwaysStageAsNonUFS` in
   `game/Config/DefaultGame.ini`); keep the unzipped folder structure intact.
4. `unreal-logs` holds build/test logs and the automation report if something fails.

## Troubleshooting

- Job stays "Waiting for a runner": runner offline or labels not exactly `windows` and `unreal`.
- "RunUAT.bat not found": fix `UE_ROOT`.
- Missing MSVC/SDK errors: re-check step 2.
- Packaged game cannot find data: confirm `data` sits beside the project folder in the zip.
- Security: self-hosted runners execute workflow code from this repo on your PC; keep the repo private
  or require approval for outside contributors' workflows.
