# Kharg Island level build guide (Unreal Engine 5.5)

For Episode 04, "Operation Open Strait": the Kharg Island beach landing (`data/maps/kharg-hormuz.json`, map id `kharg_hormuz`). Written for someone new to Unreal. The game code for the level lives in `game/Source/MidnightHavoc/World/`.

This is a hypothetical scenario on real geography (`real_world_hypothetical`). The level is game layout, not a survey. Do not model real equipment or targeting data. Aegis stays abstract.

Official docs home: <https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-5-documentation>

## Before you start

- Install UE 5.5 from the Epic Games Launcher: <https://dev.epicgames.com/documentation/en-us/unreal-engine/installing-unreal-engine>
- Build the C++ project once so the `AHavoc...` classes appear in the editor: <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-cpp-code-editor-in-unreal-engine>
- Learn the editor basics (viewport, Content Browser, Details panel): <https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-editor-interface>

## Step 1. Enable the Water plugin; confirm Lumen and Nanite

1. `Edit > Plugins`, search "Water", tick **Water**, restart the editor. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine> and <https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-plugins-in-unreal-engine>
2. `Edit > Project Settings > Engine > Rendering`:
   - **Dynamic Global Illumination Method** = Lumen, **Reflection Method** = Lumen. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine>
   - Nanite is on by default in 5.5. Confirm by opening a static mesh and checking **Enable Nanite Support**. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine>
3. Water needs **Support Global Clip Plane for Planar Reflections** off and, for Lumen, **Hardware Ray Tracing** optional. Keep defaults unless the water looks wrong.

## Step 2. Make a World Partition level

1. `File > New Level`, choose **Open World** (it is World Partition enabled). Save as `Content/Maps/Kharg/L_KhargHormuz`.
2. Open `Window > World Partition > World Partition Editor` to see the grid. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine>
3. Turn on **One File Per Actor** (default) so teammates can edit the level together: <https://dev.epicgames.com/documentation/en-us/unreal-engine/one-file-per-actor-in-unreal-engine>

## Step 3. Build the Kharg Island landscape from public elevation data

Use a free source. Copernicus DEM GLO-30 or NASA SRTM both work.

1. **Download.**
   - Copernicus DEM (30 m): <https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM>
   - SRTM (30 m) via USGS EarthExplorer: <https://earthexplorer.usgs.gov/>
   - Kharg Island is near 29.25° N, 50.32° E. Download the tile(s) covering roughly 29–30° N, 50–51° E, and the Gulf waters to the west and south.
   - Check the licence on the download page and keep a note of the source and licence in `Content/Maps/Kharg/SOURCES.txt`.
