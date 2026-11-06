# Issue #71 planning: asset and voice shortlist (awaiting approval)

Status: **PLAN ONLY.** Nothing is imported, bought or built. Work starts when Shaheen comments **approve** on #71.

Honesty notes: sizes marked "check" and exact Fab listing URLs must be confirmed on the Fab page when logged in (Fab pages need JavaScript and an Epic login, so Bhaddru cannot read them). Licence summaries are from vendor pages on 2026-11-06; read the current text before use. Costs are USD.

## 1. Free Fab / Quixel packs (Shaheen clicks "Add to project")

Search links (Fab): https://www.fab.com/search?q=<term>. Already added on 2026-11-04 (per `HavocAssets.ini`): drones (Raven, Warden, Talon), cruise missile, RPG launcher, Challenger tank, MK2 grenade, 9mm pistol, revolver pack. Exact asset names still need setting.

| Role(s) | Candidate | Link | Licence | Size | Cost |
|---|---|---|---|---|---|
| `Terrain.Sand`, `Terrain.Rock`, `Props.Rocks`, `Foliage.Scrub` | Quixel Megascans surfaces, rocks, scrub (free subset; Epic reports the full library free on Fab around 5-12 Nov 2026, confirm on the listing) | https://www.fab.com/sellers/Quixel | Fab Standard License (use in shipped product, no resale of raw assets) | check per asset (surfaces 2K/4K, tens to hundreds of MB each) | Free (confirm) |
| Kharg / Paktia / Gardun mountain and desert dressing | Megascans rock cliffs, desert scenes, Unreal-only free scene packs | https://www.fab.com/sellers/Quixel | Fab Standard | check | Free (confirm) |
| `Props.Tank/Pipes/Pier/Container/Fence` | Free industrial / refinery / port props: Fab search "industrial", "oil refinery", "shipping container", "chain link fence" (pick Free filter, UE 5.5) | https://www.fab.com/search?q=industrial%20props&is_free=1 | Fab Standard (check each) | check | Free |
| `Character.Marine` + AnimBP, enemy infantry | Epic's UE5 Mannequin + Game Animation Sample / Lyra (locomotion, aim, hit, death retargeted) as the animation base; free soldier mesh from Fab search "soldier" | https://www.fab.com/search?q=soldier&is_free=1 | Epic content licence for Lyra / Mannequin (UE only); Fab Standard for packs | Lyra content several GB; check | Free |
| `Weapon.KR47`, `FX.Muzzle` | Free rifle (AK/M4-style generic) with animations from Fab search "rifle"; muzzle flash via Niagara packs | https://www.fab.com/search?q=rifle&is_free=1 | Fab Standard | check | Free |
| `Aircraft.Falcon`, `Vessel.SmallBoat`, vehicles (Warden tank, Nomad truck) | Fab free helicopter, boat, truck listings | https://www.fab.com/search?q=helicopter&is_free=1 | Fab Standard | check | Free, else paid listing shortlisted separately for approval |
| `FX.*` (explosions, smoke, water splash, fire, dust, tracers) | Epic free "Niagara Examples / Content Examples" plus free Niagara explosion/smoke packs on Fab | https://www.fab.com/search?q=niagara%20explosion&is_free=1 | Epic content licence / Fab Standard | check | Free |
| Sky, sun, fog, water | UE built-in: SkyAtmosphere, VolumetricCloud, ExponentialHeightFog, Water plugin (no download) | https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine | Part of engine | 0 | Free |

Already custom (Blender, not from Fab): MRL, artillery, enemy tank, IED, Oil Tanker.

## 2. Audio sources (SFX, ambience, music)

| Source | Link | Licence | Size | Cost |
|---|---|---|---|---|
| Sonniss GDC Game Audio Bundle 2024 (9 parts; no 2025 bundle; 2026 has its own page) | https://sonniss.com/gameaudiogdc | Royalty-free, commercial, no attribution, no resale as SFX, no AI/ML training; keep the licence PDF | 23.5 GB (download in parts as needed) | Free |
| Freesound (CC0 and CC-BY only, record attribution) | https://freesound.org | Per file; no CC-BY-NC | small per file | Free |
| Music: Kevin MacLeod / incompetech (CC-BY) | https://incompetech.com | CC-BY, credit required | small | Free |
| Music: Pixabay Music | https://pixabay.com/music/ | Pixabay Content Licence (confirm game use per track) | small | Free |
| Music: Fab / paid cinematic packs (menu, briefing, combat, stingers) | https://www.fab.com/search?q=military%20music | Fab Standard | check | USD 20-100 per pack, **needs approval** |
| Paid SFX if free is not enough: Boom Library | https://www.boomlibrary.com | Royalty-free game use, no resale | check | USD 50-600 per pack, **needs approval** |

Excluded: BBC Sound Effects (non-commercial licence).

Ambience (coast, desert, mountain, urban): Sonniss and Freesound first.

## 3. Voices: two options (Shaheen picks; no spend until approved)

Scope from `docs/audio-voice-plan.md`: Ep01-04 radio chatter, squad callouts, briefings and cutscene lines; subtitles on by default. Full plan is about 1,350 lines in 4 languages. For Ep01-04 only, a first pass of roughly 300-500 English lines is suggested, plus Persian/Arabic only if those factions speak in these episodes.

| | (a) Human voice actors | (b) Licensed AI voice |
|---|---|---|
| How | Agency casting (e.g. voices.com), signed game-use contracts, military adviser review | Commercial plan with a vendor offering stock voices and a commercial licence (e.g. ElevenLabs, https://elevenlabs.io/pricing; or Microsoft/Google TTS). Stock or vendor-licensed voices only: no cloning of real people or actors |
| Cost | Full plan USD 27,000-54,000; coalition English only (session A) USD 8,000-15,000; adviser USD 300-700/day | Roughly USD 20-100/month for the volume needed (vendor prices change, re-quote); about 1 month of subscription for Ep01-04 |
| Quality | Best acting, native Persian/Arabic dialects | Good for radio chatter and prototypes, weaker for emotion and Persian/Arabic dialects; check licence covers shipped game use and keep invoices |
| Time | Weeks (casting, sessions, pickups) | Days |
| Risks | Budget | Licence terms, player perception, no consent for actor voice cloning (per plan) |

Recommendation: AI voice for radio chatter and callouts now (cheap, filtered by the Radio submix anyway), human actors later for key cutscene lines when budget allows. Shaheen decides.

## 4. Free-first order of work after approval (one PR per area)

1. Graphics and lighting: Lumen on (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`), Kharg landscape import, quality presets.
2. Weapons, then 3. characters and vehicles, 4. maps, 5. VFX, 6. audio, voices and music.
Each import is recorded in `game/Content/Maps/*/SOURCES.txt` or `docs/asset-sources.md`, and in `HavocAssets.ini` and `game/Content/Fab/fab_manifest.json`.

## Total cost if everything free is chosen

USD 0. Optional spend, all needing approval: music pack USD 20-100, Boom Library USD 50-600, AI voice about USD 20-100/month, human voice USD 8,000-54,000.
