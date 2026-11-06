"""Build Content/Maps/Gardun/L_Gardun in one run (Unreal Python, UE 5.5). Greybox, shared by Episode 02 and 03.

Run in the editor:  Tools > Execute Python Script...  (or Output Log: py "<path>/build_gardun_level.py")
Run headless:       game/scripts/run-build-gardun-level.ps1
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python

Layout follows docs/gardun-map.md (north is -Y, South Gate at +Y) and the sector ids in data/maps/gardun-city.json;
the script refuses to run if the JSON and the SECTORS table below disagree, so every sector id has exactly one
placed AHavocSectorVolume. Coordinates: Unreal cm, +X east, +Y south, origin = centre of the 2.5 km city core.

Episode 02 (the valley corridor) lies south of the city: Checkpoint Echo -> Relay Kilo -> riverbed bridge ->
drainage tunnel -> narrows -> South Gate staging yard. Corridor positions are the Episode 02 game-space metres
(src/gameplay/missions/episode02/SilentCorridor.ts and play/src/corridor.ts) turned 180 degrees so the convoy
drives north into the city: UE x = SOUTH_GATE.x + (2020 - data.x) * 100, UE y = SOUTH_GATE.y + (2180 - data.z) * 100.

Idempotent: every actor this script spawns is tagged HavocGardunBuilder; a re-run deletes those and rebuilds them.
Gameplay actors (sector volumes, director, starts, vehicles, markers, nav bounds) are never spatially loaded,
so they stay loaded in World Partition at any distance.
Terrain: Copernicus DEM from make_gardun_heightmap.py (source in Content/Maps/Gardun/SOURCES.txt).
Greybox only: plain-colour engine shapes, no art. Art comes later (issue #71).
"""
import json
import os

import unreal

TAG = "HavocGardunBuilder"
LEVEL_DIR = "/Game/Maps/Gardun"
LEVEL_NAME = "L_Gardun"
LEVEL_PATH = LEVEL_DIR + "/" + LEVEL_NAME
HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", ".."))
MAP_JSON = os.path.normpath(os.path.join(GAME_DIR, "..", "data", "maps", "gardun-city.json"))
SOURCE_DIR = os.path.join(GAME_DIR, "Content", "Maps", "Gardun", "Source")
HEIGHTMAP = os.path.join(SOURCE_DIR, "gardun_heightmap_r16.png")
TERRAIN_OBJ = os.path.join(SOURCE_DIR, "gardun_terrain.obj")
TERRAIN_INFO = os.path.join(SOURCE_DIR, "gardun_terrain.json")
TERRAIN_PATH = LEVEL_DIR + "/Terrain"
PLACEHOLDER_DIR = LEVEL_DIR + "/Placeholder"
# Terrain square in cm; must match make_gardun_heightmap.py (X_MIN, Y_MIN, SIZE_M).
T_X0, T_Y0, T_SIZE = -350000.0, -250000.0, 700000.0

U = 400  # play/src/gardun.ts: 1 world unit = 4 m = 400 cm; unit (x, z) -> Unreal (x * U, z * U)
SOUTH_GATE_XY = (0, 272 * U)


def corridor(dx, dz):
    """Episode 02 game-space metres (from Checkpoint Echo) -> Unreal cm, turned 180 degrees about South Gate."""
    return (SOUTH_GATE_XY[0] + (2020 - dx) * 100, SOUTH_GATE_XY[1] + (2180 - dz) * 100)


# sector id -> (centre x, y, half-extent x, y) in units of 4 m (from play/src/gardun.ts). Z is set below.
SECTOR_UNITS = {
    "mountain_fort": (-215, -185, 60, 60),
    "west_ridge":    (-235, 25, 60, 90),
    "old_gardun":    (0, -20, 120, 100),
    "east_power":    (215, -150, 70, 70),
    "citadel":       (0, -230, 70, 60),
    "government":    (150, 40, 60, 55),
    "airfield":      (-200, 190, 75, 45),
    "riverbed":      (0, 109, 312, 9),      # the city stretch of the Dry Riverbed (z 100 to 118 in gardun.ts)
    "industrial":    (190, 185, 70, 50),
    "south_gate":    (0, 272, 45, 30),      # attacker staging yard
}
SECTOR_Z, SECTOR_HZ = 0, 60000  # relief is re-based to the city core; +-600 m of height covers every ridge
SECTORS = {k: (x * U, y * U, SECTOR_Z, hx * U, hy * U, SECTOR_HZ) for k, (x, y, hx, hy) in SECTOR_UNITS.items()}

