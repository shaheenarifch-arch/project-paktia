# Effects (VFX) Production Plan

Scope: battlefield effects for Operation Midnight Havoc (design doc section 3, "Battlefield presentation and audio"). Planning only. All effects are built in Unreal Engine 5.5 using Niagara. Audio hooks are in `docs/audio-voice-plan.md`.

Help links (Epic):

- Niagara overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-visual-effects-in-unreal-engine
- Niagara key concepts: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-niagara-effects-for-unreal-engine
- Niagara performance and scalability: https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-in-niagara-for-unreal-engine
- Niagara effect types and budgets: https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-effect-types-in-unreal-engine
- Niagara GPU simulation: https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-gpu-simulation-in-unreal-engine
- Niagara data channels (many small events): https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-data-channels-in-unreal-engine
- Post process materials (night vision and thermal): https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine
- Unreal Insights profiling: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine

(Epic reorganises its docs from time to time. If a link moves, search the title on the documentation site.)

## 1. Effect list

Priority: **P0** needed for the vertical slice, **P1** needed for full release, **P2** polish or later.

| # | Effect | Priority | Niagara notes |
|---|---|---|---|
| 1 | Muzzle flash | P0 | Short sprite burst plus light. Pool and reuse emitters. Use a data channel for many shooters. Cheap, no shadows. |
| 2 | Tracers | P0 | Ribbon or beam, a few per second per firing unit. Colour per faction. Night only boost. |
| 3 | Grenade smoke | P1 | Sprite emitter with flipbook, lifetime 20 to 40 s. Collides with nothing. Reacts to wind. |
| 4 | Vehicle smoke (damaged) | P0 | Looping sprite emitter, intensity scales with damage. |
| 5 | Burning (vehicles, buildings) | P1 | Fire flipbook plus dark smoke, scalability cuts the smoke first. |
| 6 | Dust and sandstorm | P1 | Mix of local dust (vehicles, footsteps) and a large fog and particle layer for storms. Storm uses a volumetric or fog card approach, not thousands of particles. |
| 7 | Artillery impact | P0 | One-shot: flash, fireball, debris mesh particles, smoke column, ground decal. Several quality tiers. |
| 8 | Rocket impact | P0 | Smaller variant of artillery, shares assets. |
| 9 | Missile impact | P1 | Larger shockwave ring, heavier debris. Rare, so can afford more cost. |
| 10 | Vehicle destruction stages | P0 | Stages: smoking, burning, destroyed, wreck. Swap mesh or material on each stage and spawn the matching Niagara system. |
| 11 | Secondary explosions | P1 | Delayed (0.5 to 4 s) small bursts from ammunition or fuel. Randomised, capped per wreck. |
| 12 | Ship damage | P1 | Hit sparks, fires on deck, smoke trails, water splashes and foam at the waterline. Reuse fire and smoke. |
| 13 | Helicopter rotor wash | P1 | Dust and sand ring under the helicopter, scaled by height above ground. Water surface variant for ships and coast. |
| 14 | Drone effects | P2 | Small prop-wash dust, rotor blur, signal glitch overlay on drone camera, crash puff. |
| 15 | IED blast (threat event) | P0 | Road-side explosion with dust column and debris. Used as a scripted threat the player must react to. Presentation only. |
| 16 | Night vision view | P1 | Post process material: green tint, grain, bloom on lights, reduced saturation. Not Niagara. |
| 17 | Thermal view | P1 | Post process with custom depth or stencil to mark warm objects, white-hot or black-hot palette. Not Niagara. |
| 18 | Nuclear detonation | P2 | Only for the strategic deterrence event. See notes below. |

### Notes

- **IED blast** is a gameplay threat event shown to the player as something to avoid and respond to. The effect covers the visuals of the blast only. No device construction details appear in the game or in these docs.
- **Nuclear detonation** appears only in the strategic deterrence event, never in routine combat. One dedicated Niagara system plus a post process flash, sky and fog change, and camera shake. It is a scripted cinematic with a warning, allowed to exceed the normal budget because nothing else is happening on screen. Handle it with care in tone: no celebration, and show consequences. It should be easy to disable or reduce flashing for accessibility.
- **Accessibility:** provide a setting to reduce flashes, camera shake and bloom for all explosion effects.
- **Event hooks:** effects are spawned from `UHavocEventSubsystem` events, the same as audio. Existing events are listed in `game/README.md`. Combat events are proposed, see `docs/audio-voice-plan.md` section 2.5.

## 2. Performance budget

Target: 300 units on screen at 60 fps on the PC target spec, and 30 fps on the lower platform spec. These are rough starting numbers to be tuned by profiling with Unreal Insights and `stat Niagara`. Budgets are per effect type at the High quality tier. Medium takes about half, Low about a quarter.

Overall VFX budget: about 1.5 to 2.5 ms GPU and about 1 ms game thread CPU for Niagara in a typical large fight.

