<#
Optional: build the Kharg level headlessly (Windows, UE 5.5).
  .\game\scripts\run-build-kharg-level.ps1 [-EngineDir "C:\Program Files\Epic Games\UE_5.5"]
Needs: Python Editor Script Plugin (enabled in the .uproject), the C++ project built once, and the heightmap
(see game/docs/kharg-level-guide.md). Exit code is non-zero if the script fails.
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python
      https://dev.epicgames.com/documentation/en-us/unreal-engine/command-line-arguments-in-unreal-engine
#>
param([string]$EngineDir = $(if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { "C:\Program Files\Epic Games\UE_5.5" }))
$game = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $exe)) { Write-Error "UnrealEditor-Cmd.exe not found at $exe. Pass -EngineDir."; exit 2 }
& $exe (Join-Path $game "MidnightHavoc.uproject") `
    "-ExecutePythonScript=$(Join-Path $game 'Content\Python\build_kharg_level.py')" `
    -unattended -nop4 -nosplash -log
exit $LASTEXITCODE
