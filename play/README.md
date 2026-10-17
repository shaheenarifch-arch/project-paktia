# Play: Operation Broken Citadel (browser prototype)

A playable browser version of Episode 03 so the campaign can be played before
the Unreal gray-box exists. It is a top-down three.js prototype, not the final
game.

The rules are not re-implemented here. The prototype drives the real domain
layer and data, the same code `npm test` covers:

| From the repo | Used for |
| --- | --- |
| `BrokenCitadelMission` (`src/gameplay/missions/episode03/`) | Phases, which objectives count, phase-1 "either one" rule, unlocks, reinforcements, fail state, checkpoints and restore |
| `SurpriseEventDirector` | The one seeded surprise event in phase 3 and its modifiers (visibility, sensor range, interference, cellars, bridge) |
| `AegisSupportController` + `ContactManager` + `TargetValidator` | Aegis: select → validate → authorise → commit → complete, refused inside the Medical Station protected zone |
| `data/missions/episode-03-broken-citadel.json`, `operation-broken-citadel.json`, `data/maps/gardun-city.json` | Briefing, phase objectives, protected zones, surprise events |
| `data/weapons/kr47.json` | KR-47 magazine size and fire modes |

The browser side only adds what an engine adapter would: movement, shooting,
enemy behaviour, capture zones and the Gardun layout in `src/world.ts`
(1 world unit = 4 m, mapped back to game-space metres for the domain layer).

## Run

```bash
cd play
npm install
npm run dev        # open the printed URL
npm run typecheck
npm run build      # static site in play/dist
```

## How to play

Pick how the Episode 02 convoy arrived (it sets reinforcements and whether the
Falcon crew is available), then deploy from South Gate.

- **WASD** move, **Shift** sprint, **mouse** aim, **click** fire, **R** reload, **B** auto/single
- Stand in an amber ring with no hostiles inside to capture it. Hostiles inside a ring push it back.
- Each time you go down, one reinforcement is spent. At zero the assault fails and you can restore the last checkpoint.
- **F** calls Falcon air support at the cursor once the Airfield is taken, if Episode 02 delivered the crew.
- **G** calls Aegis on a hostile you have kept in sight, once the Mountain Fort is taken.
- **E** uses a cellar hatch if the tunnel discovery event opens them. **Esc** pauses. **M** toggles sound.

## Not in this prototype yet

Allied squads, the Warden and Nomad vehicles, Episodes 01 and 02, multiplayer and
the dedicated server. Those belong to the Unreal client plan in `FUTURE-PLAN.md`.
