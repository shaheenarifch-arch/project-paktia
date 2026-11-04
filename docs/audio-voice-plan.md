# Audio and Voice Production Plan

Scope: battlefield audio and voice for Operation Midnight Havoc (design doc section 3, "Battlefield presentation and audio"). This is a planning document only. Costs are rough estimates and must be re-quoted before purchase. Licence terms change, so read the current licence text before buying or downloading anything.

## 1. Sound sourcing

Rule: shipped builds use real recordings, either from licensed libraries or our own field recordings. No synthesized placeholders in shipped builds. Temporary stand-ins are allowed in development builds only and must be tracked and removed before release.

Categories needed: small arms, machine guns, artillery and rocket launches and impacts, vehicles (wheeled, tracked, armoured), fixed-wing aircraft, helicopters, drones, ships and boats, explosions, debris, ambience (desert, mountain, coast, urban).

### 1.1 Free options

| Source | Link | Licence notes (check current terms) | Cost |
|---|---|---|---|
| Sonniss GDC Game Audio Bundles | https://sonniss.com/gameaudiogdc | Royalty-free for commercial game use. Cannot be resold or redistributed as a sound library. Each year's bundle has its own licence PDF; keep a copy. | Free |
| Freesound | https://freesound.org | Per-file licences (CC0, CC-BY, CC-BY-NC). Use only CC0 or CC-BY, and record attribution. Avoid NC. | Free |
| BBC Sound Effects | https://sound-effects.bbcrewind.co.uk | RemArc licence is for personal, educational or research use. **Not suitable for a commercial game.** Listed so nobody uses it by mistake. | n/a |
| Own field recordings | n/a | We own them. Needs location permission and legal range access. | Equipment and time |

### 1.2 Paid libraries

| Source | Link | Licence notes | Rough cost |
|---|---|---|---|
| Sound Ideas / Pro Sound Effects | https://www.prosoundeffects.com | Usually royalty-free for game use, no resale. Subscription or per-library. | USD 100 to 1,500 per library |
| Boom Library | https://www.boomlibrary.com | Royalty-free, game use allowed, no resale of raw files. Strong weapons and explosions packs. | USD 50 to 600 per pack |
| A Sound Effect (marketplace) | https://www.asoundeffect.com | Many vendors, each with its own licence. Check each. | USD 20 to 500 per pack |
| Soundly / Epidemic Sound SFX | https://getsoundly.com | Subscription with commercial licence terms. Confirm game and redistribution rules. | About USD 10 to 25 per month |
| Fab (formerly Unreal Marketplace) audio packs | https://www.fab.com | Fab standard licence allows use in a shipped product, not resale of the assets. Some packs are synthesised, so read the description. | USD 20 to 200 per pack |

Rough starting budget: USD 1,500 to 4,000 for a core paid set (weapons, explosions, vehicles, aircraft), plus the free bundles.

### 1.3 Own field recordings

- Recorder, shotgun and stereo microphones, windshields, spare batteries. Approx USD 800 to 2,500 to buy, or rent.
- Record only vehicles, aircraft flyovers, boats, generators and legal range sessions with licensed operators. Follow local law and range safety rules.
- Capture at 96 kHz / 24-bit, with close, mid and far distance takes, and room tone for each location.

### 1.4 Sourcing rules

- Keep an `audio-licences` spreadsheet: file or pack, source URL, licence, date, price paid, receipt.
- Check the licence allows use in a commercial game and in trailers.
- No ripping from films, games or video sites.
- Credits file lists any CC-BY sources.

## 2. Audio system design (Unreal Engine 5.5)

### 2.1 Building blocks

- **MetaSounds** for procedural layering: random one-shot variation, pitch and volume jitter, looped engine layers driven by parameters (RPM, speed, load). Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/metasounds-in-unreal-engine
- **Sound Attenuation** assets for distance falloff, spatialization and air absorption. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-attenuation-in-unreal-engine
- **Sound Classes, Submixes and Sound Mix** for buses (weapons, vehicles, voice, ambience, UI) and ducking. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/audio-mixer-overview-in-unreal-engine
- **Submix effects** for filtering and reverb. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/submixes-in-unreal-engine
- **Audio Occlusion** (line traces plus low-pass filter). Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-attenuation-in-unreal-engine (occlusion section)
- **Concurrency** to cap simultaneous sounds. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-concurrency-in-unreal-engine
- **Audio Modulation** for control buses and mix states. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/audio-modulation-overview-in-unreal-engine

