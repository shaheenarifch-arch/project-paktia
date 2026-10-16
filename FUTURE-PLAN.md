# Project Paktia Future Plan

This file lists future work only. Current, working systems are described in
`README.md` and `docs/`. Every item is grounded in what exists in the repo
today and is classified as:

- **PLANNED**: next work on an existing system or a stated vertical-slice target (`docs/vertical-slice.md`).
- **DEPENDENCY**: must land before PLANNED items that rely on it.
- **LATER**: in scope for the project but after the vertical slice.
- **OPTIONAL**: worth doing only if a need appears.

Out of scope per `docs/vertical-slice.md` (not planned): real militant
organizations or commanders, real attack reconstruction, open world,
blockchain, marketplace trading, ranked esports, jets and naval warfare,
universal destruction, console and mobile release.

## Current baseline (for reference)

- `src/`: TypeScript domain layer: drones (Raven, Warden, Talon), intelligence contacts, Aegis support workflow with protected zones, tactical map and command manager, Falcon-2 aircraft crash/recovery, Mission 01 *Operation Mountain Shield* state machine, after-action telemetry. Tests: `src/tests/`.
- `data/`: Gardun map (10 sectors), *Operation Broken Citadel* (5 Stronghold Assault phases), KR-47, Warden tank, Nomad transport, Falcon helicopter, currencies.
- `game/`: Unreal 5.5 skeleton: replicated `APaktiaCharacter` (health, Enhanced Input), replicated `APaktiaGameState` (match phase, scores), game mode, `Combat/CombatAuthority.h` interface contract.
- `workers/`: Cloudflare gateway (`/health`, `/api/v1/game/config`) and match-result queue consumer (stub ack). `services/`: economy and matchmaking contracts. `migrations/`: players, matches, token ledger, entitlements.
- `ui/`: Vite tactical-command UI prototype; optional three.js hologram avatar (not mounted).

## Gardun world expansion

- PLANNED: Gray-box all 10 `gardun-city.json` sectors in Unreal following the `docs/gardun-map.md` flow and route requirements (main, flank, interior, elevated, conditional).
- PLANNED: Implement the four `undergroundRoutes` (Mountain Fort tunnel, Old Gardun cellars, river drainage, Citadel operations) with the rule that heavy vehicles stay out of tunnels and the dense inner city.
- DEPENDENCY: Shared sector/objective ID registry between `data/`, the Unreal map, and `APaktiaGameState` phases, so validators can cross-check them.
- LATER: Mission 01 terrain (Checkpoint Echo, Ridge Station, Falcon-2 crash grid R-17) as a separate Paktia Highlands map.
- OPTIONAL: Extra Gardun districts beyond the 2.5 km x 2.5 km core.

## Mission framework

- DONE: Data-driven mission framework (`src/gameplay/missions/framework/`), used by Episodes 02 and 03 (`docs/campaign.md`).
- PLANNED: Port Mission 01 onto the framework (its hand-written state machine is unchanged today).
- PLANNED: Map the Episode 03 / *Operation Broken Citadel* phases onto `EPaktiaMatchPhase` server-side (the campaign already drives them from `operation-broken-citadel.json`).
- PLANNED: Wire `DroneTutorial`, `RidgeStation`, and `Counterattack` sub-states into mission transitions; today only `RIDGE_STATION_CAPTURED`, `COUNTERATTACK`, and the crash states have side effects.
- PLANNED: Emit Mission 01 transitions on `EventBus` (Episodes 02 and 03 already emit `mission:*` events).
- LATER: Branching outcomes (for example `abandonCrashRecovery()` affects later objectives or the debrief).

## Combined-arms gameplay

- PLANNED: Link drone reconnaissance (`DroneSensor` → `ContactManager`) to squad and vehicle tasking through `CommandManager`.
- PLANNED: Aegis support availability tied to captured objectives (pattern exists: Ridge Station → `aegis.setOnline(true)`).
- LATER: Cross-role objectives in Stronghold Assault (vehicle escort plus infantry capture).

## Infantry systems

- PLANNED: Two infantry classes per faction (vertical-slice scope) on top of `APaktiaCharacter`.
- PLANNED: Server-authoritative fire intents via `ICombatAuthority::ValidateFireIntent` / `ApplyValidatedFireIntent`.
- DEPENDENCY: Stamina, stance, and interaction input actions in Enhanced Input (only Move, Look, and Jump exist).
- LATER: Revive/medic flow (Mission 01 crew `CRITICAL`/`INJURED` conditions are a design precedent).

