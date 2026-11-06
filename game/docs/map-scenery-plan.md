# Map scenery plan: rivers, fountains, mountains (issue #74)

Status: **plan and asset list only.** Nothing here is built in the editor yet. Only the Kharg map
exists in the repo (`Content/Maps/Kharg`), and building water, Niagara and landscape needs the
Unreal Editor on paktia-pc. Per Shaheen's go-ahead (comment on #70, relayed on #74) this plan is
committed without waiting for an "approve" comment. Build one PR per map, and do not merge until
play-tested on Shaheen's PC.

Help (Epic docs):
- Water system: <https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine>
- Landscape: <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-outdoor-terrain-in-unreal-engine>
- Niagara: <https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-visual-effects-in-unreal-engine>
- World Partition: <https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine>
- HLODs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine>
- Fab in UE: <https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine>

## Where each feature goes

| Map | Rivers | Fountains | Mountains |
| --- | --- | --- | --- |
| Paktia Highlands (Ep01, `first-light-highlands`) | Mountain stream (Water Body River) in the valley below Ridge Station | none | Landscape range around Checkpoint Echo and Ridge Station; rock and snow-line materials, cliffs, scree, scrub; distant peaks as far-distance mesh or skybox |
| Gardun (Ep02-03, `gardun-city`) | River under the riverbed bridge (Ep02 bridge-obstruction choice); seasonal stream with shallow water along the Dry Riverbed (`riverbed`); river through the city's arched bridges | Main square; Medical Station courtyard (protected zone) | Ranges framing the valley corridor; Mountain Fort (`mountain_fort`); West Ridge for the radio site |
| Kharg (Ep04, `kharg-hormuz`) | No river. Tune the Water Body Ocean shoreline and waves (`build_water` in `build_kharg_level.py`) | One small square fountain in Kharg town | Low rocky inland hills; Zagros coast on the horizon (far-distance mesh) |
| Parvaneh (Ep07, `city-of-freedom`) | Arched river bridges river | Domed-square fountain | later, when the map is built |

## How each is built

**Rivers.** `WaterBodyRiver` actors (Water plugin, already enabled) drawn as splines following the
landscape, with flow, foam and shoreline blending, plus a looping running-water sound on an
`AmbientSound` per reach. The Water Zone from `build_water` is extended to cover each river.
Gameplay: a `HavocWaterDepthVolume` (to be added in C++) per body marks **shallow** (slows infantry
and vehicles) or **deep** (blocks vehicles). Nav mesh gets matching nav modifier volumes: shallow =
higher area cost, deep = vehicle-blocked. Verify with `P` (nav debug) in the editor.

**Fountains.** One Blueprint `BP_Fountain`: basin mesh, a small water-surface material plane,
Niagara spray and splash systems, a looping ambient sound. Scenery only: it takes damage FX but has
no health and is not registered as a target (no `HavocTargetable` tag). The Medical Station fountain
sits inside its protected-zone volume.

**Mountains.** Landscape with a real heightmap (Copernicus DEM or SRTM, same pipeline as
`make_kharg_heightmap.py`; record tile ids and licence), a layered rock / scree / snow-line landscape
material blended by slope and height, rock cliffs and scrub scattered with foliage tools. Far peaks
are a low-poly far-distance mesh or skybox. World Partition streaming plus HLODs keep long views fast.

## Assets (free only)

Per Shaheen: free assets only, paid items skipped. Fab links are searches so they never go stale;
confirm the price reads **Free** and read the licence box before adding. Add to
`Config/HavocAssets.ini` role keys after import (see [fab-assets.md](fab-assets.md)).

| Need | Source | Licence | Cost | Needs Shaheen's Epic account? |
| --- | --- | --- | --- | --- |
| Water Body River / Ocean, Water Zone | Unreal Engine Water plugin (built in) | Engine EULA | Free | No |
| Niagara water spray / splash | Niagara plugin (built in) + [free water FX on Fab](https://www.fab.com/search?q=water%20fountain%20niagara%20free) | Fab Standard | Free listings only | **Yes** (add to library) |
| Rock cliffs, scree, boulders | [Megascans rocks on Fab](https://www.fab.com/search?q=megascans%20rock%20cliff) | Megascans free for UE | Free | **Yes** |
| Rock / snow / gravel surfaces | [Megascans surfaces](https://www.fab.com/search?q=megascans%20mountain%20rock%20surface) or Poly Haven CC0 (<https://polyhaven.com/textures>) | Megascans / CC0 | Free | Megascans: **Yes**. Poly Haven: No |
| Scrub and dry plants | [Desert scrub on Fab](https://www.fab.com/search?q=desert%20scrub%20plants&listing_types=3d-model) | Fab Standard / Megascans | Free | **Yes** |
| Fountain basin / statue mesh | [Fountain on Fab](https://www.fab.com/search?q=fountain%20free&listing_types=3d-model), or a custom Blender model ([custom-models.md](custom-models.md)) | Fab Standard, or ours | Free | **Yes** for Fab; No for custom |
| Running water, fountain audio | [Free water sounds on Fab](https://www.fab.com/search?q=water%20stream%20sound%20free&listing_types=audio) or Freesound CC0 (<https://freesound.org/search/?q=river&f=license%3A%22Creative+Commons+0%22>) | Fab Standard / CC0 | Free | Fab: **Yes**. Freesound: No |
| Heightmaps (Highlands, Gardun, Kharg hills) | [Copernicus DEM](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM) or [SRTM via USGS EarthExplorer](https://earthexplorer.usgs.gov/) | Copernicus free licence / USGS public domain | Free | No (their own free accounts) |

Record each imported asset's source, link and licence in `Content/Custom/SOURCES.txt` or
`fab-assets.md` before merging. Marketplace assets stay out of git (`Content/ThirdParty/`,
`Content/Megascans/` are ignored).

## Items for Shaheen (Epic account)

Anything marked **Yes** above: add the free Fab / Megascans listings to the Epic library, then
**Add to Project**. Nothing paid is in this plan; any listing that shows a price is skipped.

## Order of work (one PR per map)

1. Kharg: ocean shoreline and wave tuning, low inland hills, horizon coast, small fountain.
2. Paktia Highlands (Ep01): mountain range and valley stream.
3. Gardun (Ep02-03): rivers, mountains, two fountains.
4. Parvaneh (Ep07): when that map exists.

## Done when (per map)

- [ ] Feature placed, with help text and links on any new actor or Blueprint
- [ ] Shallow / deep water volumes match the nav mesh
- [ ] Fountains are not targetable
- [ ] World Partition and HLODs build; long view holds frame rate
- [ ] Sources and licences recorded
- [ ] Play-tested on Shaheen's PC
