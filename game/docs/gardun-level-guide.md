# Gardun level build guide (Unreal Engine 5.5)

The shared greybox map for Episode 02 "Silent Corridor" and Episode 03 "Broken Citadel" (`data/maps/gardun-city.json`, map id `gardun_city`, layout contract in `docs/gardun-map.md`). Gardun is fictional. Same method as the Kharg level (`kharg-level-guide.md`).

## Build it

1. Heightmap and terrain mesh (not committed). Any machine with Python 3:
   `python -m pip install rasterio numpy pillow` then `python game/Content/Python/make_gardun_heightmap.py --download`
   Source: Copernicus DEM GLO-30, free and open licence, recorded in `Content/Maps/Gardun/SOURCES.txt`. Use `--dem file.tif` for your own DEM. Format notes: <https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine>
2. Build the C++ project once so the `AHavoc...` classes exist: <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-cpp-code-editor-in-unreal-engine>
3. Run `Content/Python/build_gardun_level.py`: in the editor `Tools > Execute Python Script...`, headless on Windows `game/scripts/run-build-gardun-level.ps1`, or on paktia-pc with the **Build Gardun level** workflow (Actions tab). Docs: <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>
4. Read `[GardunBuilder]` lines in the Output Log. Press `P` in the viewport to see the NavMesh.

The run fails (and CI commits nothing) unless every sector id in the JSON has exactly one `HavocSectorVolume`. Re-running is safe: everything it spawns is tagged `HavocGardunBuilder` and rebuilt.

## What it places

- **City (Ep03):** 10 sector volumes (Mountain Fort, West Ridge, Old Gardun, East Power, Citadel, Government Quarter, Airfield, Dry Riverbed, Industrial Ward, South Gate), 13 objective blocks, the city riverbed with three crossings (bridge, west ford, east crossing), citadel walls with the Outer Gate gap, and portals for the underground routes in the JSON.
- **Valley corridor (Ep02):** road from Checkpoint Echo to South Gate, Dry Riverbed, riverbed bridge, drainage tunnel (infantry only), ambush narrows, Relay Kilo and corridor-end markers, South Gate staging yard. Corridor zones (`corridor_*`) are plain trigger boxes, because each sector id may have only one volume.
- **Common:** real-DEM terrain mesh with walkable collision, sun/sky/fog, a NavMesh bounds volume over city and corridor, player starts (Echo and South Gate), a convoy vehicle, drone and helicopter, and the `HavocLevelDirector` (`gardun_city`, default mission Episode 02; change `Mission Id` for Episode 03).
- **Always loaded:** every gameplay actor has *Is Spatially Loaded* off, so World Partition never streams it out.

## Coordinates

Unreal cm, +X east, +Y south (north is -Y), origin at the centre of the 2.5 km core. City positions come from `play/src/gardun.ts` (1 unit = 4 m). Corridor positions come from the Episode 02 metres and are turned 180 degrees about South Gate so the convoy drives north into the city (formula in the script header). Terrain covers x -3500 to 3500 m, y -2500 to 4500 m.

Greybox only. Art and audio are issue #71.
