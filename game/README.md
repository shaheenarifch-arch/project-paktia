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
| `Campaign/` | `UHavocCampaign` (GameInstance subsystem: unlock order, best-result rule, carryover, `USaveGame` slot `HavocCampaign`) | `src/gameplay/campaign/Campaign.ts` |
| `Missions/` | `UHavocMissionSubsystem`, `UHavocMissionRunner`, `IHavocMissionSystems` | `src/gameplay/missions/framework/MissionRunner.ts` |
| `Missions/Episode01/` | `UHavocMountainShieldMission` (tutorial, Ridge Station, counterattack, Falcon-2 crash, crew recover/abandon; Aegis is a flag for now) | `src/gameplay/missions/mission01/` |
| `Missions/Episode04/` | `UHavocOpenStraitMission`, `UHavocOilTerminal`, `UHavocShippingLanes`, `UHavocTankerEscort` | `src/gameplay/missions/episode04/` (OpenStrait, OilTerminal, ShippingLanes, TankerEscort) |
| `Tests/` | Automation tests (`MidnightHavoc.Missions.*`, `MidnightHavoc.Aircraft.*`, `MidnightHavoc.Drones.*`, `MidnightHavoc.Playable.*`, `MidnightHavoc.Campaign.*`, `MidnightHavoc.Vehicles.*`) | `src/tests/episode04.test.ts`, `src/tests/episodes.test.ts`, `src/tests/system.test.ts`, `src/tests/helicopter-crash.test.ts` |
| `Data/` | `UHavocDataSubsystem`, `FHavoc*Def` structs | `data/` (missions, maps, aircraft, vehicles, weapons) |
| `Drones/` | `AHavocDrone` (Raven, Warden, Talon subclasses), `IHavocDroneDetectable` | `src/gameplay/drones/` (DroneBase, DroneFlightController, DroneBattery, DroneSignalManager, DroneSensor, DroneCamera) |
| `Aircraft/` | `AHavocFalconHelicopter`, `UHavocCrashSequence` | `src/gameplay/aircraft/` (HelicopterController, HelicopterDamageState, CrewState, HelicopterCrashController) |
| `Vehicles/` | `AHavocVehicleBase`, `AHavocBoatVehicle`, `AHavocGroundVehicle` (Warden), `AHavocNomadVehicle`, `AHavocHelicopterVehicle`, `AHavocDroneVehicle` (see `docs/vehicles.md`) | `data/vehicles/` (stats, seats) |
| `TacticalUI/` | `UHavocTacticalHUD` (see `docs/hud-guide.md`) | `src/gameplay/command/TacticalMap.ts` |
| `World/` | `AHavocLevelDirector`, `AHavocSectorVolume`, `AHavocTerminalPart`, `AHavocLaneHazard`, `AHavocTankerSpline` (see `docs/kharg-level-guide.md`) | `data/maps/kharg-hormuz.json`, Episode 04 rules |
| `AI/` | `AHavocAIController` (perception, patrol / cover / engage / search / fallback), `AHavocEnemyCharacter` (Iran, Houthi, ISIS opposing and coalition allied units; AI-only), `UHavocSquadSubsystem` + `HavocSquadLogic.h` (squad patrol / engage / fallback), `UHavocEnvQueryTest_Cover` (EQS cover); see `docs/ai.md` | `data/units/` (game-balance ratings only), `data/ai/squad-behaviour.json` (squad tuning and phase deployments) |
| `Audio/` | `UHavocAudioSubsystem` (plays item-3 sound and Niagara roles; see `docs/audio-voice-plan.md`, `docs/vfx-plan.md`) | none yet |
| `Assets/` | `UHavocAssetSubsystem` (reads `Config/HavocAssets.ini`; see `docs/fab-assets.md`) | – |
| `Characters/`, `Networking/` | existing | – |
| `Combat/` | `UHavocWeaponComponent` (KR-47, implements `ICombatAuthority`) | `data/weapons/kr47.json` |
| `Player/` | `AHavocPlayerCharacter` (third-person Marine), `AHavocPlayerController`, `UHavocPlaySettings` | – |
| `Input/` | `UHavocPlayInput` (Enhanced Input actions and key mappings built in C++) | – |
| `UI/` | `UHavocPlayHUD`, `UHavocMenuWidget` (main, pause, settings, results; built in C++) | – |
| `Game/` | `AHavocPlayableGameMode` | – |
| `Missions/HavocEpisodeFlow.h` | `IHavocEpisodeFlow`: shared episode interface (start, pause/resume, restart from checkpoint, objective, result) used by the controller, HUD and menus; [Unreal interfaces](https://dev.epicgames.com/documentation/en-us/unreal-engine/interfaces-in-unreal-engine) | – |
| `Missions/Episode04/` (flow) | `AHavocEpisode04Flow` implements `IHavocEpisodeFlow` and drives the playable loop on top of `UHavocOpenStraitMission` | – |

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

**Packaged builds.** Before every Win64 build, the `PreBuildSteps` in `MidnightHavoc.uproject`
copy the repo's `data/` to `game/Content/HavocData/` (git-ignored; never edit it, edit `data/`).
`Config/DefaultGame.ini` stages that folder as a non-asset directory
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects>),
and the data subsystem falls back to it when `<ProjectDir>/../data` is missing. The log line
`LogHavocData: Loading game data from '...'` shows which folder was used. CI fails the build if the
package has no `HavocData`.

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
**Campaign**, **How to play**, **Settings**, **Quit**. Esc pauses (Resume, Restart from last checkpoint, How to play, Settings, Quit). **How to play** lists every key from your current bindings. The first time you fly the Raven, drive a vehicle or fly the Falcon, a hint shows once (H or gamepad X dismisses it). The results screen
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
| H | Dismiss a first-time hint |
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
**Intro and outro cutscenes.** Starting the episode plays a 15 s fly-over (north beach, oil terminal, airstrip) with subtitles before
the briefing hands over to the Raven recon; finishing it plays a 10 s outro (oil terminal, north beach) matching the result, then the
results screen. Press **Space**, **Enter** or the gamepad bottom button to skip. Subtitles follow the Subtitles setting. They are
greybox, driven from code (`Cutscene/`), so they need no assets. To replace one with a hand-made Level Sequence, build it in Sequencer
(<https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematics-and-movie-making-in-unreal-engine>), play it from a Blueprint
child of `AHavocEpisode04Flow` and call `SkipCutscene` / keep the `HavocCutscene: 04 intro|outro` log lines so CI still sees it.
The smoke test starts each playable episode in turn and reports per episode (`docs/smoke-test.md`).

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

