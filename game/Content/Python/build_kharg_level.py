"""Build Content/Maps/Kharg/L_KhargHormuz in one run (Unreal Python, UE 5.5).

Run in the editor:  Tools > Execute Python Script...  (or Output Log: py "<path>/build_kharg_level.py")
Run headless:       game/scripts/run-build-kharg-level.ps1
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python

Idempotent: every actor this script spawns is tagged HavocKhargBuilder. A re-run deletes those and
rebuilds them; actors you placed by hand are left alone.

Layout is simplified game space (real_world_hypothetical): no real equipment, markings or targeting data.
Sector ids/roles come from data/maps/kharg-hormuz.json; the script refuses to run if the JSON and the
LAYOUT table below disagree. Coordinates: Unreal cm, +X east, +Y south, origin = island centre.

Art comes from the Fab manifest (Content/Fab/fab_manifest.json, item 3). A missing asset is logged as a
warning that names the Fab pack. No grey boxes are spawned in its place.
"""
import json
import os

import unreal

TAG = "HavocKhargBuilder"
LEVEL_DIR = "/Game/Maps/Kharg"
LEVEL_NAME = "L_KhargHormuz"
LEVEL_PATH = LEVEL_DIR + "/" + LEVEL_NAME
HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", ".."))
MAP_JSON = os.path.normpath(os.path.join(GAME_DIR, "..", "data", "maps", "kharg-hormuz.json"))
HEIGHTMAP = os.path.join(GAME_DIR, "Content", "Maps", "Kharg", "Source", "kharg_heightmap_r16.png")
MANIFEST = os.path.join(GAME_DIR, "Content", "Fab", "fab_manifest.json")

# Landscape: 2017 x 2017 verts over 20 km -> ~992 cm per quad. Z scale 100 (1 m = 128 raw units).
LAND_RES = 2017
LAND_SCALE_XY = 20000.0 * 100 / (LAND_RES - 1)
LAND_SCALE_Z = 100.0

# sector id -> (centre x, y, z, half-extent x, y, z) in cm. Game layout, not a survey.
LAYOUT = {
    "task_force_sea":       (-700000, -700000, 0, 200000, 200000, 5000),
    "kharg_north_beach":    (0, -380000, 0, 150000, 45000, 5000),
    "kharg_west_beach":     (-250000, 0, 0, 45000, 150000, 5000),
    "kharg_airstrip":       (60000, -80000, 2000, 30000, 140000, 5000),
    "kharg_oil_terminal":   (-170000, 120000, 0, 90000, 90000, 5000),
    "kharg_town":           (110000, 160000, 0, 70000, 60000, 5000),
    "hormuz_inbound_lane":  (600000, 0, 0, 60000, 600000, 5000),
    "hormuz_outbound_lane": (850000, 0, 0, 60000, 600000, 5000),
}
TERMINAL_PARTS = {  # part id -> (x, y, z), all inside kharg_oil_terminal
    "east_jetty": (-110000, 100000, 0),
    "sea_island_jetty": (-260000, 150000, 0),
    "tank_farm": (-180000, 60000, 500),
    "pump_station": (-150000, 140000, 500),
}
LANES = {  # lane id -> spline points (x, y); hazards sit along them. Episode 04 defaults: 4 inbound, 3 outbound
    "inbound": ([(600000, 600000), (600000, 200000), (600000, -200000), (600000, -600000)], 4),
    "outbound": ([(850000, -600000), (850000, -200000), (850000, 200000), (850000, 600000)], 3),
}
PLAYER_STARTS = {  # north and west beaches
    "north_beach_1": (-30000, -350000, 300, 90), "north_beach_2": (30000, -350000, 300, 90),
    "west_beach_1": (-225000, -30000, 300, 0), "west_beach_2": (-225000, 30000, 300, 0),
}
DIRECTOR_POS = (0, -330000, 300)
# NavMesh bounds (centre x, y, z, half-extent x, y, z) in cm: covers every island sector in LAYOUT
# (beaches, airstrip, oil terminal, town). Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-navigation-in-unreal-engine
NAV_BOUNDS = (-60000, -100000, 0, 250000, 340000, 50000)
VEHICLES = {  # class name -> (x, y, z, yaw); one of each playable vehicle (item 7), docs/vehicles.md. Boat sits on the sea (z = water level).
    "HavocBoatVehicle": (0, -440000, 0, 90),
    "HavocGroundVehicle": (90000, -360000, 300, 90),
    "HavocHelicopterVehicle": (-90000, -360000, 300, 90),
    "HavocDroneVehicle": (0, -320000, 300, 90),
}

# Fab manifest slots used here (item 3 may add more). Each slot: {"path": "/Game/Fab/...", "pack": "<Fab pack name>"}.
SLOTS = {
    "landscape_material": "landscape material (sand/rock)",
    "mesh_east_jetty": "jetty mesh", "mesh_sea_island_jetty": "jetty mesh",
    "mesh_tank_farm": "storage tank mesh", "mesh_pump_station": "pump station mesh",
    "mesh_hazard_marker": "abstract buoy-style hazard marker",
}


