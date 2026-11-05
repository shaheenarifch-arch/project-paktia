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
| `Tests/` | Automation tests (`MidnightHavoc.Missions.*`, `MidnightHavoc.Aircraft.*`, `MidnightHavoc.Drones.*`, `MidnightHavoc.Playable.*`, `MidnightHavoc.Vehicles.*`) | `src/tests/episode04.test.ts`, `src/tests/episodes.test.ts`, `src/tests/system.test.ts`, `src/tests/helicopter-crash.test.ts` |
| `Data/` | `UHavocDataSubsystem`, `FHavoc*Def` structs | `data/` (missions, maps, aircraft, vehicles, weapons) |
| `Drones/` | `AHavocDrone` (Raven, Warden, Talon subclasses), `IHavocDroneDetectable` | `src/gameplay/drones/` (DroneBase, DroneFlightController, DroneBattery, DroneSignalManager, DroneSensor, DroneCamera) |
| `Aircraft/` | `AHavocFalconHelicopter`, `UHavocCrashSequence` | `src/gameplay/aircraft/` (HelicopterController, HelicopterDamageState, CrewState, HelicopterCrashController) |
| `Vehicles/` | `AHavocVehicleBase`, `AHavocBoatVehicle`, `AHavocGroundVehicle`, `AHavocHelicopterVehicle`, `AHavocDroneVehicle` (see `docs/vehicles.md`) | `data/vehicles/` (stats, seats) |
| `TacticalUI/` | `UHavocTacticalHUD` (see `docs/hud-guide.md`) | `src/gameplay/command/TacticalMap.ts` |
| `World/` | `AHavocLevelDirector`, `AHavocSectorVolume`, `AHavocTerminalPart`, `AHavocLaneHazard`, `AHavocTankerSpline` (see `docs/kharg-level-guide.md`) | `data/maps/kharg-hormuz.json`, Episode 04 rules |
| `AI/` | `AHavocAIController` (perception, patrol / cover / engage / search), `AHavocEnemyCharacter` (Iran, Houthi, ISIS units; AI-only) | `data/units/` (game-balance ratings only) |
| `Audio/` | `UHavocAudioSubsystem` (plays item-3 sound and Niagara roles; see `docs/audio-voice-plan.md`, `docs/vfx-plan.md`) | none yet |
| `Assets/` | `UHavocAssetSubsystem` (reads `Config/HavocAssets.ini`; see `docs/fab-assets.md`) | – |
| `Characters/`, `Networking/` | existing | – |
| `Combat/` | `UHavocWeaponComponent` (KR-47, implements `ICombatAuthority`) | `data/weapons/kr47.json` |
| `Player/` | `AHavocPlayerCharacter` (third-person Marine), `AHavocPlayerController`, `UHavocPlaySettings` | – |
| `Input/` | `UHavocPlayInput` (Enhanced Input actions and key mappings built in C++) | – |
| `UI/` | `UHavocPlayHUD`, `UHavocMenuWidget` (main, pause, settings, results; built in C++) | – |
| `Game/` | `AHavocPlayableGameMode` | – |
| `Missions/Episode04/` (flow) | `AHavocEpisode04Flow` drives the playable loop on top of `UHavocOpenStraitMission` | – |

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

## How to play (Episode 04)

Start the game (packaged build: `MidnightHavoc.exe`; editor: open `L_KhargHormuz` and press Play). The main menu opens first:
**Play Episode 04**, **Settings**, **Quit**. Esc pauses (Resume, Restart from last checkpoint, Settings, Quit). The results screen
shows the outcome, terminal integrity, tankers through/lost and supply flow.

| Key | Action |
| --- | --- |
| W / A / S / D | Move |
| Mouse | Look |
| Left Shift | Sprint |
| C | Crouch (toggle) |
| Right mouse | Aim |
| Left mouse | Fire the KR-47 (hold for automatic) |
| R | Reload |
| E | Interact: on foot, call a resupply drop (3 per mission); in the Raven, mark a revealed lane hazard for the clearance team |
| T | Launch or take control of the Raven drone (recon and lane clearance phases) |
| W / A / S / D, Space, Left Ctrl, mouse X | Raven: fly, climb, descend, turn |
| 1 / 2 | Choose the north / west landing beach |
| G | Call Falcon support (tanker escort, after the airstrip is secured) |
| Space, W / A / S / D, Q / E | Falcon: collective, cyclic tilt, yaw |
| Tab | Return to the Marine from the Raven or the Falcon |
| F | Get into the nearest vehicle (on foot, within ~4.5 m) or out of the one you drive; see `docs/vehicles.md` |
| Esc or P | Pause menu |

