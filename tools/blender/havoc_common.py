"""Shared helpers for the Havoc custom-model scripts (Blender 4.x Python API).

Run a model script headless, e.g.:
    blender -b -P tools/blender/build_tank_enemy.py
Docs: https://docs.blender.org/api/current/

Textures are real CC0 photo-scanned materials from Poly Haven, downloaded on first run
through the Poly Haven API (https://api.polyhaven.com) into game/Content/Custom/_textures/.
No drawn, flat-colour or placeholder art: a missing texture aborts the build.
Shapes are fictional and visual-only; no real-world specs.
"""
import json
import math
import os
import sys
import urllib.request

import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CUSTOM = os.path.join(ROOT, "game", "Content", "Custom")
TEX_DIR = os.path.join(CUSTOM, "_textures")
RENDER_DIR = os.path.join(ROOT, "game", "docs", "renders")
API = "https://api.polyhaven.com/files/"
UA = {"User-Agent": "havoc-blender-tools/1.0"}

# Poly Haven id per material role (all CC0). Keep in sync with Content/Custom/SOURCES.txt.
TEXTURES = {
    "paint": "green_metal_rust",
    "rust": "rusty_metal_02",
    "plate": "metal_plate",
    "rubber": "asphalt_02",
    "sand": "sand_01",
    "rock": "rock_boulder_dry",
}
MAPS = {"diff": "Diffuse", "nor": "nor_gl", "rough": "Rough"}


def _get(url, dest):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r, open(dest, "wb") as f:
        f.write(r.read())


def fetch_texture(tex_id):
    """Download the 1k jpg diffuse / GL normal / roughness maps; return {map: path}."""
    os.makedirs(TEX_DIR, exist_ok=True)
    paths = {k: os.path.join(TEX_DIR, f"{tex_id}_{k}_1k.jpg") for k in MAPS}
    if all(os.path.exists(p) for p in paths.values()):
        return paths
    req = urllib.request.Request(API + tex_id, headers=UA)
    files = json.load(urllib.request.urlopen(req, timeout=60))
    for k, key in MAPS.items():
        _get(files[key]["1k"]["jpg"]["url"], paths[k])
    return paths


_mat_cache = {}


