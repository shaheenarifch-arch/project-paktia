# Episode 02 playtest checklist (greybox)

Use this with the packaged build (`MidnightHavoc-Win64` artifact) or in the editor on `L_KhargHormuz`. Keys are in `game/README.md`
("How to play (Episode 02, greybox)"). If a step fails, note the step number and attach `Saved/Logs/MidnightHavoc.log`.

Help: [Automation tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine),
[Output log](https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine).

## Before you start
- [ ] Actions run is green, including `MidnightHavoc.Missions.Episode02.*`.
- [ ] `data/` is beside the game folder (the mission JSON is read from there).

## The loop
- [ ] Main menu > Campaign shows Episode 02 with **Start Episode 02**; Episodes 01 and 03 stay locked; Episode 04 still starts.
- [ ] Start: the Marine appears on the greybox slab at Checkpoint Echo, the convoy (3 boxes) is beside it, the Raven is airborne, the objective says to place Relay Kilo.
- [ ] Press E far from the relay site: a toast says how many metres to go. Over the site: Relay Kilo drops and the objective moves to the scout.
- [ ] Raven at the corridor end: "Corridor scouted"; with a weak link it says so and does not complete.
- [ ] The convoy rolls and halts at the outage point. The Raven cannot be launched (T explains the jam). The hint shows the distance to Relay Kilo.
- [ ] E at Relay Kilo (within 25 m) restores it; the Marine is moved to the bridge and the objective asks for the route (1 / 2).
- [ ] **1 Riverbed:** 3 hostiles appear in the narrows, the convoy takes damage in volleys (status block), killing all three ends the ambush. Losing Nomad-1 (crew) shows CONVOY LOST.
- [ ] **2 Drainage:** the far bank has hostiles; clearing them starts Warden-1's bridge timer; the convoy then moves on. Convoy vehicles are never damaged on this route.
- [ ] South Gate: yard hostiles are cleared, the staging timer runs, then the results screen shows SUCCESS (full convoy) or PARTIAL (lost vehicles) with the route, crew, Nomads and Warden lines.
- [ ] Die on purpose: you return to the last checkpoint (Relay Kilo, the bridge or South Gate) with the convoy as it was then.
- [ ] FAILED offers Restart from last checkpoint; Play Episode 02 again works.

## Rules
- [ ] Hostiles are AI-only; nothing lets you control them.
- [ ] The HUD tracker follows the eight steps ([x] / [>] / [ ]).
