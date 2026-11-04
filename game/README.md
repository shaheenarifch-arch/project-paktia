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
| `Missions/` | `UPaktiaMissionSubsystem` | `src/gameplay/missions/framework/MissionRunner.ts` |
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
