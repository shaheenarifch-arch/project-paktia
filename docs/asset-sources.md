# Asset sources and licences (issue #71)

Every production asset is recorded here (or in `game/Content/Maps/*/SOURCES.txt`). Free only; paid
items are listed under "Needs Shaheen" and are not bought. Real art and recordings only.
Fab packs go in `game/Content/ThirdParty/` (git-ignored, no redistribution). Mapping:
`game/Config/HavocAssets.ini` (roles) and `game/Content/Fab/fab_manifest.json` (Kharg builder slots).
Pack checklist and links: [`game/docs/fab-assets.md`](../game/docs/fab-assets.md).

## Status

| Area | State |
| --- | --- |
| Lumen GI + reflections | Already on in `DefaultEngine.ini` |
| Graphics presets Low/Medium/High/Epic | Already in `HavocPlaySettings` (`SetOverallScalabilityLevel`) |
| `fab_manifest.json` | Added (expected paths, to be confirmed after import) |
| Fab packs (guns, characters, vehicles, props, VFX, ambience) | Not imported: needs Shaheen's Epic account |
| Audio (free sources below) | Not downloaded or imported yet |

## Needs Shaheen (Epic account / approval)

- Click **Add to My Library**, then **Add to Project**, for every free pack in `game/docs/fab-assets.md`
  (incl. the 2026-11-04 list). Bhaddru cannot sign in to Fab. Then send the exact asset names.
- Check each listing's licence. Packs on the free **Personal** licence (Pistol 9mm Custom, Rock
  Environment Pack, Soviet Assault Rifle, Perfect Fire VFX, Procedural Wind) are non-commercial.
- Skipped (paid, needs approval): Boom Library and other paid SFX packs, paid music, voice actors
  or licensed AI voices (options and costs in `docs/audio-voice-plan.md`).

## Free audio sources (SFX, ambience, music)

| Source | Link | Licence | Cost | Rule |
| --- | --- | --- | --- | --- |
| Sonniss GDC Game Audio Bundles | https://sonniss.com/gameaudiogdc | Royalty-free commercial game use, no resale; keep each year's licence PDF | Free | Weapons, explosions, vehicles, ambience |
| Freesound | https://freesound.org | Per file: CC0 or CC-BY only; no CC-BY-NC | Free | Record file URL and author for CC-BY |
| Pixabay Music / Sound | https://pixabay.com/music/ | Pixabay Content Licence; check each item | Free | Menu/briefing/combat music candidates |
| Kevin MacLeod (incompetech) | https://incompetech.com | CC-BY 4.0, credit required | Free | Music candidates |
| BBC Sound Effects | n/a | Non-commercial | n/a | **Never use** |

Imported audio rows (add one per file when imported): file, source URL, author, licence, date.

| File | Source URL | Author | Licence | Date |
| --- | --- | --- | --- | --- |
| (none yet) | | | | |