# Episode 02 corridor points, game-space metres from Checkpoint Echo.
E02 = {
    "echo": (0, 0), "relay_kilo": (600, 900), "corridor_end": (1700, 1500), "narrows": (1250, 1650),
    "bridge": (1520, 1400), "drain_a": (1408, 1320), "drain_b": (1592, 1656), "south_gate": (2020, 2180),
}
ROAD = [(0, 0), (120, 320), (380, 680), (660, 928), (1000, 1120), (1320, 1280), (1520, 1400), (1700, 1500),
        (1860, 1800), (2020, 2180)]
RIVER = [(2240, 1000), (1840, 1220), (1520, 1400), (1340, 1540), (1250, 1650), (1288, 1848), (1500, 2020),
         (1820, 2112), (2360, 2180)]
RIVER_WIDTH_CM = 10400  # 26 units
# Corridor zones: plain trigger boxes (not sector volumes: the sector ids belong to the city). name -> (point, half x, half y m)
CORRIDOR_ZONES = {
    "corridor_valley": ((850, 800), 1100, 900),
    "corridor_narrows": (E02["narrows"], 300, 250),
    "corridor_bridge": (E02["bridge"], 150, 150),
    "corridor_drainage_tunnel": ((1500, 1488), 200, 220),
}
# Underground routes from the map JSON "undergroundRoutes": id -> (entrance x, y, exit x, y) in Unreal cm.
UNDERGROUND = {
    "river_drainage": corridor(*E02["drain_a"]) + corridor(*E02["drain_b"]),
    "old_gardun_cellars": (-42 * U, 82 * U, -78 * U, -30 * U),
    "mountain_fort_tunnel": (-215 * U, -150 * U, -170 * U, -90 * U),
    "citadel_operations": (0, -222 * U, 32 * U, -272 * U),
}
# City objective markers (play/src/gardun.ts OBJECTIVES), unit coordinates. id -> (x, z)
OBJECTIVES = {
    "capture_airfield": (-200, 190), "secure_industrial_depot": (190, 185), "west_ridge_radio": (-235, 25),
    "government_relay": (150, 40), "market_square": (55, -30), "medical_station": (-17.5, 17.5),
    "municipal_ruins": (-70, -55), "mountain_fort": (-215, -185), "east_power_station": (215, -150),
    "outer_gate": (0, -172), "inner_courtyard": (0, -222), "command_hall": (-32, -258), "operations_room": (32, -272),
}
# The city crossings of the Dry Riverbed (gardun.ts CROSSINGS): id -> (x0, x1) units; river z 100 to 118.
CROSSINGS = {"riverbed_bridge": (-12, 12), "west_ford": (-152, -126), "east_crossing": (112, 132)}

PLAYER_STARTS = {  # name -> (x, y, yaw); Echo for Episode 02, South Gate yard for Episode 03
    "echo_1": corridor(0, 0) + (270,), "echo_2": (corridor(0, 0)[0] + 800, corridor(0, 0)[1]) + (270,),
    "south_gate_1": (-1500, 272 * U, 270), "south_gate_2": (1500, 272 * U, 270),
}
VEHICLES = {  # class name -> (x, y, yaw); convoy at Echo, helicopter and drone at the South Gate yard
    "HavocGroundVehicle": corridor(0, 0)[:1] + (corridor(0, 0)[1] - 1200,) + (270,),
    "HavocDroneVehicle": (corridor(0, 0)[0] - 1200, corridor(0, 0)[1] - 400, 270),
    "HavocHelicopterVehicle": (0, 272 * U + 3500, 270),
}
DIRECTOR_XY = (0, 272 * U - 3000)
# NavMesh covers the city core and the corridor. Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-navigation-in-unreal-engine
NAV_BOUNDS = (65000, 110000, 0, 195000, 240000, 60000)
TEMPLATE_CLASSES = ("Landscape", "LandscapeStreamingProxy", "DirectionalLight", "SkyAtmosphere", "SkyLight",
                    "ExponentialHeightFog", "VolumetricCloud")


