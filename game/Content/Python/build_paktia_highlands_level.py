"""Build Content/Maps/PaktiaHighlands/L_PaktiaHighlands (Episode 01 greybox) in one run (Unreal Python, UE 5.5).

Run in the editor:  Tools > Execute Python Script...  (or Output Log: py "<path>/build_paktia_highlands_level.py")
Run headless:       the "Build level" workflow (.github/workflows/build-level.yml), level = PaktiaHighlands
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python

Greybox: terrain mesh from make_paktia_heightmap.py (Copernicus DEM), engine basic shapes, plain-colour materials.
Idempotent: every actor spawned here is tagged HavocPaktiaBuilder; a re-run deletes those and rebuilds them.

Always loaded: the level is a plain (non World Partition) level, so every actor, gameplay actors included,
is loaded with the map and nothing is spatially streamed. Do not convert it to World Partition.

Sector ids come from data/maps/paktia-highlands.json; the script refuses to run if the JSON and LAYOUT disagree.
Coordinates: Unreal cm, +X east, +Y south, origin = centre of the 20 km terrain. Heights are relative to the lowest point.
"""
import json
import os

import unreal

TAG = "HavocPaktiaBuilder"
LEVEL_DIR = "/Game/Maps/PaktiaHighlands"
LEVEL_NAME = "L_PaktiaHighlands"
LEVEL_PATH = LEVEL_DIR + "/" + LEVEL_NAME
HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", ".."))
MAP_JSON = os.path.normpath(os.path.join(GAME_DIR, "..", "data", "maps", "paktia-highlands.json"))
TERRAIN_OBJ = os.path.join(GAME_DIR, "Content", "Maps", "PaktiaHighlands", "Source", "paktia_terrain.obj")
TERRAIN_PATH = LEVEL_DIR + "/Terrain"
PLACEHOLDER_DIR = LEVEL_DIR + "/Placeholder"

# sector id -> (centre x, y, half-extent x, y) in cm. Z is placed on the terrain; every sector is 300 m tall each way.
SECTOR_HALF_Z = 30000
LAYOUT = {
    # Ids must match data/maps/paktia-highlands.json (the missions reference them).
    "highlands_staging": (-650000, 300000, 70000, 60000),   # Marine drop-off and Raven launch, west of Echo
    "checkpoint_echo":   (-400000, 300000, 60000, 50000),
    "ridge_station":     (200000, -250000, 50000, 50000),
    "crash_grid_r17":    (400000, 100000, 40000, 40000),
}
PLAYER_STARTS = {  # name -> (x, y, yaw), all inside checkpoint_echo
    "echo_1": (-420000, 300000, 0), "echo_2": (-400000, 300000, 0),
    "echo_3": (-380000, 300000, 0), "echo_4": (-400000, 320000, 0),
}
FALCON_CRASH = (400000, 100000, 20)  # x, y, yaw: Falcon-2 wreck marker inside crash_grid_r17
RIDGE_STATION = (200000, -250000)
DIRECTOR_POS = (-400000, 270000)
# NavMesh bounds (centre x, y, half-extent x, y): all four sectors and the ground between them.
NAV_BOUNDS = (-100000, 50000, 640000, 380000)
NAV_HALF_Z = 60000
TERRAIN_HALF = 1000000.0  # terrain is 20 km across
TEMPLATE_CLASSES = ("Landscape", "LandscapeStreamingProxy", "DirectionalLight", "SkyAtmosphere", "SkyLight",
                    "ExponentialHeightFog", "VolumetricCloud")


def log(m):
    unreal.log("[PaktiaBuilder] " + m)


def warn(m):
    unreal.log_warning("[PaktiaBuilder] " + m)


