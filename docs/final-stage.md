# Final stage: the Iran liberation arc (Episodes 05–08)

> **Help:** this is the story bible and data guide for Episodes 05–08. Epic's Unreal docs are linked at the end. Epic issue: <https://github.com/shaheenarifch-arch/project-paktia/issues/48>. Campaign overview: `docs/campaign.md`. Data rules: `data/README.md`.

Status: F0 (story bible and data). The files below are data only; Episodes 05–08 are built on them in later issues without new contracts.

## Arc summary

After the Strait of Hormuz is reopened (Episode 04), an invented Iranian resistance network asks the coalition for help. The coalition **supports**; the resistance **leads**. Over four episodes the people of Iran take back their villages, a provincial city and the capital from the regime's security forces, then vote. The campaign ends with the coalition handing over to an elected civilian government and leaving. This is liberation, not occupation.

The arc is a declared real-world hypothetical (`setting: real_world_hypothetical`, `fictional: false`), like Episode 04. It is not a reconstruction of any real event.

## Tone guide

- **The people are the heroes.** Resistance members, neighbourhood guards, medics, defectors and ordinary voters carry the story. Coalition characters are supporting cast.
- **Coalition supports, doesn't occupy.** The resistance gives the orders for the ground fight. Coalition help is scouting, air cover, aid and logistics. Nothing in the arc is held by coalition forces after Episode 08.
- **The enemy is the regime's security forces, never the Iranian people.** Conscripts are people too. Offer surrender first.
- **Civilians are never targets.** Civilian harm costs Hearts & Minds score and can fail an episode.
- **No real people.** No real leaders, officials or named individuals. All characters, units and the resistance network are invented.
- **No real equipment specs, targeting data or operating/building instructions.** Units use game-balance ratings only.
- **Fictional or composited places.** Names and layouts are invented, Iranian-inspired architecture (qanats, bazaars, domed halls). They are not meant to match real towns.
- ISIS stays AI-only and does not appear in this arc.

## The resistance network (all invented)

The network is a loose web of cells, with local councils, medics and defectors. There is no single leader and no real person behind any role.

| Name | Role | Used in |
| --- | --- | --- |
| **Sahar** | Cell coordinator at the highland safehouse. Careful, warm, insists the coalition listen first. | E05 |
| **Old Babak** | Village elder of Qanat Village; keeper of the water channels. | E05 |
| **Dr. Roya** | Field doctor who runs the Shahrbaran hospital district and decides where aid goes. | E06 |
| **Kian** | Radio technician who keeps the city radio station alive. | E06 |
| **Mina** | Council spokesperson at the Gulshar assembly hall. | E07 |
| **Sergeant Farhad** | Capital-guard conscript who defects with his squad. | E07 |
| **Madar Leila** | Elected civilian caretaker of the interim assembly. | E08 |
| **Coalition liaison (Warden-1 team)** | The coalition voice; always supports, never leads. | E05–E08 |

Rule for writers: any new name must be invented. Do not use names of real officials or well-known figures.

## Episodes

Each episode is a state graph that passes `parseMissionDefinition`. Terminal outcomes are `SUCCESS`, `PARTIAL` or `FAILED`.

| Ep | Operation | Data | Map |
| --- | --- | --- | --- |
| 05 | First Light | `data/missions/episode-05-first-light.json` | `data/maps/first-light-highlands.json` |
| 06 | Broken Chains | `data/missions/episode-06-broken-chains.json` | `data/maps/shahrbaran-city.json` |
| 07 | Long Night | `data/missions/episode-07-long-night.json` | `data/maps/gulshar-capital.json` |
| 08 | Open Hand | `data/missions/episode-08-open-hand.json` | `data/maps/homeland-handover.json` |

### Episode 05: Operation First Light

A small coalition team crosses the highlands to meet Sahar's cell. Choose the ridge pass (fast, watched) or the dry wadi (slow, hidden). Meet the cell and listen. Protect Qanat Village from security squads, escort the aid convoy in, then offer the ridge-road garrison, mostly conscripts, a chance to surrender. Teaches Hearts & Minds and surrender.
Outcomes: trusted (`SUCCESS`), uneasy (`PARTIAL`), failed.

### Episode 06: Operation Broken Chains

Shahrbaran rises. Choose to hold the bridges first or shelter the bazaar crowds first, then free the detained, retake the radio station, bring aid to the hospital district and offer the headquarters garrison surrender. Dr. Roya and Kian lead on the ground.
Outcomes: city free (`SUCCESS`), city scarred (`PARTIAL`), failed.

### Episode 07: Operation Long Night

