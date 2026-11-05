# Operation Midnight Havoc – Unreal Engine project

Unreal Engine 5.5, single C++ runtime module `MidnightHavoc`. The dedicated server
owns all gameplay authority (see `Source/MidnightHavoc/Combat/CombatAuthority.h`).

## Open and compile (Windows, UE 5.5)

1. Install Unreal Engine 5.5 via the Epic Games Launcher:
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/installing-unreal-engine>
2. Install Visual Studio with the Unreal workloads:
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine>
3. Right-click `MidnightHavoc.uproject` → **Generate Visual Studio project files**
   (details: <https://dev.epicgames.com/documentation/en-us/unreal-engine/generating-unreal-engine-project-files-for-your-ide>).
4. Open the generated `MidnightHavoc.sln`, select **Development Editor** / **Win64**,
   and build the `MidnightHavoc` target. Or double-click the `.uproject` and let the
   editor compile when prompted.

The server target is `MidnightHavocServer`.

## Folder → TypeScript source

Folders are under `Source/MidnightHavoc/`.

| Folder | Class | TypeScript / data source |
| --- | --- | --- |
| `Core/` | `UHavocEventSubsystem` | `src/core/EventBus.ts` |
| `Missions/` | `UHavocMissionSubsystem`, `UHavocMissionRunner`, `IHavocMissionSystems` | `src/gameplay/missions/framework/MissionRunner.ts` |
| `Missions/Episode04/` | `UHavocOpenStraitMission`, `UHavocOilTerminal`, `UHavocShippingLanes`, `UHavocTankerEscort` | `src/gameplay/missions/episode04/` (OpenStrait, OilTerminal, ShippingLanes, TankerEscort) |
| `Tests/` | Automation tests (`MidnightHavoc.Missions.*`, `MidnightHavoc.Aircraft.*`, `MidnightHavoc.Drones.*`) | `src/tests/episode04.test.ts`, `src/tests/episodes.test.ts`, `src/tests/system.test.ts`, `src/tests/helicopter-crash.test.ts` |
| `Data/` | `UHavocDataSubsystem`, `FHavoc*Def` structs | `data/` (missions, maps, aircraft, vehicles, weapons) |
| `Drones/` | `AHavocDrone` (Raven, Warden, Talon subclasses), `IHavocDroneDetectable` | `src/gameplay/drones/` (DroneBase, DroneFlightController, DroneBattery, DroneSignalManager, DroneSensor, DroneCamera) |
| `Aircraft/` | `AHavocFalconHelicopter`, `UHavocCrashSequence` | `src/gameplay/aircraft/` (HelicopterController, HelicopterDamageState, CrewState, HelicopterCrashController) |
| `Vehicles/` | `AHavocVehicleBase` | `data/vehicles/` (Warden, Nomad) |
| `TacticalUI/` | `UHavocTacticalHUD` (see `docs/hud-guide.md`) | `src/gameplay/command/TacticalMap.ts` |
| `World/` | `AHavocLevelDirector`, `AHavocSectorVolume`, `AHavocTerminalPart`, `AHavocLaneHazard`, `AHavocTankerSpline` (see `docs/kharg-level-guide.md`) | `data/maps/kharg-hormuz.json`, Episode 04 rules |
| `AI/` | `AHavocAIController` | none yet |
| `Audio/` | `UHavocAudioSubsystem` | none yet |
| `Assets/` | `UHavocAssetSubsystem` (reads `Config/HavocAssets.ini`; see `docs/fab-assets.md`) | – |
| `Characters/`, `Combat/`, `Networking/` | existing | – |

## Art and audio assets (Fab / Megascans)

Real third-party assets only; they are not committed. `docs/fab-assets.md` lists the free packs
to add to the Fab library and the `Content/ThirdParty/<Pack>` target folders.
`Config/HavocAssets.ini` maps game roles (`Terrain.Sand`, `Character.Marine`, `Aircraft.Falcon`, ...)
to asset paths and `UHavocAssetSubsystem` reads it, so swapping a pack is config-only.
Help: <https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine>

## Game data

The game definitions live in the repo's `data/` folder, the same files the TypeScript
layer reads. They are the single source of truth and are never copied into `.uasset`
files. `UHavocDataSubsystem` (a game instance subsystem in `Source/MidnightHavoc/Data/`)
loads every JSON file under `missions/`, `maps/`, `aircraft/`, `vehicles/` and `weapons/`
at startup using `FJsonObjectConverter`
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/JsonUtilities/FJsonObjectConverter>;
JSON overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/json-in-unreal-engine>).
A file that fails to parse is logged as a warning (`LogHavocData`) and skipped.
Missions whose `fictional` flag does not match their `setting`, or whose map's `setting`
differs, are logged as errors and not registered (same rule as `scripts/validate-contracts.mjs`).