## Vehicles

- PLANNED: Warden tank (`component_damage`, `mobility_damage`, `repair`, `resupply`) and Nomad transport (`damage`, `repair`, `transport`) as Unreal pawns with seats from `data/vehicles/*.json`.
- DEPENDENCY: Seat-switching and enter/exit replication.
- LATER: Convoy behaviour for Mission 01 `M01_CONVOY`.

## Drones

- PLANNED: Engine adapters for `DroneController`: position integration with `DroneFlightController.step`, signal from `DroneSignalManager.calculate` against relay positions, and battery from `DroneBattery`.
- PLANNED: Use `DroneCamera` zoom/active state for the Raven optical feed shown in `ui/`.
- PLANNED: Talon `authorizeMission`/`abortMission` bound to the Aegis workflow states.
- LATER: Drone loss/recovery telemetry (`DRONE_LOSS`, `DRONE_RECOVERY` metrics already defined) feeding the after-action report.

## Helicopters

- PLANNED: Falcon helicopter (`data/aircraft/falcon-helicopter.json`: pilot, gunner, passenger; flight, damage, repair, resupply, countermeasure_gameplay) as an Unreal pawn.
- PLANNED: Drive `HelicopterDamageState` from server damage and transition `HelicopterController` flight states (the `attemptEmergencyLanding` and `recoverAircraft` paths exist but are unused).
- LATER: Player-flown Falcon in Stronghold Assault (Airfield `air_support` sector).

## Tactical command/UI

- PLANNED: Replace hard-coded values in `ui/src/main.ts` with live data from `TacticalMap`, `DroneController.list()`, and `AegisSupportController` state.
- PLANNED: Decide whether the Falcon crash demo UI in `project-paktia-helicopter-crash-build.zip` (`ui/src/main.ts`, `style.css`) gets merged into the current tactical UI as a crash-event panel; it is not merged today.
- OPTIONAL: Mount the hologram avatar (`ui/src/avatar3d.ts`) in a command-portrait slot.
- LATER: Unreal UMG implementation of the tactical map.

## Friendly/enemy AI

- PLANNED: Squad AI for Viper, Bravo, and Echo that responds to `CommandManager` actions (`MOVE`, `ORBIT`, `RETURN`, ...).
- PLANNED: Enemy counterattack waves for `Counterattack` and `M01_RECOVERY_DEFENSE`.
- LATER: Contact behaviour that makes `ContactTracker` lose and regain tracking.
- OPTIONAL: LLM-drafted radio lines and briefings, limited by the AI boundary in `docs/architecture.md` (no LLM authority over combat, rewards, or results).

## Navigation

- DEPENDENCY: Unreal NavMesh with separate areas for infantry, wheeled, and tracked vehicles (heavy vehicles excluded from inner city and tunnels).
- PLANNED: Drone flight corridors and altitude bands for `DroneFlightController`.
- LATER: Dynamic nav updates for `bridge_obstruction` and `tunnel_discovery` surprise events.

## Fictional weapon/gameplay systems

- PLANNED: KR-47 per `data/weapons/kr47.json` (fire modes, magazine, damage, recoil, animation, and audio profile IDs).
- DEPENDENCY: Damage/recoil/animation/audio profile tables that those IDs resolve to.
- PLANNED: Three later fictional primaries (vertical-slice scope).
- All weapon data remains fictional game-balance data (enforced by `scripts/validate-contracts.mjs`).

## Damage and destruction

- PLANNED: Server-side damage pipeline behind `ICombatAuthority` for characters (`ApplyAuthoritativeDamage` exists), vehicles, and aircraft.
- PLANNED: Drone damage via `DroneBase.applyDamage` thresholds (disabled ≥ 0.5, destroyed ≥ 1).
- LATER: Scripted, limited destruction at objectives (universal destruction is excluded).

## Objectives and mission sequencing

- PLANNED: Objective entities for each Broken Citadel objective ID (`capture_airfield` ... `operations_room`) with capture state replicated in `APaktiaGameState`.
- PLANNED: Server-controlled surprise events (`sandstorm`, `communications_outage`, `tunnel_discovery`, `bridge_obstruction`, `nightfall`): one per match, per vertical-slice scope.
- DEPENDENCY: Mission framework generalization (see above).

