# Play: the Gardun campaign (browser prototype)

A playable browser version of the campaign so it can be played before the
Unreal gray-box exists. It is a top-down three.js prototype, not the final
game. Episodes 02 and 03 are playable and linked: Episode 02's result is what
Episode 03 starts with.

The rules are not re-implemented here. The prototype drives the real domain
layer and data, the same code `npm test` covers:

| From the repo | Used for |
| --- | --- |
| `Campaign` | Episode order, unlocks, results that never get worse on a replay, carryover from one episode to the next (saved in the browser) |
| `SilentCorridorMission`, `ConvoyEscort`, `RelayNetwork` (`src/gameplay/missions/episode02/`) | Every Episode 02 step, convoy integrity and losses, the drone link and Relay Kilo, the outage, the bridge choice, the ambush contacts, the result and its carryover |
| `BrokenCitadelMission` (`src/gameplay/missions/episode03/`) | Phases, phase-1 "either one" rule, unlocks, reinforcements, fail state |
| `MissionRunner` | Checkpoints and restore in both episodes |
| `SurpriseEventDirector` | Episode 03's seeded phase-3 event and its modifiers |
| `AegisSupportController` + `ContactManager` + `TargetValidator` | Aegis: select → validate → authorise → commit → complete, refused inside the Medical Station protected zone |
| `data/missions/*`, `data/maps/gardun-city.json`, `data/weapons/kr47.json` | Briefings, states, objectives, protected zones, surprise events, KR-47 magazine and fire modes |

The browser side adds only what an engine adapter would: movement, shooting,
hostile behaviour, vehicles following routes, the Raven drone, capture zones
and the two maps. Each map maps world units back to the game-space metres the
domain layer uses (1 unit = 4 m).

## Layout

```text
src/engine.ts      shared battle layer: player and KR-47, hostiles, shots, camera, input, pause/end
src/hud.ts         data-driven HUD: briefing, vitals, abilities, minimap, choice and result cards
src/collision.ts   collision grid, instanced solids, ground ribbons, route paths
src/corridor.ts    Episode 02 map: Checkpoint Echo → Relay Kilo → bridge → South Gate
src/episode02.ts   Episode 02 on SilentCorridorMission
src/gardun.ts      Episode 03 map: Gardun
src/episode03.ts   Episode 03 on BrokenCitadelMission
src/campaign.ts    campaign save/load in the browser
src/main.ts        campaign menu
```

A new episode is one map file plus one `Battle` subclass that reports results
into its mission.

## Run

```bash
cd play
npm install
npm run dev        # open the printed URL
npm run typecheck
npm run build      # static site in play/dist
```

## How to play

Common: **WASD** move, **Shift** sprint, **mouse** aim, **click** fire, **R** reload, **B** auto/single, **Esc** pause, **M** sound.

**Episode 02, Operation Silent Corridor.** Escort Warden-1 and the two Nomads from Checkpoint Echo to South Gate.

- **Q** flies Raven (the squad holds still). The link only reaches about 1.6 km from a working relay.
- Fly to the blue ring and press **E** to drop Relay Kilo, then fly to the amber beacon beyond the bridge to scout.
- When the outage hits, run to Relay Kilo to restore it. The convoy waits if you are too far away.
- At the blocked bridge choose (**1**/**2**): the riverbed detour (ambush in the narrows) or the drainage tunnel (**E** on the hatch, clear the far bank, Warden-1 clears the wreck).
- Clear the South Gate yard. Losing Nomad-1 with the crew fails the episode; losing Nomad-2 gives a result with losses.

**Episode 03, Operation Broken Citadel.** Capture the amber rings across five phases. Each time you go down costs one reinforcement. **F** calls Falcon air support at the cursor (Airfield taken and crew delivered). **G** calls Aegis on a hostile you have kept in sight (Mountain Fort taken). **E** uses a cellar hatch if the tunnel event opens them.

Episode 01 is not playable yet; the menu asks how it ended (crew recovered or not), which decides what Nomad-1 carries.

## Not in this prototype yet

Episode 01, Episode 04, allied squads, driving vehicles, multiplayer and the
dedicated server.
