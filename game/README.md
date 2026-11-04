# Project Paktia – Unreal Engine project

Unreal Engine 5.5, single C++ runtime module `ProjectPaktia`. The dedicated server
owns all gameplay authority (see `Source/ProjectPaktia/Combat/CombatAuthority.h`).

## Open and compile (Windows, UE 5.5)

1. Install Unreal Engine 5.5 via the Epic Games Launcher:
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/installing-unreal-engine>
2. Install Visual Studio with the Unreal workloads:
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine>
3. Right-click `ProjectPaktia.uproject` → **Generate Visual Studio project files**
   (details: <https://dev.epicgames.com/documentation/en-us/unreal-engine/generating-unreal-engine-project-files-for-your-ide>).
4. Open the generated `ProjectPaktia.sln`, select **Development Editor** / **Win64**,
   and build the `ProjectPaktia` target. Or double-click the `.uproject` and let the
   editor compile when prompted.

The server target is `ProjectPaktiaServer`.

## Folder → TypeScript source

Folders are under `Source/ProjectPaktia/`.

| Folder | Class | TypeScript / data source |
| --- | --- | --- |
| `Core/` | `UPaktiaEventSubsystem` | `src/core/EventBus.ts` |
| `Missions/` | `UPaktiaMissionSubsystem`, `UPaktiaMissionRunner`, `IPaktiaMissionSystems` | `src/gameplay/missions/framework/MissionRunner.ts` |
| `Missions/Episode04/` | `UPaktiaOpenStraitMission`, `UPaktiaOilTerminal`, `UPaktiaShippingLanes`, `UPaktiaTankerEscort` | `src/gameplay/missions/episode04/` (OpenStrait, OilTerminal, ShippingLanes, TankerEscort) |
| `Tests/` | Automation tests (`ProjectPaktia.Missions.*`) | `src/tests/episode04.test.ts`, runner cases in `src/tests/episodes.test.ts` |
| `Data/` | `UPaktiaDataSubsystem`, `FPaktia*Def` structs | `data/` (missions, maps, aircraft, vehicles, weapons) |
| `Drones/` | `APaktiaDroneBase` | `src/gameplay/drones/DroneBase.ts` |
| `Aircraft/` | `APaktiaHelicopterBase` | `src/gameplay/aircraft/HelicopterController.ts` (Falcon) |
| `Vehicles/` | `APaktiaVehicleBase` | `data/vehicles/` (Warden, Nomad) |
| `TacticalUI/` | `UPaktiaTacticalHUDWidget` | `src/gameplay/command/TacticalMap.ts` |
| `AI/` | `APaktiaAIController` | none yet |
| `Audio/` | `UPaktiaAudioSubsystem` | none yet |
| `Characters/`, `Combat/`, `Networking/` | existing | – |

## Game data

The game definitions live in the repo's `data/` folder, the same files the TypeScript
layer reads. They are the single source of truth and are never copied into `.uasset`
files. `UPaktiaDataSubsystem` (a game instance subsystem in `Source/ProjectPaktia/Data/`)
loads every JSON file under `missions/`, `maps/`, `aircraft/`, `vehicles/` and `weapons/`
at startup using `FJsonObjectConverter`
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/JsonUtilities/FJsonObjectConverter>;
JSON overview: <https://dev.epicgames.com/documentation/en-us/unreal-engine/json-in-unreal-engine>).
A file that fails to parse is logged as a warning (`LogPaktiaData`) and skipped.
Missions whose `fictional` flag does not match their `setting`, or whose map's `setting`
differs, are logged as errors and not registered (same rule as `scripts/validate-contracts.mjs`).

**Data path.** Default is `<ProjectDir>/../data`. To override it, add this to
`Config/DefaultGame.ini` (create the file if needed); relative paths are taken from the project directory:

```ini
[/Script/ProjectPaktia.PaktiaDataSubsystem]
DataDirectory=D:/path/to/data
```

**Adding a mission.** Add a JSON file to `data/missions/` (see
`episode-02-silent-corridor.json`). Keep `fictional` consistent with `setting`
(default `fictional`; `real_world_hypothetical` needs `fictional: false`) and make sure
`map` names a file in `data/maps/` with the same setting. Optional fields may be omitted. If you add a new
field, add the matching property to `FPaktiaMissionDef` (JSON key `failState` maps to
property `FailState`; booleans such as `fictional` are read by hand in
`PaktiaDataSubsystem.cpp`). Then run `npm run validate`. Look missions up with
`UPaktiaDataSubsystem::GetMission` or `UPaktiaMissionSubsystem::GetMissionDef`.

**Packaged builds.** Add `../data` under Project Settings > Packaging > **Additional
Non-Asset Directories to Package**
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects>),
or set `DataDirectory` to wherever the files are shipped.

## Missions

`UPaktiaMissionSubsystem::StartMission(FName)` builds a `UPaktiaMissionRunner` from the
`FPaktiaMissionDef` loaded by `UPaktiaDataSubsystem`. The runner follows `next`, named
`branches`, `requires` flags and `failState`, runs `effects` through registered handlers
(startup error if one is missing), and records checkpoints (state, flags, history and a JSON
snapshot of the mission's systems via `IPaktiaMissionSystems`). Transitions are published on
`UPaktiaEventSubsystem` as `mission:state`, `mission:blocked`, `mission:checkpoint` and
`mission:complete` with the same JSON fields as `MissionEvents` in the TypeScript, and on native
delegates (`OnState`, `OnBlocked`, `OnCheckpoint`, `OnComplete`) for C++ listeners.

Episode 04 (`episode_04_open_strait`) starts as a `UPaktiaOpenStraitMission`
(`UPaktiaMissionSubsystem::GetOpenStrait()`). Call `SetEpisode3Outcome("SUCCESS")` before
`StartMission` for the +10 reinforcements. Gameplay actors drive it through its BlueprintCallable
functions (`RevealHazards`, `ClearHazard`, `DamageTerminal`, `DamageTanker`, `SpendReinforcements`, ...).
Branch order in `GetChoices()` relies on `TMap` keeping JSON insertion order.

## Running tests

The tests in `Source/ProjectPaktia/Tests/` are Unreal Automation Tests
(`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, built when `WITH_DEV_AUTOMATION_TESTS` is on, i.e. Development Editor).
They read `data/missions/episode-04-open-strait.json` from `<ProjectDir>/../data`.

- In the editor: **Tools > Session Frontend > Automation**, tick `ProjectPaktia.Missions`, then **Start Tests**.
  Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine>
  and <https://dev.epicgames.com/documentation/en-us/unreal-engine/session-frontend-in-unreal-engine>.
- From the command line (headless):
  `UnrealEditor-Cmd.exe ProjectPaktia.uproject -ExecCmds="Automation RunTests ProjectPaktia.Missions; Quit" -unattended -nullrhi -log`
  (see <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>).

The TypeScript suite (`npm test`) remains the reference behaviour.
