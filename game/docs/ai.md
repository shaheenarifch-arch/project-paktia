# AI squads (StateTree, EQS cover, NavMesh)

Allied and opposing infantry squads patrol sectors, take cover, react to the player and follow the mission
phases. Code is in `Source/MidnightHavoc/AI/`, tuning is in `data/ai/squad-behaviour.json`, tests are in
`Source/MidnightHavoc/Tests/HavocSquadTests.cpp`.

Rules that apply here as everywhere (Episode 04): abstract game balance only, no real operating or targeting
detail. **ISIS is AI-only**: a unit's team comes from its faction (`coalition` = allied; `iran`, `houthi`,
`isis` = opposing), so ISIS can never be on the player's side or be possessed.

## How it fits together

| Piece | What it does |
| --- | --- |
| `UHavocSquadSubsystem` | One per level (server only). Spawns squads, shares a threat inside a squad and ticks each squad through **Patrol → Engage → Fallback → Patrol**. Publishes `squad:mode` on the event bus. |
| `HavocSquadLogic.h` | The squad rules as plain C++ (`FSquadModel::Update`), no world needed, so the automation tests run them with a simulated clock. |
| `AHavocAIController` | The unit brain: perception, patrol, engage, cover, search and **fallback**. It follows the squad mode (`ApplySquadMode`). Units outside a squad still act alone. |
| `UHavocEnvQueryTest_Cover` + `HavocCoverEqs` | The **EQS** cover query (grid around the unit, NavMesh-projected, filtered to points the threat cannot see). If EQS finds nothing the controller falls back to its own trace search, then to pulling straight back. |
| `AHavocLevelDirector` | On every `mission:state` event it calls `ApplyPhase`, so the squads listed for that state are deployed and the previous phase's squads are removed. |
| `AHavocEpisode04Flow` | Registers each garrison and counter-attack wave as an opposing squad. Raiders use the *assault* posture (never fall back). |

### Squad rules (`data/ai/squad-behaviour.json` → `tuning`)

* **Patrol → Engage**: a member sees or is shot by a hostile within `loseThreatSeconds`. The whole squad takes the target.
* **Engage → Fallback**: alive / size is at or below `fallbackStrength` (patrol posture only).
* **Engage → Patrol**: no threat for `disengageSeconds`.
* **Fallback → Patrol**: after `regroupSeconds` once the threat is lost, or after `maxFallbackSeconds` regardless.
* `patrolRadiusCm`: how far members wander from where they spawned. `coverSearchRadiusCm`, `coverGridSpacingCm`, `useEqsCover`: the cover query.
* `phases[]`: `{ "state": "<mission state>", "deployments": [{ "faction", "sector", "size", "posture": "patrol" | "assault" }] }`.
  Do not add a `team` key; `npm run validate` rejects it. The shipped data deploys three allied coalition infantry at the
  landing beach (north or west) and at the oil terminal once the island is secured.

Edit the JSON, run `npm run validate`, press Play. No recompile. The data path is the same as for the other game data (see `game/README.md`).

## StateTree

StateTree assets are binary editor assets and cannot be committed from this repo, so the built-in C++ logic runs by default
and gives the full patrol / engage / fallback cycle. To drive units with your own StateTree instead:

1. Enable the **StateTree** and **GameplayStateTree** plugins (already listed in `MidnightHavoc.uproject`).
2. Create a Blueprint child of `HavocAIController` and add a **StateTreeAIComponent**. Assign a StateTree asset using the
   `StateTreeAIComponentSchema`.
3. Set `AIControllerClass` of the enemy character Blueprint to that child. The built-in logic switches itself off when
   the controller has a StateTree component (or a `BehaviorTreeAsset`).
4. In the tree use the Blueprint-callable pieces of the controller: read `SquadMode` (Patrol / Engage / Fallback) and
   `State`, and call `CommandPatrol`, `CommandEngage`, `CommandFallback`, `GetThreat`, `IsHostileToMe`. A simple tree:
   root with three states, enter *Patrol* / *Engage* / *Fallback* by `SquadMode`, and call the matching command in each.

Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine>

## EQS cover

The cover query is built in C++ at runtime (`HavocCoverEqs::BuildCoverQuery`), because EQS query assets are binary too.
Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-in-unreal-engine>.
Set `useEqsCover` to `false` in the JSON to use only the trace search.

## NavMesh for the Kharg level

* Project settings: `Config/DefaultEngine.ini` sets the NavMesh agent (radius 34, height 176, slope 44, step 35), coarser cells
  for the 20 km landscape, and `bAutoCreateNavigationData`. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-system-in-unreal-engine>
* Level: `Content/Python/build_kharg_level.py` places one **NavMeshBoundsVolume** (`NavMeshBounds_Kharg`) over the island: both beaches,
  the airstrip, the oil terminal and the town. If you build the level by hand, place one volume over the island by hand
  (<https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-navigation-in-unreal-engine>).
* Check it: in the editor viewport press **P**. The island should be green. If it is not, scale the volume (its brush is a 200 cm cube by default)
  or raise the volume so it overlaps the landscape.
* With no NavMesh units still work: patrol points and movement fall back to straight-line steering, and cover to the trace search.

## Checklist (Windows build, then in the editor)

- [ ] `MidnightHavoc` builds (Win64 Development Editor and Game), including `StateTreeModule`, `GameplayStateTreeModule` and the EQS includes
- [ ] `Automation RunTests MidnightHavoc.AI` passes (3 tests: data and spawn plan, patrol/engage transitions, smoke cycle)
- [ ] Open `L_KhargHormuz`, press **P**: NavMesh covers the island
- [ ] Play, choose the north beach: log shows `Squads: phase 'E04_NORTH_LANDING': 1 squad(s) deployed`; three allied Marines spawn on the beach
- [ ] Opposing garrison and allied squad shoot each other; you do not hurt allies and they do not hurt you
- [ ] Shoot a garrison member: the rest turn on you (Engage); kill all but ~40 % and the rest run to cover (Fallback), then return to patrol after ~8 s out of sight
- [ ] `LogHavocWorld` shows no `Squads:` warnings; `squad:mode` events appear for patrol, engage and fallback
- [ ] Standing behind a wall, units in Fallback stop at the wall (EQS), not in the open
- [ ] ISIS units (west beach) never appear as allies; possession of an enemy character is refused