**The mission, step by step** (the rules and states come from `data/missions/episode-04-open-strait.json`):

1. **Beach recon.** The Raven launches automatically. Fly it over both beaches to mark the defences (north is closer to the terminal but
   heavier defended; the west beach is quieter). Reinforcements are lost on landing for each marked defender.
2. **Choose a beach** with 1 or 2. The Marine lands there; clear the beach defenders to secure the beachhead.
3. **Terminal and airstrip.** Clear each garrison in either order (the beach decides the order). Stray fire damages the oil terminal's
   jetties, tanks and pumps, which lowers the final result. Securing the airstrip unlocks Falcon support.
4. **Hold the oil terminal** for the timer against counter-attacks (stay inside the terminal area).
5. **Lane clearance.** The Raven launches again; fly along the inbound and outbound lanes to reveal hazards, press E to mark each one.
6. **Tanker escort.** Six tankers transit the lanes. A contact warning appears before each attack: press G to put the Falcon on station
   (fly it or press Tab and let it cover). Tankers that are hit take damage; a lost tanker lowers the supply flow.
7. **Debrief.** The result is SUCCESS, PARTIAL or FAILED. If the Marine dies you return to the last checkpoint and lose reinforcements; at
   zero reinforcements before the terminal is secured the operation fails (Restart from last checkpoint on the results screen).

Enemy squads (Iran, Houthi and an AI-only ISIS cell) are spawned from `data/units`. ISIS is never player-controlled.
Episode 04 is a `real_world_hypothetical` scenario: no real people, no real equipment specs or targeting data; Aegis stays abstract.

**What the level needs.** The level from `docs/kharg-level-guide.md` (sector volumes, terminal parts, lane hazards, tanker splines,
PlayerStarts). `Config/DefaultEngine.ini` makes `L_KhargHormuz` the default map and `AHavocPlayableGameMode` the game mode. Optional:
place a **NavMeshBoundsVolume** over the island for pathfinding (<https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-system-in-unreal-engine>);
without one the enemies walk in straight lines. Effects and audio use the item-3 roles in `Config/HavocAssets.ini`
(`Audio.Weapon.Rifle`, `FX.Muzzle`, `FX.Explosion.Large`, `FX.Smoke`, `FX.Water.Splash`, `Audio.Explosion`, `Audio.Ambient.Coast`,
`Character.Marine`, `Weapon.KR47`, `Drone.Raven`, `Aircraft.Falcon`, `Vessel.Tanker`); a role that is not imported yet is logged once
(`LogHavocAssets`) and skipped, the game still plays.

Tuning (enemy counts, hold time, tanker speed, threat timing, Falcon timing) is on `AHavocEpisode04Flow` (Details panel of a Blueprint child,
or edit the defaults in `HavocEpisode04Flow.h`). Settings are saved to `GameUserSettings.ini`.
More: playtest checklist `docs/episode-04-playtest-checklist.md`, HUD `docs/hud-guide.md`.

Epic help: [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine),
[AI Perception](https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine),
[UMG](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine),
[Packaging](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects).

## Building the Kharg level

Run `Content/Python/build_kharg_level.py` (editor: `Tools > Execute Python Script...`, or headless with
`scripts/run-build-kharg-level.ps1`) to build `Content/Maps/Kharg/L_KhargHormuz`. Make the heightmap first with
`Content/Python/make_kharg_heightmap.py`. Full steps and the manual fallback: `docs/kharg-level-guide.md`.
Python scripting help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>

## Running tests

The tests in `Source/MidnightHavoc/Tests/` are Unreal Automation Tests
(`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, built when `WITH_DEV_AUTOMATION_TESTS` is on, i.e. Development Editor).
They read `data/missions/episode-04-open-strait.json` from `<ProjectDir>/../data`.

- In the editor: **Tools > Session Frontend > Automation**, tick `MidnightHavoc` (or just `MidnightHavoc.Missions`, `MidnightHavoc.Aircraft`, `MidnightHavoc.Drones` or `MidnightHavoc.Playable`), then **Start Tests**.
  Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine>
  and <https://dev.epicgames.com/documentation/en-us/unreal-engine/session-frontend-in-unreal-engine>.
- From the command line (headless):
  `UnrealEditor-Cmd.exe MidnightHavoc.uproject -ExecCmds="Automation RunTests MidnightHavoc; Quit" -unattended -nullrhi -log`
  (see <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>).

The TypeScript suite (`npm test`) remains the reference behaviour.