def log(m):
    unreal.log("[GardunBuilder] " + m)


def warn(m):
    unreal.log_warning("[GardunBuilder] " + m)


def load_map_def():
    with open(MAP_JSON, encoding="utf-8") as f:
        d = json.load(f)
    if d.get("id") != "gardun_city" or d.get("fictional") is not True:
        raise RuntimeError("gardun-city.json must stay gardun_city / fictional=true")
    ids = [s["id"] for s in d["sectors"]]
    missing = [i for i in ids if i not in SECTORS]
    extra = [i for i in SECTORS if i not in ids]
    if missing or extra:
        raise RuntimeError("SECTORS does not match %s. Missing: %s, not in JSON: %s" % (MAP_JSON, missing, extra))
    miss_u = [r for r in d.get("undergroundRoutes", []) if r not in UNDERGROUND]
    if miss_u:
        raise RuntimeError("UNDERGROUND is missing routes from %s: %s" % (MAP_JSON, miss_u))
    return d


def configure_project():
    """Lumen GI + reflections (same as the Kharg level)."""
    try:
        s = unreal.get_default_object(unreal.RendererSettings)
        s.set_editor_property("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
        s.set_editor_property("reflection_method", unreal.ReflectionMethod.LUMEN)
    except Exception as e:
        warn("Could not set Lumen in Project Settings (%s). Set it by hand." % e)


def open_or_create_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        log("Opening existing level %s" % LEVEL_PATH)
        if not les.load_level(LEVEL_PATH):
            raise RuntimeError("Could not load " + LEVEL_PATH)
        return les
    unreal.EditorAssetLibrary.make_directory(LEVEL_DIR)
    log("Creating World Partition level from the Open World template")
    if not les.new_level_from_template(LEVEL_PATH, "/Engine/Maps/Templates/OpenWorld"):
        raise RuntimeError("Could not create the Open World level (needs UE 5.5 editor)")
    return les


def clear_previous(eas):
    n = 0
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) in a.tags:
            eas.destroy_actor(a)
            n += 1
    log("Removed %d actors from the previous run" % n)


def remove_template_actors(eas):
    n = 0
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) not in a.tags and a.get_class().get_name() in TEMPLATE_CLASSES:
            eas.destroy_actor(a)
            n += 1
    log("Removed %d Open World template actors" % n)


def always_loaded(a):
    """World Partition: never stream this actor out (gameplay logic must exist at any distance)."""
    try:
        a.set_editor_property("is_spatially_loaded", False)
    except Exception as e:
        warn("Could not mark %s always loaded (%s). Untick Is Spatially Loaded in its Details panel." % (a.get_actor_label(), e))


def spawn(eas, cls, label, loc, rot=(0, 0, 0), scale=None, loaded=True):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if a is None:
        raise RuntimeError("Could not spawn %s. Build the C++ project first." % label)
    a.set_actor_label(label)
    a.tags = [unreal.Name(TAG)]
    if scale:
        a.set_actor_scale3d(unreal.Vector(*scale))
    if loaded:
        always_loaded(a)
    return a


