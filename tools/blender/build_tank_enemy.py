"""Fictional enemy main battle tank (visual only). blender -b -P tools/blender/build_tank_enemy.py"""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from havoc_common import Model

bones = [("root", (0, 0, 0.5), (0, 0, 1), None), ("hull", (0, 0, 1), (0, 1, 1), "root"),
         ("turret", (0, 0.2, 2.1), (0, 0.2, 2.7), "hull"), ("barrel", (0, 1.2, 2.4), (0, 4.5, 2.4), "turret"),
         ("track_L", (-1.6, 0, 0.7), (-1.6, 1, 0.7), "hull"), ("track_R", (1.6, 0, 0.7), (1.6, 1, 0.7), "hull")]
m = Model("EnemyTank", bones)
m.part("Hull", "box", (0, 0, 1.2), (2.4, 6.4, 0.9), "hull", "paint", uv_scale=3)
m.part("Glacis", "box", (0, 3.3, 1.35), (2.3, 1.2, 0.5), "hull", "paint", rot=(math.radians(-25), 0, 0), uv_scale=3)
for s, x, b in (("L", -1.6, "track_L"), ("R", 1.6, "track_R")):
    m.part(f"Track{s}", "box", (x, 0, 0.7), (0.8, 6.8, 1.0), b, "rubber", uv_scale=2)
    m.part(f"Skirt{s}", "box", (x * 1.07, 0, 1.3), (0.1, 5.6, 0.7), "hull", "rust", uv_scale=2)
    for i in range(6):
        m.part(f"RoadWheel{s}{i}", "cyl", (x, -2.6 + i * 1.05, 0.65), (0.5, 0.5, 0.7), b, "plate",
               rot=(0, math.pi / 2, 0), verts=20, uv_scale=1)
m.part("Turret", "box", (0, 0.2, 2.1), (2.0, 2.8, 0.8), "turret", "paint", uv_scale=3)
m.part("Mantlet", "box", (0, 1.5, 2.4), (0.9, 0.5, 0.6), "turret", "plate", uv_scale=1)
m.part("Barrel", "cyl", (0, 3.0, 2.4), (0.11, 0.11, 3.4), "barrel", "rust", rot=(math.pi / 2, 0, 0), uv_scale=1)
m.part("Cupola", "cyl", (-0.5, -0.4, 2.65), (0.35, 0.35, 0.25), "turret", "plate", verts=20, uv_scale=1)
m.export()
