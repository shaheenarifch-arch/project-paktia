"""Build Content/Maps/Paktia/L_PaktiaHighlands (Episode 01, greybox) in one run (Unreal Python, UE 5.5).

Run in the editor:  Tools > Execute Python Script...  (or Output Log: py "<path>/build_paktia_level.py")
Run headless:       Actions > Build Paktia level (same launch as build_kharg_level.py)
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python

Greybox bar (issue #70): sectors from data/maps/paktia-highlands.json, a sloped ground with ridge blocks,
block buildings for Checkpoint Echo and Ridge Station, a marked crash grid R-17, player starts and a NavMesh.
No Fab art: production art is wired in later from Shaheen's Fab packs. Idempotent: every actor this script
spawns is tagged HavocPaktiaBuilder; a re-run deletes those and rebuilds them.
Coordinates: Unreal cm, +X east, +Y south, origin = Checkpoint Echo.
"""
import json
import os

import unreal

TAG = "HavocPaktiaBuilder"
LEVEL_DIR = "/Game/Maps/Paktia"
LEVEL_NAME = "L_PaktiaHighlands"
LEVEL_PATH = LEVEL_DIR + "/" + LEVEL_NAME
HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", ".."))
MAP_JSON = os.path.normpath(os.path.join(GAME_DIR, "..", "data", "maps", "paktia-highlands.json"))

# sector id -> (centre x, y, z, half-extent x, y, z) in cm. Game layout, not a survey.
LAYOUT = {
    "highlands_staging": (-60000, 90000, 0, 25000, 20000, 20000),
    "checkpoint_echo":   (0, 0, 0, 20000, 20000, 20000),
    "ridge_station":     (120000, -160000, 25000, 20000, 20000, 20000),
    "crash_grid_r17":    (-90000, -230000, 10000, 25000, 25000, 20000),
}
# Block buildings (centre x, y, size x, y, z in cm) as stand-ins. Echo = checkpoint compound, Ridge = relay station.
BLOCKS = {
    "echo_hut_1": (-6000, -4000, (60, 40, 30)), "echo_hut_2": (7000, 3000, (50, 50, 30)),
    "echo_wall": (0, -12000, (200, 6, 20)), "ridge_tower": (120000, -160000, (30, 30, 80)),
    "ridge_hut": (126000, -154000, (50, 40, 30)),
}
RIDGES = [  # cover and sight lines: (x, y, scale x, y, z), basic cubes at 100 cm
    (-30000, -60000, (300, 80, 60)), (60000, -90000, (200, 100, 90)), (90000, -200000, (250, 90, 120)),
    (-60000, -140000, (150, 150, 70)), (30000, 40000, (180, 60, 40)),
]
PLAYER_STARTS = {"start_1": (-60000, 90000, 270), "start_2": (-56000, 92000, 270)}  # x, y, yaw
NAV_BOUNDS = (10000, -70000, 10000, 200000, 220000, 60000)


def log(m):
    unreal.log("[PaktiaBuilder] " + m)


def havoc(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        raise RuntimeError("%s not found. Build the MidnightHavoc C++ project and restart the editor." % name)
    return cls


def load_map_def():
    with open(MAP_JSON, encoding="utf-8") as f:
        d = json.load(f)
    ids = [s["id"] for s in d["sectors"]]
    if set(ids) != set(LAYOUT):
        raise RuntimeError("LAYOUT does not match %s. JSON: %s, LAYOUT: %s" % (MAP_JSON, sorted(ids), sorted(LAYOUT)))
    return d


def open_or_create_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        if not les.load_level(LEVEL_PATH):
            raise RuntimeError("Could not load " + LEVEL_PATH)
        return les
    unreal.EditorAssetLibrary.make_directory(LEVEL_DIR)
    if not les.new_level(LEVEL_PATH):
        raise RuntimeError("Could not create " + LEVEL_PATH)
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


def cube(eas, label, x, y, z, scale):
    a = spawn(eas, unreal.StaticMeshActor, label, (x, y, z), scale=scale)
    a.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube"))
    return a


def main():
    load_map_def()
    les = open_or_create_level()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) in a.tags:
            eas.destroy_actor(a)
    # Ground: 6 km x 6 km slab that tilts up toward Ridge Station (greybox terrain).
    cube(eas, "PaktiaGround", 0, -70000, -500, (600, 600, 10))
    ramp = cube(eas, "PaktiaRidgeSlope", 120000, -160000, 12000, (220, 220, 10))
    ramp.set_actor_rotation(unreal.Rotator(roll=0, pitch=0, yaw=0), False)
    for label, (x, y, size) in BLOCKS.items():
        z = 25000 if label.startswith("ridge") else 0
        cube(eas, label, x, y, z + size[2] * 50, size)
    for i, (x, y, size) in enumerate(RIDGES):
        cube(eas, "ridge_cover_%d" % (i + 1), x, y, size[2] * 50, size)
    cube(eas, "crash_marker_r17", -90000, -230000, 10300, (20, 20, 6))
    spawn(eas, unreal.DirectionalLight, "PaktiaSun", (0, 0, 100000), (0, -45, 30))
    spawn(eas, unreal.SkyAtmosphere, "PaktiaSky", (0, 0, 0))
    spawn(eas, unreal.SkyLight, "PaktiaSkyLight", (0, 0, 50000))

    sector_cls = havoc("HavocSectorVolume")
    for sid, (x, y, z, hx, hy, hz) in LAYOUT.items():
        v = spawn(eas, sector_cls, sid, (x, y, z), scale=(hx / 40.0, hy / 40.0, hz / 40.0))  # TriggerBox half extent 40
        v.set_editor_property("sector_id", sid)
    for name, (x, y, yaw) in PLAYER_STARTS.items():
        spawn(eas, unreal.PlayerStart, "PlayerStart_" + name, (x, y, 200), (0, 0, yaw))
    x, y, z, hx, hy, hz = NAV_BOUNDS
    spawn(eas, unreal.NavMeshBoundsVolume, "NavMeshBounds_Paktia", (x, y, z), scale=(hx / 100.0, hy / 100.0, hz / 100.0))
    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Done. Level saved at %s." % LEVEL_PATH)


main()