def havoc(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        raise RuntimeError("%s not found. Build the MidnightHavoc C++ project and restart the editor." % name)
    return cls


def placeholder_material(name, rgb, roughness=0.9, two_sided=False):
    """Plain-colour greybox material. Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python"""
    path = PLACEHOLDER_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    unreal.EditorAssetLibrary.make_directory(PLACEHOLDER_DIR)
    mel = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PLACEHOLDER_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", two_sided)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    col.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
    rough.set_editor_property("r", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def shape(eas, label, kind, loc, scale, rgb, rot=(0, 0, 0), mat_name="M_GardunGrey"):
    """Greybox engine shape. Basic shapes are 100 cm, so scale 10 = 10 m. Not world-partition streamed out."""
    a = spawn(eas, unreal.StaticMeshActor, label, loc, rot, scale)
    a.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/%s.%s" % (kind, kind)))
    a.static_mesh_component.set_material(0, placeholder_material(mat_name, rgb))
    return a


_HEIGHTS = None
BASE_ELEVATION = None


def ground_z(x, y, above=300.0):
    """Terrain height (cm) at x, y from gardun_terrain.obj plus `above` (0 + above when the mesh is missing)."""
    global _HEIGHTS
    if _HEIGHTS is None:
        _HEIGHTS = []
        if os.path.isfile(TERRAIN_OBJ):
            with open(TERRAIN_OBJ) as f:
                for line in f:
                    if line.startswith("v "):
                        _HEIGHTS.append(float(line.split()[3]))
    if not _HEIGHTS:
        return above
    n = int(round(len(_HEIGHTS) ** 0.5))
    c = min(n - 1, max(0, int(round((x - T_X0) / T_SIZE * (n - 1)))))
    r = min(n - 1, max(0, int(round((y - T_Y0) / T_SIZE * (n - 1)))))
    return _HEIGHTS[r * n + c] + above


def build_landscape(eas):
    """Real terrain as a static mesh (UE 5.5 Python cannot create a Landscape; same approach as the Kharg level)."""
    if not os.path.isfile(TERRAIN_OBJ):
        warn("Terrain mesh missing: %s. Run: python game/Content/Python/make_gardun_heightmap.py --download. Terrain skipped." % TERRAIN_OBJ)
        return None
    if os.path.isfile(TERRAIN_INFO):
        with open(TERRAIN_INFO, encoding="utf-8") as f:
            info = json.load(f)
        log("Terrain source: %s tile %s, base elevation %.0f m" % (info["dataset"], info["tile"], info["base_elevation_m"]))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", TERRAIN_OBJ)
    task.set_editor_property("destination_path", TERRAIN_PATH)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = None
    for p in task.get_editor_property("imported_object_paths") or []:
        a = unreal.EditorAssetLibrary.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            mesh = a
    if mesh is None:
        raise RuntimeError("Gardun terrain mesh import failed (%s)" % TERRAIN_OBJ)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_material(0, placeholder_material("M_GardunGround", (0.55, 0.48, 0.36), two_sided=True))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    land = spawn(eas, unreal.StaticMeshActor, "GardunTerrain", (0, 0, 0))
    land.static_mesh_component.set_static_mesh(mesh)
    best = None  # lay it flat if the importer treated the OBJ as Y-up
    for rot in ((0, 0, 0), (90, 0, 0), (-90, 0, 0), (0, 90, 0), (0, -90, 0)):
        land.set_actor_rotation(unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]), False)
        _, ext = land.get_actor_bounds(False)
        if best is None or ext.z < best[1]:
            best = (rot, ext.z)
    land.set_actor_rotation(unreal.Rotator(roll=best[0][0], pitch=best[0][1], yaw=best[0][2]), False)
    origin, ext = land.get_actor_bounds(False)
    exp = (T_X0 + T_SIZE / 2, T_Y0 + T_SIZE / 2)
    if abs(origin.x - exp[0]) > 5000 or abs(origin.y - exp[1]) > 5000:
        warn("Terrain bounds centre is (%d, %d), expected (%d, %d). The importer may have mirrored or moved it: check it in the viewport."
             % (origin.x, origin.y, exp[0], exp[1]))
    log("Placed GardunTerrain (7 km, half height %.0f cm)" % ext.z)
    return land


def build_sky(eas):
    spawn(eas, unreal.DirectionalLight, "GardunSun", (0, 0, 100000), (0, -45, 30))
    spawn(eas, unreal.SkyAtmosphere, "GardunSkyAtmosphere", (0, 0, 0))
    sky = spawn(eas, unreal.SkyLight, "GardunSkyLight", (0, 0, 50000))
    sky.get_editor_property("light_component").set_editor_property("real_time_capture", True)
    spawn(eas, unreal.VolumetricCloud, "GardunClouds", (0, 0, 0))
    fog = spawn(eas, unreal.ExponentialHeightFog, "GardunHeightFog", (0, 0, 0))
    fc = fog.get_editor_property("component")
    fc.set_editor_property("fog_density", 0.002)  # light dry-air haze; the sandstorm event raises it at runtime
    fc.set_editor_property("fog_height_falloff", 0.4)


def segment(eas, label, p0, p1, width, height, rgb, mat_name, lift=0.0):
    """A flat greybox slab between two (x, y) points, resting on the terrain at its midpoint."""
    import math
    mx, my = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2
    length = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    yaw = math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0]))
    return shape(eas, label, "Cube", (mx, my, ground_z(mx, my, lift)), (length / 100.0, width / 100.0, height / 100.0), rgb, (0, 0, yaw), mat_name)


