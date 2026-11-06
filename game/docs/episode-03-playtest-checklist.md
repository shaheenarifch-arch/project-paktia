# Episode 03 playtest checklist

Use this with the packaged build from the item-1 workflow (`MidnightHavoc-Win64` artifact) or in the editor. Start with `-HavocEpisode=3`
(or open any level with `?Episode=3`). Keys are in `game/README.md` ("How to play"). If a step fails, note the step number and attach `Saved/Logs/MidnightHavoc.log`.

Help: [Packaging](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects),
[Automation tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine),
[Output log](https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine).

## Before you start
- [ ] Actions run is green, including the automation tests (`MidnightHavoc.Missions.Episode03.*`).
- [ ] The log says "building the greybox Gardun" (no Gardun sector volumes in the level).

## The loop
- [ ] Main menu shows Episode 03; Start Episode 03 puts the Marine at South Gate and the HUD shows "PHASE 1 / 5" and the objective.
- [ ] Phase 1: pillars mark the Airfield and the Industrial Depot; taking either one (clear the garrison, stand in the marker) moves to phase 2.
- [ ] Taking the Airfield: with a Falcon crew from Episode 02 the toast says Falcon support is unlocked and G strikes nearby hostiles, then rearms; without the crew G says there is no crew.
- [ ] Phase 2: taking only one relay site does not advance; the second drops the HUD to "Enemy counterattacks: 60%" and counterattack waves get smaller.
- [ ] Phase 3: a surprise event toast appears and the HUD shows "EVENT: ..."; it ends when phase 4 starts. The same seed gives the same event.
- [ ] The Medical Station has no garrison; holding it shows the protected-zone toast and Falcon fire holds inside it.
- [ ] Phase 4: taking the Mountain Fort turns "Aegis: online".
- [ ] Phase 5: four Citadel objectives, each with more defenders; taking the last shows CITADEL SECURED.
- [ ] Reinforcements start at 20 (+5 per surviving Nomad); dying costs 2 and returns you to the last checkpoint; at zero the results screen shows FAILED and Restart from last checkpoint works.
- [ ] Results: phases reached, objectives, reinforcements, Aegis, surprise event; Play again works.