## Building any level on paktia-pc (GitHub Actions)

Actions > **Build level** > Run workflow > pick `Kharg`, `PaktiaHighlands` or `Gardun` (`.github/workflows/build-level.yml`).
It makes the heightmap, builds the editor, runs the level's builder headless, commits the level through Git LFS and starts the Unreal build.
`PaktiaHighlands` runs `Content/Python/build_paktia_highlands_level.py` (heightmap: `make_paktia_heightmap.py`, sources: `Content/Maps/PaktiaHighlands/SOURCES.txt`)
and makes `Content/Maps/PaktiaHighlands/L_PaktiaHighlands`, the Episode 01 greybox: 4 sectors from `data/maps/paktia-highlands.json`,
player starts, the Falcon-2 crash site, a nav mesh. It is a plain level (not World Partition), so all actors stay loaded. `Gardun` has no builder yet.

## Running tests

The tests in `Source/MidnightHavoc/Tests/` are Unreal Automation Tests
(`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, built when `WITH_DEV_AUTOMATION_TESTS` is on, i.e. Development Editor).
They read `data/missions/episode-04-open-strait.json` from `<ProjectDir>/../data`.

- In the editor: **Tools > Session Frontend > Automation**, tick `MidnightHavoc` (or just `MidnightHavoc.Missions`, `MidnightHavoc.Aircraft`, `MidnightHavoc.Drones`, `MidnightHavoc.Playable` or `MidnightHavoc.AI`), then **Start Tests**.
  Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine>
  and <https://dev.epicgames.com/documentation/en-us/unreal-engine/session-frontend-in-unreal-engine>.
- From the command line (headless):
  `UnrealEditor-Cmd.exe MidnightHavoc.uproject -ExecCmds="Automation RunTests MidnightHavoc; Quit" -unattended -nullrhi -log`
  (see <https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine>).

The TypeScript suite (`npm test`) remains the reference behaviour.

## How to play (Episode 02, greybox)

Episode 02, *Operation Silent Corridor*, is playable from **Campaign > Start Episode 02** in the same level as Episode 04. The valley is a greybox:
`AHavocEpisode02Flow` builds a floating slab of blocks (Checkpoint Echo, Relay Kilo site, bridge, riverbed narrows, drainage mouth, South Gate)
at `GreyboxOrigin` (3 km above the Kharg level) and box vehicles for the convoy, so no extra level is needed. Replace the blocks with the real
Gardun level and vehicle meshes later; the rules come from `data/missions/episode-02-silent-corridor.json` and do not change.
Controls are the same as Episode 04 (above); **1 / 2** choose the route at the bridge, and G (Falcon) is not used in this episode.

1. **Relay Kilo.** The Raven launches from Echo. Fly it to the relay site on the valley shoulder and press **E** to drop Relay Kilo. The drop only
   counts if the drone link still reaches the site.
2. **Scout the corridor.** Fly on to the end of the corridor with a usable link (not lost or critical) to mark the roadblock.
3. **Advance.** The convoy rolls down the main road (Warden-1 leads, Nomad-1 carries the Falcon-2 crew, Nomad-2 carries supplies).
4. **Comms outage.** Every relay is jammed and the Raven cannot fly. Walk the Marine to Relay Kilo and press **E** within 25 m to restore it.
5. **Bridge choice** (checkpoint). Press **1** for the Dry Riverbed (the whole convoy goes; an ambush hits it in the narrows, repel it and keep
   Nomad-1 alive) or **2** for the drainage tunnel (infantry only: clear the far bank, then Warden-1 pushes the wreck off the bridge).
6. **South Gate** (checkpoint). Clear the staging yard; the staging area is then set up.
7. **Result.** SUCCESS if the whole convoy arrives, PARTIAL if vehicles were lost, FAILED if the crew transport (or, without crew, every Nomad)
   is destroyed (Restart from last checkpoint on the results screen). The carryover (crew delivered, Nomads left, Warden, route, relays)
   is `UHavocSilentCorridorMission::GetCarryover()` for Episode 03.

Tuning (ambush size and volley damage, garrisons, convoy speed, relay and scout radii) is on `AHavocEpisode02Flow`. Smoke test:
start with `-HavocSmokeStart -HavocEpisode=2` (see `docs/smoke-test.md`). Playtest checklist: `docs/episode-02-playtest-checklist.md`.
Enemies are AI-only `isis` / `houthi` data units; there is no navigation mesh on the greybox slab, so they move in straight lines.
Help: [Blockout meshes](https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-scene-and-blockout-meshes),
[Possessing pawns](https://dev.epicgames.com/documentation/en-us/unreal-engine/possessing-pawns-in-unreal-engine).

## How to play (Episode 07)

Episode 07, *City of Freedom*, is a hypothetical urban operation in the invented city of Parvaneh. The controls are the same as Episode 04 (above). The rules and objectives come from `data/missions/episode-07-city-of-freedom.json`.

- Link up with the three resistance squads (AI allies), then clear the Old Bazaar, hold the Courtyard Quarter, take Dome Square and the security compound.
- Civilians are in the streets. Never fire on them. Escort them to shelter.
- Soldiers who surrender are detained, never fired on.
- Any harm to a civilian or a surrendered soldier fails the current objective and lowers Hearts & Minds. Three violations fail the operation.
- Side objectives: treat patients at the field hospital and deliver aid drops to raise Hearts & Minds.
- The enemy is the regime's security forces, never the Iranian people. No real people, equipment specs or targeting data appear.

Status: the mission rules and automation test (`npm test`, `src/tests/episode07.test.ts`) are in. The Unreal level, resistance AI and civilian actors still need building and a Windows build check. Playtest checklist: `docs/episode-07-playtest-checklist.md`.

Help: [Packaging](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects), [AI in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/artificial-intelligence-in-unreal-engine), [Automation tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine).