The uprising reaches Gulshar. Meet the council; defectors from the capital guard arrive. Take the broadcast house first or the guard barracks first. Protect the central square from hardline enforcers, secure the ministries and public archives intact, bring aid to the field hospitals and make the final surrender offer.
Outcomes: capital free (`SUCCESS`), capital free with a hardliner remnant (`PARTIAL`), failed.

### Episode 08: Operation Open Hand

The coalition steps back. Help guards secure the assembly hall, escort ballots and aid, then deal with a hardliner remnant: **offer surrender** or **contain the enclave**. Protect the polling districts on election day, observe the civilian count, hand over and withdraw. The Hearts & Minds score picks the ending.

## Ending variants

Selected at `E08_ENDING` from the final score (`data/final-stage/hearts-minds.json`, `endings`). The coalition withdraws in every ending.

| Ending | Score | Outcome | State | What the player sees |
| --- | --- | --- | --- | --- |
| Elected government | 75–100 | `SUCCESS` | `E08_END_ELECTED` | A free, elected civilian government with the people's trust. |
| Fragile handover | 45–74 | `PARTIAL` | `E08_END_FRAGILE` | An elected government, thin trust, fragile peace; coalition support stays on call. |
| Unfinished peace | 0–44 | `PARTIAL` | `E08_END_UNFINISHED` | The vote happened, but the transition is contested. |

A failed episode (`E08_OPERATION_FAILED`) restarts from its last checkpoint; it is not an ending.

## Hearts & Minds spec

Data: `data/final-stage/hearts-minds.json`.

- A score from 0 to 100. It starts at 30 and carries between episodes.
- **Gains** (capped per category per episode): civilians protected, surrenders accepted, defectors welcomed, aid delivered.
- **Penalties** (not capped): civilian harmed (−8), surrendered soldier fired upon (−12), civilian infrastructure damaged, aid convoy lost, detainee mistreated.
- **Fail rule:** `civilianHarmFailCount` (5) civilian-harm events in one episode fail it.
- **Unlocks resistance support** at 20 / 40 / 60 / 80: scouts, neighbourhood guards, the defector battalion and the city uprising force. Each tier lists the formations it unlocks (all in `data/formations/iran-resistance.json`).
- **Selects the ending** at E08 using the table above.

Mission effects the engine handles: `heartsminds.start`, `heartsminds.start_episode`, `heartsminds.track_civilians`, `heartsminds.track_aid`, `heartsminds.track_infrastructure`, `heartsminds.reveal_civilians`, `ending.select`.

## Surrender and defection spec

Data: `data/final-stage/surrender-defection.json`.

- **Offer:** by loudspeaker, broadcast or resistance liaison, to a unit that is outnumbered or cut off. A 20-second ceasefire applies while the offer stands. A broadcast needs the `surrender.broadcast` effect (the radio or broadcast station is taken).
- **Accept chance:** a base chance per unit type, raised by low morale and by the Hearts & Minds score. Hardline enforcers almost never accept; conscripts often do.
- **Outcomes:** *surrender* (unit leaves the fight and goes to a collection point), *defect* (unit becomes a resistance defector squad), or *refuse* (fighting resumes, offer can repeat after a cooldown).
- **Protection:** surrendered soldiers are never targets. Firing on them costs −12; leaving prisoners without a collection point costs −6.
- Defectors join the resistance as `iran_resistance_defector_squad` and count toward the defector battalion (max 4 per episode).

Mission effects: `surrender.enable`, `surrender.broadcast`, `resistance.unlock_cell`, `resistance.unlock_defectors`, `spoilers.offer`, `spoilers.contain`, `coalition.withdraw`.

## Data added by F0

| Path | Contents |
| --- | --- |
| `data/factions/iran-resistance.json` | Playable ally, invented. |
| `data/units/iran-resistance.json`, `data/formations/iran-resistance.json` | Resistance units and formations. |
| `data/units/iran-regime-security.json`, `data/formations/iran-regime-security.json` | Regime security-force units and formations (faction `iran`). |
| `data/maps/*.json` (4 files above) | Fictional inland locations. |
| `data/missions/episode-05..08-*.json` | State graphs. |
| `data/final-stage/hearts-minds.json`, `surrender-defection.json` | System specs as data. |
| `src/tests/final-stage-data.test.ts` | Validation tests for all of the above. |

## Help links (Epic)

- Data-driven gameplay elements: <https://dev.epicgames.com/documentation/en-us/unreal-engine/data-driven-gameplay-elements-in-unreal-engine>
- Behavior Trees (for surrender and defection AI): <https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-trees-in-unreal-engine>
- Gameplay Tags (for mission effects and flags): <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine>