def build_sectors(eas):
    cls = havoc("HavocSectorVolume")
    for sid, (x, y, z, hx, hy, hz) in SECTORS.items():
        v = spawn(eas, cls, sid, (x, y, z), scale=(hx / 40.0, hy / 40.0, hz / 40.0))  # TriggerBox half extent 40
        v.set_editor_property("sector_id", sid)
    log("Placed %d sector volumes" % len(SECTORS))


def build_corridor(eas):
    """Episode 02: road, Dry Riverbed, riverbed bridge, drainage tunnel, narrows, staging yard."""
    pts = [corridor(*p) for p in ROAD]
    for i in range(len(pts) - 1):
        segment(eas, "road_%02d" % i, pts[i], pts[i + 1], 800, 60, (0.30, 0.29, 0.27), "M_GardunRoad", 30)
    rv = [corridor(*p) for p in RIVER]
    for i in range(len(rv) - 1):
        segment(eas, "riverbed_%02d" % i, rv[i], rv[i + 1], RIVER_WIDTH_CM, 20, (0.45, 0.40, 0.30), "M_GardunRiverbed", 10)
    bx, by = corridor(*E02["bridge"])
    shape(eas, "riverbed_bridge", "Cube", (bx, by, ground_z(bx, by, 450)), (30, 10, 1.5), (0.40, 0.40, 0.42), (0, 0, 0), "M_GardunStructure")
    # Anchor for the bridge_obstruction event (the wreck blocks the deck): a target point the mission can find by tag.
    spawn(eas, unreal.TargetPoint, "bridge_obstruction_point", (bx - 800, by, ground_z(bx, by, 600)))
    a, b = (corridor(*E02["drain_a"]), corridor(*E02["drain_b"]))
    for end, (px, py) in (("a", a), ("b", b)):  # tunnel portals; infantry only
        shape(eas, "drainage_tunnel_portal_" + end, "Cube", (px, py, ground_z(px, py, 150)), (6, 6, 3), (0.25, 0.25, 0.27), (0, 0, 0), "M_GardunTunnel")
    segment(eas, "drainage_tunnel_run", a, b, 250, 250, (0.20, 0.20, 0.22), "M_GardunTunnel", -300)
    nx, ny = corridor(*E02["narrows"])
    shape(eas, "ambush_narrows_marker", "Cylinder", (nx, ny, ground_z(nx, ny, 100)), (20, 20, 2), (0.8, 0.3, 0.1), (0, 0, 0), "M_GardunMarker")
    for name, (p, hx, hy) in CORRIDOR_ZONES.items():
        cx, cy = corridor(*p)
        spawn(eas, unreal.TriggerBox, name, (cx, cy, 0), scale=(hx * 100 / 40.0, hy * 100 / 40.0, SECTOR_HZ / 40.0))
    for name in ("echo", "relay_kilo", "corridor_end"):
        px, py = corridor(*E02[name])
        shape(eas, "marker_" + name, "Cylinder", (px, py, ground_z(px, py, 100)), (15, 15, 2), (0.9, 0.8, 0.1), (0, 0, 0), "M_GardunMarker")
    # South Gate staging yard: a flat apron inside the south_gate sector.
    sx, sy = SOUTH_GATE_XY
    shape(eas, "staging_yard", "Cube", (sx, sy, ground_z(sx, sy, 20)), (180, 100, 0.4), (0.38, 0.36, 0.32), (0, 0, 0), "M_GardunYard")