| Effect | Particles per instance (rough) | Max active instances | Total particle cap | GPU target | Notes |
|---|---|---|---|---|---|
| Muzzle flash | 5 to 15 | 40 | 600 | under 0.1 ms | Only nearest 40 firers. Others use a light-only flash or nothing. |
| Tracers | 1 to 3 | 60 | 180 | under 0.05 ms | Skip tracers beyond view distance. |
| Grenade smoke | 100 to 200 | 6 | 1,200 | 0.2 ms | Large sprites cause overdraw, keep alpha cheap. |
| Vehicle smoke | 40 to 80 | 25 | 2,000 | 0.3 ms | Reduce on distance. LOD to a single billboard. |
| Burning | 80 to 150 | 12 | 1,800 | 0.3 ms | Cull when off screen. |
| Dust and sandstorm | 200 local, 5 to 10 big cards for storm | 20 local | 4,000 | 0.4 ms | GPU sim. Overdraw is the main cost. |
| Artillery impact | 150 to 400 | 6 | 2,400 | 0.3 ms | Short lifetime. Stagger spawns across frames. |
| Rocket and missile impact | 100 to 300 | 8 | 2,400 | 0.25 ms | Share artillery assets. |
| Vehicle destruction and secondary | 150 to 300 | 8 | 2,400 | 0.25 ms | Cap secondary explosions at 3 per wreck. |
| Ship damage | 200 to 500 | 3 | 1,500 | 0.2 ms | |
| Rotor wash | 100 to 300 | 6 | 1,800 | 0.2 ms | Only for nearest helicopters. |
| Drone effects | 5 to 30 | 20 | 600 | under 0.1 ms | |
| IED blast | 300 to 600 | 2 | 1,200 | 0.2 ms | |
| Night vision and thermal | n/a (post process) | 1 | n/a | 0.3 to 0.5 ms | Full screen cost. Only one active at a time. |
| Nuclear detonation | up to 20,000 | 1 | 20,000 | up to 3 ms | Cinematic exception, all other VFX scaled down while it plays. |

Budget rules:

- Use Niagara **effect types** to set per-type instance limits, max distance, and cull rules (see links above).
- Define three scalability tiers (Low, Medium, High) per system. Low drops secondary layers first (smoke, then debris, then light).
- Prefer GPU simulation for large counts and CPU simulation for small, gameplay-linked effects.
- Use flipbook textures and cheap materials. Limit translucent overdraw. Avoid per-particle lights. Use a small, capped pool of dynamic lights.
- Test with a stress scene: 300 units, 20 percent firing, 10 active fires and 6 impacts per second.
- Ground decals (craters, scorch) capped at about 200 and fade out.

## 3. Sourcing: marketplace vs in-house

Fab replaced the Unreal Engine Marketplace as Epic's content store: https://www.fab.com. Licence terms on Fab allow use inside a shipped game but not resale of the raw assets. Read the specific listing for engine version support (UE 5.5), Niagara vs Cascade, and whether textures are included.

| Option | Pros | Cons | Rough cost |
|---|---|---|---|
| Fab effect packs, for example search "Niagara explosions", "Niagara fire smoke", "military VFX" at https://www.fab.com | Fast, good starting point, many variations | Often over budget for 300 units, look inconsistent across packs, need re-tuning and LOD work | USD 20 to 150 per pack, often discounted |
| Epic free content, for example the Niagara example content in the Unreal Engine launcher, and Epic's free monthly Fab content: https://www.fab.com/sellers/Epic%20Games | No cost, official | Limited military content | Free |
| Unreal Engine Content Examples (Niagara) https://dev.epicgames.com/community/learning | Learning and reusable pieces | Not game-ready | Free |
| Flipbook and texture sources, for example https://www.fab.com and https://ambientcg.com (CC0 materials) | Cheap base textures | Need processing into flipbooks | Free to USD 50 |
| Houdini or EmberGen for baking own flipbooks: https://www.sidefx.com and https://jangafx.com/software/embergen | Matches our style, fully owned | Needs a VFX artist and time | EmberGen roughly USD 15 to 40 per month, Houdini Indie about USD 269 per year (check current price) |
| In-house Niagara artist | Consistent and optimised | Slower to start | 4 to 8 weeks for the P0 set |

Recommendation:

1. Buy or collect one or two well-reviewed Fab packs for P0 effects (muzzle flash, impacts, smoke) and use them as a base. Budget about USD 200 to 500.
2. Re-tune everything to the budget table, and re-skin so the look is consistent.
3. Build in-house: night vision and thermal post process, IED blast, vehicle destruction stages, rotor wash, nuclear detonation. These depend on game rules and need custom parameters.
4. Keep a licence log (pack, URL, licence, price, date) in the same spreadsheet as audio.

## 4. Schedule outline

| Phase | Work | Effort |
|---|---|---|
| 1 | P0 set: muzzle, tracers, vehicle smoke, artillery and rocket impact, destruction stages, IED blast | 3 to 4 weeks |
| 2 | Budget pass and stress scene, tune tiers | 1 week |
| 3 | P1 set: grenade smoke, burning, dust and sandstorm, missile, secondary, ship damage, rotor wash, night vision, thermal | 4 to 6 weeks |
| 4 | P2 set: drone effects, nuclear detonation cinematic, polish | 2 to 3 weeks |

## 5. Open questions

- Target hardware spec for the 60 and 30 fps budgets.
- Final combat event names in `UHavocEventSubsystem`.
- Art direction (realistic vs stylised) so packs can be chosen.
- Tone and accessibility review for the strategic deterrence sequence.