def havoc(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        raise RuntimeError("%s not found. Build the MidnightHavoc C++ project and restart the editor." % name)
    return cls


def load_map_def():
    with open(MAP_JSON, encoding="utf-8") as f:
        d = json.load(f)
    ids = [s["id"] for s in d["sectors"]]
    missing = [i for i in ids if i not in LAYOUT]
    extra = [i for i in LAYOUT if i not in ids]
    if missing or extra:
        raise RuntimeError("LAYOUT does not match %s. Missing: %s, not in JSON: %s" % (MAP_JSON, missing, extra))
    return d


def open_or_create_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        log("Opening existing level %s" % LEVEL_PATH)
        if not les.load_level(LEVEL_PATH):
            raise RuntimeError("Could not load " + LEVEL_PATH)
        return les
    unreal.EditorAssetLibrary.make_directory(LEVEL_DIR)
    log("Creating a plain level (no World Partition: everything stays loaded)")
    if not les.new_level(LEVEL_PATH):
        raise RuntimeError("Could not create the level " + LEVEL_PATH)
    return les


def spawn(eas, cls, label, loc, rot=(0, 0, 0), scale=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if a is None:
        raise RuntimeError("Could not spawn %s. Build the C++ project first." % label)
    a.set_actor_label(label)
    a.tags = [unreal.Name(TAG)]
    if scale:
        a.set_actor_scale3d(unreal.Vector(*scale))
    return a


def clear_previous(eas):
    n = 0
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) in a.tags or a.get_class().get_name() in TEMPLATE_CLASSES:
            eas.destroy_actor(a)
            n += 1
    log("Removed %d actors from the previous run or the default level" % n)


def placeholder_material(name, rgb, roughness=0.9):
    path = PLACEHOLDER_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    unreal.EditorAssetLibrary.make_directory(PLACEHOLDER_DIR)
    mel = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, PLACEHOLDER_DIR, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    col.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
    rough.set_editor_property("r", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def basic_shape(shape):
    return unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/%s.%s" % (shape, shape))


_HEIGHTS = None


def ground_z(x, y, above=300.0):
    """Terrain height (cm) at x, y from paktia_terrain.obj plus `above`."""
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
    c = min(n - 1, max(0, int(round((x + TERRAIN_HALF) / (2 * TERRAIN_HALF) * (n - 1)))))
    r = min(n - 1, max(0, int(round((TERRAIN_HALF - y) / (2 * TERRAIN_HALF) * (n - 1)))))
    return _HEIGHTS[r * n + c] + above


def build_terrain(eas):
    """Import the DEM mesh, lay it flat, give it collision so the Marine can walk on it."""
    if not os.path.isfile(TERRAIN_OBJ):
        raise RuntimeError("Terrain mesh missing: %s. Run: python game/Content/Python/make_paktia_heightmap.py --download"
                           % TERRAIN_OBJ)
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
        raise RuntimeError("Terrain mesh import failed (%s)" % TERRAIN_OBJ)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_material(0, placeholder_material("M_PaktiaRock", (0.38, 0.33, 0.27)))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    land = spawn(eas, unreal.StaticMeshActor, "PaktiaTerrain", (0, 0, 0))
    land.static_mesh_component.set_static_mesh(mesh)
    # The OBJ importer may stand the grid on its side: keep the rotation with the smallest vertical extent.
    best = None
    for rot in ((0, 0, 0), (90, 0, 0), (-90, 0, 0), (0, 90, 0), (0, -90, 0)):
        land.set_actor_rotation(unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]), False)
        _, ext = land.get_actor_bounds(False)
        if best is None or ext.z < best[1]:
            best = (rot, ext.z)
    land.set_actor_rotation(unreal.Rotator(roll=best[0][0], pitch=best[0][1], yaw=best[0][2]), False)
    log("Placed PaktiaTerrain (20 km, half height %.0f cm)" % best[1])


