"""Fictional oil tanker ship (visual only). blender -b -P tools/blender/build_tanker_ship.py"""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from havoc_common import Model

bones = [("root", (0, 0, 0), (0, 0, 2), None), ("hull", (0, 0, 4), (0, 10, 4), "root"),
         ("deckcrane_L", (-6, 20, 11), (-6, 20, 14), "hull"), ("deckcrane_R", (6, 20, 11), (6, 20, 14), "hull"),
         ("radar", (0, -78, 42), (0, -78, 44), "hull"), ("rudder", (0, -112, 2), (0, -112, 8), "root"),
         ("propeller", (0, -114, 4), (0, -116, 4), "root")]
m = Model("OilTanker", bones)
m.part("Hull", "box", (0, 0, 6), (28, 230, 14), "hull", "paint", uv_scale=12)
m.part("Bow", "cone", (0, 123, 6), (14.0, 14.0, 16), "hull", "paint", rot=(-math.pi / 2, 0, 0), verts=4, uv_scale=12)
m.part("Deck", "box", (0, 5, 13.5), (27, 195, 0.6), "hull", "plate", uv_scale=8)
m.part("Superstructure", "box", (0, -80, 25), (22, 24, 22), "hull", "plate", uv_scale=6)
m.part("Bridge", "box", (0, -78, 38), (24, 14, 5), "hull", "paint", uv_scale=6)
m.part("Funnel", "cyl", (0, -90, 42), (3.2, 3.2, 12), "hull", "rust", verts=20, uv_scale=3)
m.part("Radar", "box", (0, -78, 44), (6, 0.6, 1.2), "radar", "plate", uv_scale=1)
for i in range(8):    # deck pipe run and tank hatches
    m.part(f"Hatch{i}", "cyl", (0, -50 + i * 20, 14.2), (3.0, 3.0, 0.6), "hull", "rust", verts=20, uv_scale=2)
m.part("Pipe", "cyl", (0, 5, 15), (0.5, 0.5, 190), "hull", "rust", rot=(math.pi / 2, 0, 0), uv_scale=1)
for s, x, b in (("L", -6, "deckcrane_L"), ("R", 6, "deckcrane_R")):
    m.part(f"Crane{s}", "box", (x, 20, 17), (0.8, 0.8, 6), b, "plate", uv_scale=1)
m.part("Rudder", "box", (0, -112, 5), (0.5, 4, 8), "rudder", "rust", uv_scale=2)
m.part("Propeller", "cyl", (0, -114, 4), (3.2, 3.2, 0.4), "propeller", "plate", rot=(math.pi / 2, 0, 0), verts=5, uv_scale=2)
m.export()