## Checkpoints/save systems

- PLANNED: Mission 01 checkpoints at mission-state boundaries (Checkpoint Echo, Ridge Station captured, evacuation complete) built from `snapshot()` data.
- DEPENDENCY: Serializable Mission 01 aircraft/drone state; Episodes 02 and 03 already save and restore checkpoints through `MissionRunner`.
- PLANNED: Tactical UI panels for Episode 02 (convoy integrity, relay link, route choice) and Episode 03 (phase objectives, reinforcements, surprise event).
- PLANNED: Gray-box the Episode 02 valley corridor (Relay Kilo shoulder, ambush narrows, drainage tunnel, bridge) as a Paktia Highlands map section.
- LATER: Persisted progression via `migrations/0001_core.sql` `players` (level, experience).

## Audio

- PLANNED: Audio sets referenced by data (`kr47_audio`) and mission radio cues keyed to mission transitions on `EventBus`.
- LATER: Dynamic music tied to match phase (`OnRep_MatchPhase` comment marks the hook).

## Animation

- PLANNED: `rifle_standard` animation set for KR-47 infantry.
- LATER: Vehicle and helicopter seat animations; crew-extraction animations for crash recovery.

## VFX

- PLANNED: Crash-site smoke/beacon (the zip demo UI shows the intended presentation), drone optical feed post-process, and Aegis support effects (abstract, non-realistic).
- LATER: Sandstorm and nightfall surprise-event visuals.

## Optimization/LOD

- DEPENDENCY: Performance budget for 16–24 players on the 2.5 km map (see Testing).
- LATER: HLOD/World Partition for Gardun sectors, vehicle and character LODs, network relevancy tuning.

## Multiplayer/network architecture

- PLANNED: Dedicated authoritative server (`ProjectPaktiaServer.Target.cs` exists), following `docs/MatchProtocol.md` (client input with sequence/frame, server snapshots with `rulesVersion`).
- PLANNED: Implement `GameServerAllocator` (`services/matchmaking/contracts.ts`) against a real hosting provider (not chosen; `MATCH_SERVER_PROVIDER` is blank in `.env.example`).
- PLANNED: Complete the match-result queue consumer in `workers/jobs/src/index.ts` (it currently only acks): load the signed result, check idempotency, update progression/economy, and write audit records.
- DEPENDENCY: Wrangler configuration for the jobs worker (`wrangler.jsonc` only defines the gateway and the queue producer) and a D1/persistent store binding for `migrations/`.
- LATER: Replicate the TypeScript domain-layer state (drones, contacts, Aegis) through Unreal replication or a server-side port.

## Unreal migration/integration planning

- DEPENDENCY: Decide how the TypeScript `src/` domain logic reaches Unreal: port it to C++ modules, or keep it as the reference spec plus tests. Engine adapters are not implemented today.
- PLANNED: Implement `ProjectPaktia::ICombatAuthority` (`game/Source/ProjectPaktia/Combat/CombatAuthority.h`) as a server-only component; it currently uses plain C++ types (`long long`, `int`, `float`) and is not a UObject/UInterface, so convert it to UE types (`int64`, `int32`) when integrated.
- PLANNED: Compile and verify the Unreal module with UE 5.5 (not verified in this repo's CI; no Unreal toolchain in CI).
- LATER: Unreal content folder (`Content/`) with LFS tracking per `.gitattributes`.

## Testing and performance targets

- PLANNED: Tests for every mission transition, drone lost-link behaviour per `fallbackBehavior`, `ContactManager.age`, and `CommandManager`.
- PLANNED: Validator cross-check that mission objective IDs and sector IDs match between `data/` files.
- PLANNED: Worker tests (gateway routes, job retry/ack) and SQL migration apply tests.
- DEPENDENCY: Target numbers for the vertical slice (server tick rate, client frame rate, match size 16–24), still to be set.
- LATER: Automated Unreal functional tests and soak tests on dedicated servers.

## Packaging and distribution

- PLANNED: Release tags and versioned game definitions per `docs/architecture.md` (GitHub as control plane).
- PLANNED: R2 asset publishing (`GAME_ASSETS` binding) and `PUBLIC_GAME_VERSION` bump process.
- LATER: Windows client and Linux dedicated-server packaging pipelines.
- OPTIONAL: Launcher and admin apps (README repository area `apps/`, not present yet).