2. **Prepare.** Open the GeoTIFF in QGIS (free, <https://qgis.org>), clip to about 20 x 20 km around the island, and export as a **16-bit PNG** or **R16 RAW** heightmap. Sea level should be a flat low value. Heightmap format notes: <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine>
3. **Import.** `Landscape Mode (Shift+2) > New > Import from File`, pick the heightmap, choose a size from the recommended resolutions list, and set **Scale** so the island matches the map JSON's play area (the island is about 8 km long, so Z scale 100 is a good start and tune by eye). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-quick-start-guide-in-unreal-engine> and <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-outdoor-terrain-in-unreal-engine>
4. **Sculpt/paint.** Smooth the coast, then paint sand and rock layers with the Landscape paint tool (Step 5 supplies the materials). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine>
5. Game space is simplified. Put the north and west beaches, the airstrip, the terminal and the town where the table at the end says.

## Step 4. Add the Gulf with a Water Body Ocean

1. `Place Actors > Water > Water Body Ocean` (needs Step 1). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine>
2. Set its location so the water surface is at the landscape's sea level. Scale the spline/collision to cover the sea around the island.
3. Add a **Water Zone** actor over the island and ocean and set its extent. The Water Zone manages the water mesh and landscape interaction.
4. If the island does not cut the water, enable the landscape as a water brush: <https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine#waterbrush>

## Step 5. Real art from Fab and Quixel Megascans (no placeholders)

1. Install the **Fab** plugin or use the web store: <https://www.fab.com/>. Quixel Megascans are free for Unreal projects through Fab: <https://dev.epicgames.com/documentation/en-us/fab/fab-documentation>
2. Add free assets to your Fab library, then import them from `Window > Fab` into the project: <https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine>
3. Suggested searches (pick free, correctly licensed items): Megascans desert sand, gravel, rock cliff and boulder sets; coastal sand; scrub and dry grass; industrial pipes, storage tanks, shipping containers, pier and jetty pieces, and fencing.
4. Use Nanite meshes where the asset supports it. Keep a list of what you used and its licence in `Content/Maps/Kharg/SOURCES.txt`.
5. Do not use grey boxes or engine shapes for the final level. For layout only, use the Fab assets directly from the start.
6. Build the terminal (jetties, tank farm, pump station) out of the industrial assets. Generic and abstract: no real equipment or markings.

## Step 6. Place sectors, terminal parts, lane hazards and tanker splines

Open `Place Actors`, search "Havoc" (or drag from `C++ Classes > MidnightHavoc > World`). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/placing-actors-in-unreal-engine>

**Sector volumes.** Place one `HavocSectorVolume` per row of the table below. Select it, in Details set **SectorId** to the exact id, and resize the box with the scale tool so it covers the area. Name the actor after the id so it is easy to find in the Outliner. A volume publishes `sector:enter` and `sector:exit` when a player or unit crosses it.

**Terminal parts.** Place four `HavocTerminalPart` actors inside `kharg_oil_terminal`, and set **PartId** to `east_jetty`, `sea_island_jetty`, `tank_farm`, `pump_station`. Attach the matching mesh as a child. Damage taken (or `ApplyTerminalDamage`) is forwarded to the Episode 04 terminal integrity.

**Lane hazards.** Place `HavocLaneHazard` actors in the water of each lane: four with **LaneId** `inbound` and three with `outbound` (the Episode 04 defaults are 4 and 3). Give each a unique **HazardId** such as `inbound_1`. They are hidden in game until a drone calls **Reveal**; a clearance team calls **Clear**. Add a visible marker mesh as a child (a buoy-like abstract hazard marker).

**Tanker splines.** Place one `HavocTankerSpline` per lane, **LaneId** `inbound` or `outbound`. Edit the spline points so the route runs along the lane through the Strait. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/blueprint-spline-components-overview-in-unreal-engine>

### Sector table (every id in `data/maps/kharg-hormuz.json`)

| Sector ID | Name in JSON | Role | What to place there |
|---|---|---|---|
| `task_force_sea` | Amphibious task force, offshore | attacker_staging | A large volume over open Gulf water off the north-west coast. Landing craft spawn here. Nothing else. |
| `kharg_north_beach` | Kharg Island north beach | landing | A volume over the north shore sand and surf line. Beach landscape paint, rocks, scrub. |
| `kharg_west_beach` | Kharg Island west beach | landing | A volume over the west shore. Same dressing as north, with a different layout. |
| `kharg_airstrip` | Kharg airstrip | air_support | A volume over a flat strip inland. Runway, hangar-style sheds, fencing. |
| `kharg_oil_terminal` | Kharg oil terminal and jetties | protected_infrastructure | A volume over the whole terminal. The four `HavocTerminalPart` actors, tanks, pipes, jetties. Protected zone: no targeting. |
| `kharg_town` | Kharg town | protected_civilian | A volume over the town. Houses, streets, vegetation. Protected zone: no targeting. |
| `hormuz_inbound_lane` | Strait of Hormuz inbound lane | shipping_lane | A long water volume along the inbound lane, the `inbound` `HavocLaneHazard` actors and the inbound `HavocTankerSpline`. |
| `hormuz_outbound_lane` | Strait of Hormuz outbound lane | shipping_lane | A long water volume along the outbound lane, the `outbound` `HavocLaneHazard` actors and the outbound `HavocTankerSpline`. |

Surprise events in the JSON (`sandstorm`, `communications_outage`, `nightfall`) need no placed actor. `nightfall` is covered by Step 8.

## Step 7. Place one Level Director

1. Place a single `HavocLevelDirector` anywhere (for example by the player start). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/placing-actors-in-unreal-engine>
2. Defaults are **MapId** `kharg_hormuz`, **MissionId** `episode_04_open_strait`, **StartMissionOnBeginPlay** on. Leave them for this level.
3. Add a **Player Start** on the task-force side. Set the level's GameMode in `World Settings` to the project GameMode: <https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-a-game-mode-in-unreal-engine>

## Step 8. Sun, sky and fog for day/night

1. Add a **Sun Sky** actor (`Place Actors > Visual Effects`), or a Directional Light + Sky Atmosphere + Sky Light + Volumetric Clouds. Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine>
2. Use the Sun Sky time of day (or the `Ultra Dynamic Sky`-style Blueprint you prefer) so the `nightfall` surprise can move the sun below the horizon. For a day/night Blueprint, rotate the Directional Light over time: <https://dev.epicgames.com/documentation/en-us/unreal-engine/directional-lights-in-unreal-engine>
3. Set the Sky Light to **Real Time Capture** so lighting follows the sun: <https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine>
4. Add an **Exponential Height Fog** actor, raise **Fog Density** slightly for desert haze, and enable **Volumetric Fog**: <https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine>

## Step 9. Press Play and check the log

1. Press **Play**. Open `Window > Developer Tools > Output Log`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine>
2. Filter for `LogHavocWorld`.
3. Good result: `Map 'kharg_hormuz': all 8 sectors have a placed volume`, and no `Error` lines from the mission start.
4. Problems it reports, and the fix:
   - `sector '<id>' (...) has no AHavocSectorVolume placed`: place that volume (see table).
   - `placed sector volume '<id>' is not in the map definition`: a typo in **SectorId**.
   - `has no SectorId`: fill in **SectorId**.
   - `has N volumes placed`: delete the duplicate.
   - `Map '...' not found in the data subsystem`: the game data folder is not found. See `game/README.md`.
   - `Mission '...' failed to start`: read the mission log lines above it.
5. Walk a character into a volume and look for the `sector:enter` event (subscribe in a Blueprint through the event subsystem, or watch the HUD).

## Checklist

- [ ] Water plugin on; Lumen and Nanite confirmed
- [ ] World Partition level saved
- [ ] Landscape from a public DEM, source and licence noted
- [ ] Water Body Ocean and Water Zone
- [ ] Real Fab / Megascans assets, no placeholders
- [ ] 8 sector volumes, 4 terminal parts, 7 lane hazards, 2 tanker splines
- [ ] One level director
- [ ] Sun, sky, height fog
- [ ] Play: no missing-sector errors