**Data path.** Default is `<ProjectDir>/../data`. To override it, add this to
`Config/DefaultGame.ini` (create the file if needed); relative paths are taken from the project directory:

```ini
[/Script/MidnightHavoc.HavocDataSubsystem]
DataDirectory=D:/path/to/data
```

**Adding a mission.** Add a JSON file to `data/missions/` (see
`episode-02-silent-corridor.json`). Keep `fictional` consistent with `setting`
(default `fictional`; `real_world_hypothetical` needs `fictional: false`) and make sure
`map` names a file in `data/maps/` with the same setting. Optional fields may be omitted. If you add a new
field, add the matching property to `FHavocMissionDef` (JSON key `failState` maps to
property `FailState`; booleans such as `fictional` are read by hand in
`HavocDataSubsystem.cpp`). Then run `npm run validate`. Look missions up with
`UHavocDataSubsystem::GetMission` or `UHavocMissionSubsystem::GetMissionDef`.

**Packaged builds.** Add `../data` under Project Settings > Packaging > **Additional
Non-Asset Directories to Package**
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects>),
or set `DataDirectory` to wherever the files are shipped.

## Missions

`UHavocMissionSubsystem::StartMission(FName)` builds a `UHavocMissionRunner` from the
`FHavocMissionDef` loaded by `UHavocDataSubsystem`. The runner follows `next`, named
`branches`, `requires` flags and `failState`, runs `effects` through registered handlers
(startup error if one is missing), and records checkpoints (state, flags, history and a JSON
snapshot of the mission's systems via `IHavocMissionSystems`). Transitions are published on
`UHavocEventSubsystem` as `mission:state`, `mission:blocked`, `mission:checkpoint` and
`mission:complete` with the same JSON fields as `MissionEvents` in the TypeScript, and on native
delegates (`OnState`, `OnBlocked`, `OnCheckpoint`, `OnComplete`) for C++ listeners.

Episode 04 (`episode_04_open_strait`) starts as a `UHavocOpenStraitMission`
(`UHavocMissionSubsystem::GetOpenStrait()`). Call `SetEpisode3Outcome("SUCCESS")` before
`StartMission` for the +10 reinforcements. Gameplay actors drive it through its BlueprintCallable
functions (`RevealHazards`, `ClearHazard`, `DamageTerminal`, `DamageTanker`, `SpendReinforcements`, ...).
Branch order in `GetChoices()` relies on `TMap` keeping JSON insertion order.

## Building the Kharg level

Run `Content/Python/build_kharg_level.py` (editor: `Tools > Execute Python Script...`, or headless with
`scripts/run-build-kharg-level.ps1`) to build `Content/Maps/Kharg/L_KhargHormuz`. Make the heightmap first with
`Content/Python/make_kharg_heightmap.py`. Full steps and the manual fallback: `docs/kharg-level-guide.md`.
Python scripting help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>

## Running tests

The tests in `Source/MidnightHavoc/Tests/` are Unreal Automation Tests
(`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, built when `WITH_DEV_AUTOMATION_TESTS` is on, i.e. Development Editor).
They read `data/missions/episode-04-open-strait.json` from `<ProjectDir>/../data`.

- In the editor: **Tools > Session Frontend > Automation**, tick `MidnightHavoc` (or just `MidnightHavoc.Missions`, `MidnightHavoc.Aircraft` or `MidnightHavoc.Drones`), then **Start Tests**.
  Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine>
  and <https://dev.epicgames.com/documentation/en-us/unreal-engine/session-frontend-in-unreal-engine>.
- From the command line (headless):
  `UnrealEditor-Cmd.exe MidnightHavoc.uproject -ExecCmds="Automation RunTests MidnightHavoc; Quit" -unattended -nullrhi -log`
  (see <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>).

The TypeScript suite (`npm test`) remains the reference behaviour.
