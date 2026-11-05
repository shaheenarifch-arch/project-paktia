# Episode 04 playtest checklist

Use this with the packaged build from the item-1 workflow (`MidnightHavoc-Win64` artifact) or in the editor on `L_KhargHormuz`.
Keys are in `game/README.md` ("How to play"). If a step fails, note the step number and attach `Saved/Logs/MidnightHavoc.log`.

Help: [Packaging](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects),
[Automation tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine),
[Output log](https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine).

## Before you start
- [ ] Actions run is green, including the automation tests (`MidnightHavoc.Playable.*` and the existing ones).
- [ ] The build contains the level `Content/Maps/Kharg/L_KhargHormuz` (built by `game/Content/Python/build_kharg_level.py`) and `data/` beside the game folder.

## Menus
- [ ] Main menu opens: Play Episode 04, Settings, Quit. Settings toggles and sliders change and persist after restart.
- [ ] Esc during play opens the pause menu (Resume, Restart from last checkpoint, Settings, Quit); the game is paused behind it.

## The loop
- [ ] Marine moves, sprints, crouches, aims, fires (ammo falls), reloads (R); the health and ammo HUD update.
- [ ] Beach recon: Raven flies (W/A/S/D, Space/Ctrl); the objective turns to "choose a beach" after both beaches are overflown; Tab and T switch between Marine and Raven.
- [ ] Press 1 (north) or 2 (west): the Marine lands, beach defenders fight back (they patrol, take cover, engage); killing them secures the beachhead.
- [ ] Terminal and airstrip garrisons are cleared; shooting the terminal parts lowers "Terminal integrity" on the HUD.
- [ ] Hold the oil terminal: the timer only runs inside the terminal area; counter-attacks arrive.
- [ ] Lane clearance: Raven reveals hazards as it flies the lanes; E marks one and it is cleared after a few seconds; objective advances when all are cleared.
- [ ] Tanker escort: tankers sail along both lanes; a contact warning appears; G calls the Falcon (possess it, fly, Tab back); a covered tanker is not damaged.
- [ ] Results screen appears with SUCCESS, PARTIAL or FAILED and the stats; Play again works.
- [ ] Die on purpose: you return to the last checkpoint and reinforcements drop. Run reinforcements to zero before the terminal is secured to see FAILED and Restart from last checkpoint.

## Rules
- [ ] ISIS fighters appear (west beach, counter-attacks) and only as AI; you can never control one.
- [ ] No real people, equipment specs or targeting data appear in text, HUD or logs.

## Sound and effects
- [ ] If the Fab packs are imported: rifle fire, muzzle flash, explosions and coast ambience play. If not, `LogHavocAssets` lists the roles to fill in `Config/HavocAssets.ini`; the game still plays.