def material(role):
    if role in _mat_cache:
        return _mat_cache[role]
    tex_id = TEXTURES[role]
    p = fetch_texture(tex_id)
    m = bpy.data.materials.new(f"M_{tex_id}")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]

    def img(path, noncolor):
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(path)
        if noncolor:
            n.image.colorspace_settings.name = "Non-Color"
        return n

    d, r, nm = img(p["diff"], False), img(p["rough"], True), img(p["nor"], True)
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(d.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(r.outputs["Color"], bsdf.inputs["Roughness"])
    nt.links.new(nm.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    _mat_cache[role] = m
    return m


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mat_cache.clear()


class Model:
    """Rigid-skinned model: each part is weighted 100% to one bone (turret, barrel, wheel...)."""

    def __init__(self, name, bones):
        """bones: list of (bone_name, head, tail, parent_name|None)."""
        reset()
        self.name = name
        arm = bpy.data.armatures.new(f"{name}_Armature")
        self.rig = bpy.data.objects.new(f"{name}_Rig", arm)
        bpy.context.scene.collection.objects.link(self.rig)
        bpy.context.view_layer.objects.active = self.rig
        bpy.ops.object.mode_set(mode="EDIT")
        for bn, head, tail, parent in bones:
            b = arm.edit_bones.new(bn)
            b.head, b.tail = Vector(head), Vector(tail)
            if parent:
                b.parent = arm.edit_bones[parent]
        bpy.ops.object.mode_set(mode="OBJECT")
        self.parts = []

    def part(self, name, kind, loc, size, bone, mat, rot=(0, 0, 0), verts=16, uv_scale=1.0):
        """kind: box | cyl (size = (radius, radius, depth)) | cone | sphere. Size is full extents for box."""
        add = bpy.ops.mesh
        if kind == "box":
            add.primitive_cube_add(size=1)
        elif kind == "cyl":
            add.primitive_cylinder_add(vertices=verts, radius=1, depth=1)
        elif kind == "cone":
            add.primitive_cone_add(vertices=verts, radius1=1, radius2=0, depth=1)
        elif kind == "sphere":
            add.primitive_uv_sphere_add(segments=verts, ring_count=max(8, verts // 2), radius=1)
        else:
            raise ValueError(kind)
        o = bpy.context.active_object
        o.name = f"{self.name}_{name}"
        o.location, o.rotation_euler = loc, rot
        o.scale = size
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        # Box-project UVs at a real-world-ish scale so the photo textures are not stretched.
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.cube_project(cube_size=2.0 / uv_scale)
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.shade_smooth()
        o.data.materials.append(material(mat))
        o.vertex_groups.new(name=bone).add(list(range(len(o.data.vertices))), 1.0, "REPLACE")
        mod = o.modifiers.new("Armature", "ARMATURE")
        mod.object = self.rig
        o.parent = self.rig
        self.parts.append(o)
        return o

    def mirror_wheels(self, prefix, xs, ys, z, radius, width, bone_fmt, mat="rubber"):
        for xi, x in enumerate(xs):
            for yi, y in enumerate(ys):
                self.part(f"{prefix}_{xi}_{yi}", "cyl", (x, y, z), (radius, radius, width),
                          bone_fmt.format(xi, yi), mat, rot=(0, math.pi / 2, 0), verts=24)

    def export(self, render=True):
        out = os.path.join(CUSTOM, self.name)
        os.makedirs(out, exist_ok=True)
        bpy.ops.object.select_all(action="DESELECT")
        for o in [self.rig] + self.parts:
            o.select_set(True)
        bpy.ops.export_scene.fbx(
            filepath=os.path.join(out, f"SK_{self.name}.fbx"), use_selection=True,
            path_mode="COPY", embed_textures=False, add_leaf_bones=False,
            bake_anim=False, apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
            object_types={"ARMATURE", "MESH"}, mesh_smooth_type="FACE")
        # Texture copies next to the FBX so UE imports find them (path_mode=COPY writes them here).
        for role in set(_mat_cache):
            for p in fetch_texture(TEXTURES[role]).values():
                dst = os.path.join(out, os.path.basename(p))
                if not os.path.exists(dst):
                    with open(p, "rb") as s, open(dst, "wb") as d:
                        d.write(s.read())
        print(f"[havoc] exported {out}")
        if render:
            self.render()

    def render(self):
        """Workbench texture-colour preview PNG (works headless, no GPU needed)."""
        os.makedirs(RENDER_DIR, exist_ok=True)
        sc = bpy.context.scene
        sc.render.engine = "BLENDER_WORKBENCH"
        sh = sc.display.shading
        sh.light, sh.color_type = "STUDIO", "TEXTURE"
        sc.render.resolution_x, sc.render.resolution_y = 1280, 720
        pts = [o.matrix_world @ Vector(c) for o in self.parts for c in o.bound_box]
        lo = Vector(map(min, zip(*pts)))
        hi = Vector(map(max, zip(*pts)))
        ctr, size = (lo + hi) / 2, max((hi - lo).length, 1.0)
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        sc.collection.objects.link(cam)
        cam.location = ctr + Vector((0.9, -1.1, 0.6)) * size
        cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
        sc.camera = cam
        sc.render.filepath = os.path.join(RENDER_DIR, f"{self.name}.png")
        bpy.ops.render.render(write_still=True)


def wheel_bones(xs, ys, z, parent="chassis", fmt="wheel_{0}_{1}"):
    """Bone list entries for a wheel grid; pairs with Model.mirror_wheels(bone_fmt=fmt)."""
    return [(fmt.format(i, j), (x, y, z), (x * 1.1 if x else 0.3, y, z), parent)
            for i, x in enumerate(xs) for j, y in enumerate(ys)]


def script_args():
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