def log(m):
    unreal.log("[KhargBuilder] " + m)


def warn(m):
    unreal.log_warning("[KhargBuilder] " + m)


def load_manifest():
    if not os.path.isfile(MANIFEST):
        warn("Fab manifest not found at %s (item 3). All art slots will be skipped. Help: "
             "https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine" % MANIFEST)
        return {}
    with open(MANIFEST, encoding="utf-8") as f:
        data = json.load(f)
    return data.get("assets", data)


def get_asset(manifest, slot):
    """Soft-path lookup. Returns the loaded asset or None (with a warning naming the Fab pack)."""
    entry = manifest.get(slot)
    if not entry:
        warn("No Fab manifest entry for '%s' (%s). Add the Fab pack for it, then re-run." % (slot, SLOTS.get(slot, "")))
        return None
    path = entry["path"] if isinstance(entry, dict) else entry
    pack = entry.get("pack", "unnamed pack") if isinstance(entry, dict) else "unnamed pack"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        warn("Asset %s for '%s' is not imported yet. Import Fab pack '%s' (Window > Fab), then re-run." % (path, slot, pack))
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def load_map_def():
    with open(MAP_JSON, encoding="utf-8") as f:
        d = json.load(f)
    if d.get("setting") != "real_world_hypothetical" or d.get("fictional") is not False:
        raise RuntimeError("kharg-hormuz.json must stay real_world_hypothetical / fictional=false")
    ids = [s["id"] for s in d["sectors"]]
    missing = [i for i in ids if i not in LAYOUT]
    extra = [i for i in LAYOUT if i not in ids]
    if missing or extra:
        raise RuntimeError("LAYOUT does not match %s. Missing: %s, not in JSON: %s" % (MAP_JSON, missing, extra))
    return d