### 2.2 Blast sounds and distance delay

Sound travels about 343 m/s, so a blast 686 m away is heard about two seconds after it is seen. Plan:

- Spawn the visual immediately. Delay the audio by `distance / 343` seconds, clamped to a maximum (suggest 3 s) so the game stays readable.
- Use a MetaSound with near, mid and far layers. Near has sharp crack and punch. Far has low rumble with high frequencies removed. Crossfade by distance.
- Attenuation curve: log-reverse for explosions, with low-pass filter rising with distance (air absorption).
- Add a short low-frequency thump for very close blasts, and a ducking effect on other buses plus a ringing tone when the player camera is very near a large blast. Keep this optional (accessibility setting).

### 2.3 Radio filtering

- Radio voice goes through a "Radio" submix: band-pass roughly 300 Hz to 3 kHz, light distortion, optional noise bed and squelch click at start and end of transmission.
- Quality varies by faction and range: coalition clean digital, others noisier.
- Original recordings stay clean. Filtering is applied at runtime so one asset serves several uses.

### 2.4 Occlusion

- Line trace from source to listener. Blocked by buildings, terrain or hills raises the low-pass filter and lowers volume.
- Indoor or enclosed spaces switch reverb submix preset (open desert, urban, interior, ship hold).
- Throttle traces (for example 5 per second per source, with a global cap) given the 300-unit target.

### 2.5 Hooking into `UHavocEventSubsystem`

`UHavocEventSubsystem` (see `game/README.md`) mirrors the TypeScript `EventBus` in `src/core/EventBus.ts`. Today the documented events are `mission:state`, `mission:blocked`, `mission:checkpoint` and `mission:complete`. Combat events below are **proposed names** and need to be agreed with the code owners before implementation.

Approach:

1. Create an `UHavocAudioSubsystem` (UWorldSubsystem) that subscribes to the event subsystem on startup.
2. A data table maps event name to a MetaSound or Sound Cue, a bus, a priority and a concurrency group. Designers edit the table, not code.
3. Events carry position and type, so the audio subsystem calls "play at location" with the distance delay from 2.2.

| Event (existing or proposed) | Audio response |
|---|---|
| `mission:state`, `mission:checkpoint` | Music state change, radio call |
| `mission:complete` | Stinger, debrief radio |
| `mission:blocked` | Short UI or radio warning |
| `combat:shot` (proposed) | Weapon one-shot, tail by environment |
| `combat:impact` (proposed) | Impact and blast layers, delayed |
| `unit:destroyed` (proposed) | Destruction sound, secondary explosions |
| `unit:casualty` (proposed) | Casualty voice call |
| `threat:ied` (proposed) | Blast, then radio contact report |
| `strategic:deterrence` (proposed) | Dedicated sequence (see VFX plan) |

Voice lines also use the event: pick a category (section 3), pick a random line not played recently, apply a per-category cooldown so chatter does not spam.

## 3. Voice plan

### 3.1 Factions and languages

| Faction | Spoken language | Notes |
|---|---|---|
| Coalition | English | Mixed accents (US, UK, plus a few other English-speaking accents) |
| Iran | Persian (Farsi) | Native speakers only |
| Houthi | Yemeni Arabic | Native speakers, dialect checked by a consultant |
| ISIS | Iraqi / Levantine Arabic | Native speakers, dialect checked by a consultant |

- Subtitles for all lines, in all supported UI languages. Subtitles are on by default for non-English speech.
- Hostile lines should be short, procedural and non-propagandistic. No hate speech, no slogans, no religious chanting used as a gimmick.
- Treat all factions with respect in tone. Enemies are written as soldiers, not caricatures.

### 3.2 Casting and advice

