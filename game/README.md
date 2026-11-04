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
| `Drones/` | `APaktiaDroneBase` | `src/gameplay/drones/DroneBase.ts` |
| `Aircraft/` | `APaktiaHelicopterBase` | `src/gameplay/aircraft/HelicopterController.ts` (Falcon) |
| `Vehicles/` | `APaktiaVehicleBase` | `data/vehicles/` (Warden, Nomad) |
| `TacticalUI/` | `UPaktiaTacticalHUDWidget` | `src/gameplay/command/TacticalMap.ts` |
| `AI/` | `APaktiaAIController` | none yet |
| `Audio/` | `UPaktiaAudioSubsystem` | none yet |
| `Characters/`, `Combat/`, `Networking/` | existing | – |