def configure_project():
    """Lumen GI + reflections. Water plugin and Python plugin are enabled in the .uproject."""
    try:
        s = unreal.get_default_object(unreal.RendererSettings)
        s.set_editor_property("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
        s.set_editor_property("reflection_method", unreal.ReflectionMethod.LUMEN)
        log("Lumen GI and reflections set (saved to DefaultEngine.ini on editor save).")
    except Exception as e:  # property names can shift between engine versions
        warn("Could not set Lumen in Project Settings (%s). Set it by hand: guide Step 1." % e)


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


def spawn(eas, cls, label, loc, rot=(0, 0, 0), scale=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if a is None:
        raise RuntimeError("Could not spawn %s. Build the C++ project first (guide, Before you start)." % label)
    a.set_actor_label(label)
    a.tags = [unreal.Name(TAG)]
    if scale:
        a.set_actor_scale3d(unreal.Vector(*scale))
    return a


def havoc(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        raise RuntimeError("%s not found. Build the MidnightHavoc C++ project and restart the editor." % name)
    return cls


def build_landscape(eas, manifest):
    """Best effort. Python cannot create a Landscape in every 5.5 build; if it fails, do guide Step 3 by hand."""
    if not os.path.isfile(HEIGHTMAP):
        warn("Heightmap missing: %s. Run: python game/Content/Python/make_kharg_heightmap.py --download "
             "(see guide Step 3). Landscape skipped." % HEIGHTMAP)
        return None
    try:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", HEIGHTMAP)
        task.set_editor_property("destination_path", LEVEL_DIR + "/Heightmap")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex_path = LEVEL_DIR + "/Heightmap/kharg_heightmap_r16"
        tex = unreal.EditorAssetLibrary.load_asset(tex_path)
        if tex is None:
            raise RuntimeError("heightmap texture import failed")
        # Landscape creation from a texture is exposed through the editor utility class when present.
        util = unreal.LandscapeEditorUtils if hasattr(unreal, "LandscapeEditorUtils") else None
        if util is None or not hasattr(util, "create_landscape_from_heightmap"):
            raise RuntimeError("no Python landscape-import API in this build")
        land = util.create_landscape_from_heightmap(
            tex, unreal.Vector(0, 0, 0), unreal.Vector(LAND_SCALE_XY, LAND_SCALE_XY, LAND_SCALE_Z), LAND_RES)
        land.set_actor_label("KhargLandscape")
        land.tags = [unreal.Name(TAG)]
        mat = get_asset(manifest, "landscape_material")
        if mat:
            land.set_editor_property("landscape_material", mat)
        return land
    except Exception as e:
        warn("Landscape not created by script (%s). Heightmap texture is imported; finish with guide Step 3 "
             "(Landscape Mode > Import from File, Scale XY %.0f, Z %.0f)." % (e, LAND_SCALE_XY, LAND_SCALE_Z))
        return None


def build_water(eas):
    try:
        ocean = spawn(eas, unreal.WaterBodyOcean, "KhargOcean", (0, 0, 0))
        zone = spawn(eas, unreal.WaterZone, "KhargWaterZone", (0, 0, 0))
        zone.set_editor_property("zone_extent", unreal.Vector2D(3000000, 3000000))
        return ocean, zone
    except Exception as e:
        warn("Water Body Ocean / Water Zone failed (%s). Enable the Water plugin and see guide Step 4." % e)
        return None, None


def build_sky(eas):
    spawn(eas, unreal.DirectionalLight, "KhargSun", (0, 0, 100000), (0, -45, 30))
    spawn(eas, unreal.SkyAtmosphere, "KhargSkyAtmosphere", (0, 0, 0))
    sky = spawn(eas, unreal.SkyLight, "KhargSkyLight", (0, 0, 50000))
    comp = sky.get_editor_property("light_component")
    comp.set_editor_property("real_time_capture", True)
    spawn(eas, unreal.VolumetricCloud, "KhargClouds", (0, 0, 0))
    fog = spawn(eas, unreal.ExponentialHeightFog, "KhargHeightFog", (0, 0, 0))
    fc = fog.get_editor_property("component")
    fc.set_editor_property("fog_density", 0.012)  # desert haze
    fc.set_editor_property("volumetric_fog", True)


def build_gameplay(eas, manifest):
    sector_cls = havoc("HavocSectorVolume")
    for sid, (x, y, z, hx, hy, hz) in LAYOUT.items():
        v = spawn(eas, sector_cls, sid, (x, y, z), scale=(hx / 40.0, hy / 40.0, hz / 40.0))  # TriggerBox half extent 40
        v.set_editor_property("sector_id", sid)

    part_cls = havoc("HavocTerminalPart")
    mesh_cls = unreal.StaticMeshActor
    for pid, pos in TERMINAL_PARTS.items():
        p = spawn(eas, part_cls, "terminal_" + pid, pos)
        p.set_editor_property("part_id", pid)
        mesh = get_asset(manifest, "mesh_" + pid)
        if mesh:
            m = spawn(eas, mesh_cls, "terminal_%s_mesh" % pid, pos)
            m.static_mesh_component.set_static_mesh(mesh)
            m.attach_to_actor(p, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, False)

    haz_cls = havoc("HavocLaneHazard")
    spl_cls = havoc("HavocTankerSpline")
    marker = get_asset(manifest, "mesh_hazard_marker")
    for lane, (pts, nhaz) in LANES.items():
        s = spawn(eas, spl_cls, "tanker_spline_" + lane, (pts[0][0], pts[0][1], 0))
        s.set_editor_property("lane_id", lane)
        route = s.get_editor_property("route")
        route.clear_spline_points(False)
        for (px, py) in pts:
            route.add_spline_point(unreal.Vector(px, py, 0), unreal.SplineCoordinateSpace.WORLD, False)
        route.update_spline()
        (x0, y0), (x1, y1) = pts[0], pts[-1]
        for i in range(nhaz):
            t = (i + 1) / (nhaz + 1)
            # offset sideways so hazards sit in the lane, not on the tanker line
            pos = (x0 + (x1 - x0) * t + (15000 if i % 2 else -15000), y0 + (y1 - y0) * t, 0)
            h = spawn(eas, haz_cls, "hazard_%s_%d" % (lane, i + 1), pos)
            h.set_editor_property("lane_id", lane)
            h.set_editor_property("hazard_id", "%s_%d" % (lane, i + 1))
            if marker:
                m = spawn(eas, mesh_cls, "hazard_%s_%d_marker" % (lane, i + 1), pos)
                m.static_mesh_component.set_static_mesh(marker)
                m.attach_to_actor(h, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                                  unreal.AttachmentRule.KEEP_WORLD, False)

    for name, (x, y, z, yaw) in PLAYER_STARTS.items():
        spawn(eas, unreal.PlayerStart, "PlayerStart_" + name, (x, y, z), (0, 0, yaw))
    spawn(eas, havoc("HavocLevelDirector"), "HavocLevelDirector", DIRECTOR_POS)
    for cls_name, (x, y, z, yaw) in VEHICLES.items():
        spawn(eas, havoc(cls_name), cls_name, (x, y, z), (0, 0, yaw))
    build_navmesh(eas)


def build_navmesh(eas):
    cls = getattr(unreal, "NavMeshBoundsVolume", None)
    if cls is None:
        warn("NavMeshBoundsVolume not available; place one over the island by hand (docs/ai.md)")
        return
    x, y, z, hx, hy, hz = NAV_BOUNDS
    # A new volume's brush is a 200 cm cube (half extent 100).
    spawn(eas, cls, "NavMeshBounds_Kharg", (x, y, z), scale=(hx / 100.0, hy / 100.0, hz / 100.0))
    log("Placed NavMeshBounds_Kharg; press P in the viewport to check the green NavMesh covers the island")


def main():
    load_map_def()
    manifest = load_manifest()
    configure_project()
    les = open_or_create_level()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clear_previous(eas)
    build_landscape(eas, manifest)
    build_water(eas)
    build_sky(eas)
    build_gameplay(eas, manifest)
    les.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Done. Level saved at %s. Press Play and check LogHavocWorld (guide Step 9)." % LEVEL_PATH)


main()
