# Episode 07 playtest checklist

Use this with the packaged build (`MidnightHavoc-Win64` artifact) once the Parvaneh level exists. Keys are in `game/README.md` ("How to play").
If a step fails, note the step number and attach `Saved/Logs/MidnightHavoc.log`.

Help: [Packaging](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-unreal-engine-projects),
[Automation tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-in-unreal-engine),
[Output log](https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine).

## Before you start
- [ ] `npm test` and `npm run validate` pass (covers `src/tests/episode07.test.ts`).
- [ ] The Windows build job is green.

## The loop
- [ ] Link up with all three resistance squads; the objective moves to the Old Bazaar.
- [ ] Civilians walk the bazaar and courtyard streets and flee from gunfire; resistance squads fight beside you.
- [ ] Escort every courtyard civilian to shelter; the objective advances.
- [ ] Choose bridges or arcades; Dome Square and the compound can be taken.
- [ ] Field hospital: treat patients. Aid drop: deliver crates. Each raises Hearts & Minds on the HUD.

## ROE
- [ ] Harm a civilian: the objective is marked failed, Hearts & Minds drops 15.
- [ ] Harm a surrendered soldier: the objective is marked failed, Hearts & Minds drops 10.
- [ ] Three violations: FAILED, and Restart from last checkpoint undoes them.
- [ ] A clean run with both side objectives ends SUCCESS; any violation ends PARTIAL.

## Rules
- [ ] The enemy is only the regime's security forces. No real people, equipment specs or targeting data appear in text, HUD or logs.
- [ ] ISIS never appears as a playable side.