- Cast professional voice actors through agencies or casting services (for example https://www.voices.com, https://www.backstage.com, or local agencies). Use signed contracts that cover game use, and consent for any AI or synthetic use is **not** assumed (do not use synthetic cloning of actors).
- Hire a **military adviser** (ex-service, per faction where possible) to review scripts for procedure, terminology and tone. Rough cost: USD 300 to 700 per day.
- Hire native-language directors or consultants for Persian and Arabic sessions, plus translators for the subtitles.
- **No impersonation of real people.** No real politicians, commanders, or public figures; no sound-alike casting. No real call signs, unit names or real radio frequencies.

### 3.3 Line categories and counts

Counts are unique lines for the coalition. Each hostile faction gets about 60 percent of the coalition count, because there are fewer unit types. Multiple actors per faction (suggest 4 coalition voices, 3 per other faction) share the pool.

| Category | Coalition lines | Per other faction |
|---|---|---|
| Radio net calls | 60 | 36 |
| Orders | 80 | 48 |
| Contact reports | 80 | 48 |
| Suppression | 40 | 24 |
| Casualty calls | 40 | 24 |
| Medevac / rescue requests | 30 | 18 |
| Convoy chatter | 50 | 30 |
| Helicopter crew calls | 50 | 30 (where the faction has helicopters) |
| Drone operator calls | 40 | 24 (where the faction has drones) |
| Amphibious landing calls (Marines) | 40 | n/a |
| **Total** | **510** | roughly 280 each |

Rough total: about 510 coalition lines plus 3 x 280 = 840 lines, about 1,350 lines before variations. Allow 2 to 3 takes per line.

### 3.4 Example coalition lines

All are generic procedure-style lines. No real call signs, units or locations. They are placeholders for the writers and can be changed.

**Radio net calls**
1. "Radio check, how do you read?"
2. "Loud and clear."
3. "Say again your last."
4. "Stand by."
5. "Roger, out."
6. "Net is busy, hold your traffic."
7. "Break, break. Priority message."
8. "All stations, check in."
9. "Wilco."
10. "Copy all."
11. "Going to alternate channel."
12. "Comms check complete."

**Orders**
1. "Move to the next waypoint."
2. "Hold this position."
3. "Fall back to the road."
4. "Take cover."
5. "Advance on my mark."
6. "Cease fire."
7. "Weapons free."
8. "Form up on the lead vehicle."
9. "Secure the building."
10. "Regroup at the rally point."
11. "Watch your sectors."
12. "Push forward, keep spacing."

**Contact reports**
1. "Contact front, two hundred meters."
2. "Contact left, infantry in the open."
3. "Enemy vehicle, east side, moving."
4. "Contact rear, one squad."
5. "Multiple contacts, high ground."
6. "Movement in the treeline."
7. "Enemy down."
8. "I've lost visual."
9. "Target destroyed."
10. "They're flanking right."
11. "Hostile aircraft overhead."
12. "Enemy position, second floor."
13. "More coming from the north."

**Suppression**
1. "I'm pinned down!"
2. "Heavy fire, can't move!"
3. "Need covering fire!"
4. "Keep their heads down!"
5. "Suppressing, now!"
6. "Reloading, cover me!"
7. "Stay low, stay low!"
8. "Fire is too heavy!"
9. "Get that gun quiet!"
10. "Cover is breaking up!"

**Casualty calls**
1. "Man down!"
2. "I'm hit!"
3. "Medic!"
4. "We have one wounded."
5. "He's still breathing."
6. "Stay with me."
7. "Two casualties, need aid."
8. "Apply pressure."
9. "Get him to cover."
10. "Status, how bad?"
11. "Vehicle crew injured."

**Medevac / rescue requests**
1. "Requesting medical evacuation, one urgent."
2. "Landing zone is hot, standing by."
3. "Mark the landing zone."
4. "Smoke is out, confirm color."
5. "Evac inbound, two minutes."
6. "Patient is stable."
7. "Request rescue, crew down."
8. "We need extraction at our position."
9. "Landing zone is clear."
10. "Wounded loaded, go, go."
11. "Search team on the way."

**Convoy chatter**
1. "Convoy, keep your spacing."
2. "Slow down, obstacle ahead."
3. "Lead vehicle, report the road."
4. "Vehicle stopped, check for damage."
5. "Halt, halt."
6. "Rolling again."
7. "Checkpoint ahead."
8. "Bridge looks clear."
9. "Last vehicle, report."
10. "Dust is bad, stay close."
11. "Fuel is getting low."
12. "Tail vehicle has a flat."

**Helicopter crew calls**
1. "Rotors turning, ready for departure."
2. "Lifting off."
3. "On approach, landing zone in sight."
4. "Touching down."
5. "Wheels up."
6. "Taking fire, breaking left."
7. "Engine warning, checking."
8. "Low on fuel."
9. "Passengers, get ready."
10. "Climbing to cruise."
11. "Landing zone is too dusty, going around."
12. "Hard landing, check the crew."

**Drone operator calls**
1. "Drone is airborne."
2. "I have eyes on the area."
3. "Link is weak."
4. "Contact on camera, two vehicles."
5. "Battery is low, returning."
6. "Tracking the target."
7. "Lost the feed."
8. "Feed is back."
9. "Area looks clear."
10. "Marking the position on the map."
11. "Drone is down."
12. "Switching to thermal."

**Amphibious landing calls (Marines)**
1. "Boats in the water."
2. "Approaching the beach."
3. "Ramp down, go!"
4. "Beach is hot, keep moving."
5. "Establish the beachhead."
6. "Clear the beach, get off the sand."
7. "Second wave inbound."
8. "Landing craft taking fire."
9. "Beach is secure."
10. "Move inland, form up."
11. "Vehicles coming ashore."
12. "Surf is rough, hold on."

Content note: lines describe calls and reports only. They contain no instructions for operating real weapons or conducting attacks.

## 4. Recording plan

### 4.1 Sessions and cost estimate

Estimates based on about 100 to 150 finished lines per actor per half-day session. Rates are rough and vary by region.

| Session | Scope | Studio days | Rough cost (studio plus talent plus director) |
|---|---|---|---|
| A. Coalition English | 510 lines, 4 actors, 2 to 3 takes | 4 | USD 8,000 to 15,000 |
| B. Persian | about 280 lines, 3 actors | 2 | USD 4,000 to 8,000 |
| C. Yemeni Arabic | about 280 lines, 3 actors | 2 | USD 4,000 to 8,000 |
| D. Iraqi / Levantine Arabic | about 280 lines, 3 actors | 2 | USD 4,000 to 8,000 |
| E. Pickups and fixes | 10 percent of all lines | 1 | USD 2,000 to 4,000 |
| Military adviser and language consultants | Script review, session attendance | about 8 days total | USD 3,000 to 6,000 |
| Translation and subtitle QA | all lines | n/a | USD 2,000 to 5,000 |

Total: about 11 studio days, roughly USD 27,000 to 54,000. Final figures need agency quotes and union (for example SAG-AFTRA, https://www.sagaftra.org) rate checks if union talent is used.

Process: script lock, adviser review, casting, directed sessions with live review of pronunciation, edit, loudness pass, in-engine test, pickups.

### 4.2 File naming

Pattern: `VO_<faction>_<lang>_<category>_<id>_<take>.wav`

- Faction codes: `COA`, `IRN`, `HOU`, `ISL`
- Language codes: `en`, `fa`, `ar-ye`, `ar-iq`
- Category codes: `NET`, `ORD`, `CON`, `SUP`, `CAS`, `MED`, `CNV`, `HEL`, `DRN`, `AMP`
- Example: `VO_COA_en_CON_0012_02.wav`
- Weapon, vehicle and effects files: `SFX_<type>_<name>_<variant>.wav`, for example `SFX_EXP_Artillery_Close_03.wav`.
- Metadata: a CSV keyed by file name with script text, subtitle text, actor, session date, and licence.

### 4.3 Formats and loudness

- Record voice at 48 kHz / 24-bit WAV, mono, in a treated booth with consistent mic distance. Record SFX at 96 kHz / 24-bit when possible.
- Deliver clean, no radio filter baked in.
- Voice loudness target: about -23 LUFS integrated per line is a workable starting point (EBU R128, https://tech.ebu.ch/docs/r/r128.pdf), true peak no higher than -1 dBTP.
- Game mix target: about -23 to -18 LUFS for console and PC, with a quieter option for mobile and headphones. Confirm with the audio lead and platform requirements.
- Keep dialogue within about 2 dB of each other between actors.
- Units are normalised in the mix tools, not by clipping.

## 5. Open questions

- Final names of combat events in `UHavocEventSubsystem`.
- Whether hostile factions need helicopter and drone lines at launch.
- Which languages the UI and subtitles support at launch.
- Budget approval for the paid library set and studio days.