def build_sky(eas):
    spawn(eas, unreal.DirectionalLight, "PaktiaSun", (0, 0, 400000), (0, -40, 60))
    spawn(eas, unreal.SkyAtmosphere, "PaktiaSkyAtmosphere", (0, 0, 0))
    sky = spawn(eas, unreal.SkyLight, "PaktiaSkyLight", (0, 0, 300000))
    sky.get_editor_property("light_component").set_editor_property("real_time_capture", True)
    fog = spawn(eas, unreal.ExponentialHeightFog, "PaktiaHeightFog", (0, 0, 0))
    fog.get_editor_property("component").set_editor_property("fog_density", 0.002)


def build_gameplay(eas):
    sector_cls = havoc("HavocSectorVolume")
    for sid, (x, y, hx, hy) in LAYOUT.items():
        v = spawn(eas, sector_cls, sid, (x, y, ground_z(x, y, 0)), scale=(hx / 40.0, hy / 40.0, SECTOR_HALF_Z / 40.0))
        v.set_editor_property("sector_id", sid)  # TriggerBox half extent is 40

    for name, (x, y, yaw) in PLAYER_STARTS.items():
        spawn(eas, unreal.PlayerStart, "PlayerStart_" + name, (x, y, ground_z(x, y, 300)), (0, 0, yaw))

    # Falcon-2 crash site: a grey wreck block plus a burnt-ground disc. Greybox only (art is issue #71).
    x, y, yaw = FALCON_CRASH
    z = ground_z(x, y, 0)
    wreck = spawn(eas, unreal.StaticMeshActor, "Falcon2_CrashSite_R17", (x, y, z + 150), (0, 0, yaw), scale=(8, 3, 3))
    wreck.static_mesh_component.set_static_mesh(basic_shape("Cube"))
    wreck.static_mesh_component.set_material(0, placeholder_material("M_PaktiaWreck", (0.12, 0.12, 0.13), 0.7))
    scorch = spawn(eas, unreal.StaticMeshActor, "Falcon2_CrashScorch", (x, y, z + 5), scale=(30, 30, 0.1))
    scorch.static_mesh_component.set_static_mesh(basic_shape("Cylinder"))
    scorch.static_mesh_component.set_material(0, placeholder_material("M_PaktiaScorch", (0.03, 0.03, 0.03), 1.0))

    # Ridge Station: a small tower marker.
    rx, ry = RIDGE_STATION
    tower = spawn(eas, unreal.StaticMeshActor, "RidgeStation_Marker", (rx, ry, ground_z(rx, ry, 600)), scale=(10, 10, 12))
    tower.static_mesh_component.set_static_mesh(basic_shape("Cube"))
    tower.static_mesh_component.set_material(0, placeholder_material("M_PaktiaStructure", (0.35, 0.36, 0.38), 0.6))

    dx, dy = DIRECTOR_POS
    director = spawn(eas, havoc("HavocLevelDirector"), "HavocLevelDirector", (dx, dy, ground_z(dx, dy, 300)))
    director.set_editor_property("map_id", "paktia_highlands")
    # data/missions has no Episode 01 file yet: keep the director from auto-starting a mission that does not exist.
    director.set_editor_property("mission_id", "episode_01_mountain_shield")
    director.set_editor_property("start_mission_on_begin_play", False)
    warn("HavocLevelDirector will not auto-start a mission: data/missions has no Episode 01 definition yet.")

    cls = getattr(unreal, "NavMeshBoundsVolume", None)
    if cls is None:
        warn("NavMeshBoundsVolume not available; place one over the valley by hand (docs/ai.md)")
        return
    x, y, hx, hy = NAV_BOUNDS
    spawn(eas, cls, "NavMeshBounds_Paktia", (x, y, ground_z(x, y, 0) + 20000),
          scale=(hx / 100.0, hy / 100.0, NAV_HALF_Z / 100.0))  # new brush = 200 cm cube (half extent 100)
    log("Placed NavMeshBounds_Paktia; press P in the viewport to check the green NavMesh")


def main():
    load_map_def()
    les = open_or_create_level()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clear_previous(eas)
    build_terrain(eas)
    build_sky(eas)
    build_gameplay(eas)
    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Done. Level saved at %s." % LEVEL_PATH)


main()