def build_city(eas):
    """Ep03 greybox: objective blocks, river crossings, underground route portals, citadel walls."""
    for oid, (ux, uz) in OBJECTIVES.items():
        x, y = ux * U, uz * U
        shape(eas, "objective_" + oid, "Cube", (x, y, ground_z(x, y, 600)), (24, 24, 12), (0.55, 0.50, 0.42), (0, 0, 0), "M_GardunBuilding")
    # The Dry Riverbed through the city: a slab with a gap at each crossing.
    x_edges = [-312] + [e for _, (a, b) in sorted(CROSSINGS.items(), key=lambda kv: kv[1][0]) for e in (a, b)] + [312]
    for i in range(0, len(x_edges), 2):
        x0, x1 = x_edges[i] * U, x_edges[i + 1] * U
        segment(eas, "city_riverbed_%d" % (i // 2), (x0, 109 * U), (x1, 109 * U), 18 * U, 20, (0.45, 0.40, 0.30), "M_GardunRiverbed", 10)
    for cid, (a, b) in CROSSINGS.items():
        cx = (a + b) / 2 * U
        shape(eas, cid, "Cube", (cx, 109 * U, ground_z(cx, 109 * U, 150)), ((b - a) * U / 100.0, 18 * U / 100.0, 1.5), (0.40, 0.40, 0.42), (0, 0, 0), "M_GardunStructure")
    # Citadel outer wall: four slabs round the sector, gap in the south side for the Outer Gate.
    cx, cy, _, hx, hy, _ = SECTORS["citadel"]
    gate = 1200
    for name, p0, p1 in (("north", (cx - hx, cy - hy), (cx + hx, cy - hy)), ("west", (cx - hx, cy - hy), (cx - hx, cy + hy)),
                         ("east", (cx + hx, cy - hy), (cx + hx, cy + hy)),
                         ("south_w", (cx - hx, cy + hy), (cx - gate, cy + hy)), ("south_e", (cx + gate, cy + hy), (cx + hx, cy + hy))):
        segment(eas, "citadel_wall_" + name, p0, p1, 800, 1200, (0.50, 0.46, 0.40), "M_GardunBuilding", 600)
    for rid, (ax, ay, bx, by) in UNDERGROUND.items():
        if rid == "river_drainage":
            continue  # portals are built with the corridor
        for end, (px, py) in (("a", (ax, ay)), ("b", (bx, by))):
            shape(eas, "%s_portal_%s" % (rid, end), "Cube", (px, py, ground_z(px, py, 150)), (5, 5, 3), (0.25, 0.25, 0.27), (0, 0, 0), "M_GardunTunnel")


def build_gameplay(eas):
    build_sectors(eas)
    build_corridor(eas)
    build_city(eas)
    for name, (x, y, yaw) in PLAYER_STARTS.items():
        spawn(eas, unreal.PlayerStart, "PlayerStart_" + name, (x, y, ground_z(x, y, 300)), (0, 0, yaw))
    director = spawn(eas, havoc("HavocLevelDirector"), "HavocLevelDirector", (DIRECTOR_XY[0], DIRECTOR_XY[1], ground_z(*DIRECTOR_XY, above=300)))
    director.set_editor_property("map_id", "gardun_city")
    director.set_editor_property("mission_id", "episode_02_silent_corridor")  # Episode 03 maps set episode_03_broken_citadel here
    for cls_name, (x, y, yaw) in VEHICLES.items():
        spawn(eas, havoc(cls_name), cls_name, (x, y, ground_z(x, y, 300)), (0, 0, yaw))
    cls = getattr(unreal, "NavMeshBoundsVolume", None)
    if cls is None:
        warn("NavMeshBoundsVolume not available; place one over the city and corridor by hand (game/docs/ai.md)")
        return
    x, y, z, hx, hy, hz = NAV_BOUNDS
    spawn(eas, cls, "NavMeshBounds_Gardun", (x, y, z), scale=(hx / 100.0, hy / 100.0, hz / 100.0))  # brush is a 200 cm cube
    log("Placed NavMeshBounds_Gardun; press P in the viewport to check the green NavMesh covers the city and corridor")


def verify(eas, data):
    """Fail the run (so CI does not commit a broken level) unless every sector id has exactly one volume."""
    cls = havoc("HavocSectorVolume")
    found = {}
    for a in eas.get_all_level_actors():
        if isinstance(a, cls):
            sid = str(a.get_editor_property("sector_id"))
            found[sid] = found.get(sid, 0) + 1
    bad = [s["id"] for s in data["sectors"] if found.get(s["id"]) != 1]
    if bad:
        raise RuntimeError("Sector volumes missing or duplicated: %s" % bad)
    log("Verified: all %d Gardun sector ids have one placed volume" % len(data["sectors"]))


def main():
    data = load_map_def()
    configure_project()
    les = open_or_create_level()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clear_previous(eas)
    remove_template_actors(eas)
    build_landscape(eas)
    build_sky(eas)
    build_gameplay(eas)
    verify(eas, data)
    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Done. Level saved at %s." % LEVEL_PATH)


main()
