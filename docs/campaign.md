# Campaign

Three episodes, each handing results to the next. All factions, places and
operations are fictional (see `README.md`, Safety boundary).

| Episode | Operation | Where | Code | Data |
| --- | --- | --- | --- | --- |
| 1 | Mountain Shield | Paktia Highlands: Checkpoint Echo, Ridge Station, Falcon-2 crash grid R-17 | `src/gameplay/missions/mission01/` | (hand-written state machine) |
| 2 | Silent Corridor | Valley corridor → Dry Riverbed → South Gate | `src/gameplay/missions/episode02/` | `data/missions/episode-02-silent-corridor.json` |
| 3 | Broken Citadel | Gardun, all five Stronghold Assault phases | `src/gameplay/missions/episode03/` | `data/missions/episode-03-broken-citadel.json` + `operation-broken-citadel.json` |

`src/gameplay/campaign/Campaign.ts` keeps the order, unlocks an episode when
the previous one ended in SUCCESS or PARTIAL, never lets a worse replay
overwrite a better result, and saves/loads as plain JSON.

## Mission framework

`src/gameplay/missions/framework/` runs any mission described as data:

- **States** with `next`, named `branches` (player choices), `requires` flags, `effects` and `checkpoint`/`terminal`.
- **`parseMissionDefinition`** rejects a mission that isn't marked fictional, uses an unknown Gardun sector, points at a missing state, has a dead end, or has unreachable states.
- **`MissionRunner`** emits `mission:state`, `mission:blocked` (with the missing flags), `mission:checkpoint` and `mission:complete` on the `EventBus`, so UI, audio and telemetry subscribe instead of polling.
- **Checkpoints** store the state, flags, history and the mission's own systems; `restoreCheckpoint()` puts all of them back. This is the restore path Mission 01 lacks.

Mission 01 is unchanged; `Campaign.carryoverFromMission01()` reads its result.

## Episode 2: Operation Silent Corridor

The task force escorts the Falcon-2 crew recovered in Mission 01 and a supply
truck to Gardun's South Gate, then sets up the staging area for the assault.

1. **Form up.** Warden-1 leads; Nomad-1 carries the crew; Nomad-2 carries supplies. If Mission 01 abandoned the crash recovery, Nomad-1 carries supplies instead.
2. **Relay Kilo.** Raven flies out and drops a field relay. It only counts where the drone link still reaches (`RelayNetwork` uses `DroneSignalManager`).
3. **Scout the corridor.** Raven must reach the corridor end with at least a DEGRADED link; what it sees becomes contacts.
4. **Communications outage** (surprise event). Every relay is jammed and the link is lost. A squad must stand at Relay Kilo (within 30 m) to restore it.
5. **Bridge obstruction** (surprise event, checkpoint). The player chooses one:
   - **Riverbed detour:** the whole convoy takes the Dry Riverbed and is ambushed in the narrows. Clear all three ambush contacts to move on.
   - **Drainage infiltration:** infantry only (no vehicle fits the tunnel). Clear the far bank, then Warden-1 clears the bridge; the bridge needs the Warden to be operational.
6. **South Gate** (checkpoint). Clear the staging yard, then establish staging.
7. **Outcome:**
   - Full convoy: SUCCESS.
   - Vehicles lost: PARTIAL.
   - Crew transport lost (or, without crew, both Nomads lost): FAILED, and the player restarts from the last checkpoint.

The carryover to Episode 3 is crew delivered, Nomads remaining, Warden operational, route taken and relays online.

## Episode 3: Operation Broken Citadel

The Stronghold Assault phases, played as a campaign mission. Phases and
objectives are read from `operation-broken-citadel.json`, so the campaign and
multiplayer share one source.

**Phase rules**

- Phase 1 is complete when **either** the Airfield or the Industrial Depot is taken (`docs/gardun-map.md`).
- Every other phase needs all of its objectives.
- Only the current phase's objectives can be captured.

**Unlocks**

| Objective | Effect |
| --- | --- |
| Airfield | Falcon air support, but only if Episode 2 delivered the crew |
| West Ridge radio + Government relay | Enemy counterattacks at 60% strength |
| Mountain Fort | Aegis online |
| Medical Station | Protected zone, so Aegis can't select anything inside it |

**Reinforcements:** 20, plus 5 for each Nomad that survived Episode 2. When they run out, the assault fails back to the last checkpoint.

**Surprise event:** exactly one per match, picked from the map's list by a seeded hash so replays and server restarts agree. It fires when phase 3 starts and ends when phase 3 does. Its effects are abstract modifiers that engine systems read:

- sandstorm: drone sensor range and visibility
- communications outage: relay interference
- tunnel discovery: opens the Old Gardun cellars route
- bridge obstruction: blocks the riverbed bridge
- nightfall: visibility

## Engine integration

As with Mission 01, the engine reports results and the episode decides what happens:

- **Episode 2 calls:** `placeRelayKilo`, `scoutCorridor`, `restoreRelayKilo`, `chooseRoute`, `damageVehicle`, `clearAmbushContact`, `clearFarBank`, `clearBridge`, `clearStaging`, `finish`, then `progress()` to move on.
- **Episode 3 calls:** `beginAssault`, `captureObjective` (server-confirmed only) and `spendReinforcements`.
- **Reading state:** `snapshot()` returns everything the UI shows.

Tests: `src/tests/episodes.test.ts`. Data checks: `npm run validate`.
