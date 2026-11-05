"""Fictional multi-barrel rocket launcher truck (visual only). blender -b -P tools/blender/build_mrl_truck.py"""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from havoc_common import Model, wheel_bones

bones = [("root", (0, 0, 0.5), (0, 0, 1), None), ("chassis", (0, 0, 1), (0, 1, 1), "root"),
         ("turret", (0, -1.6, 2.0), (0, -1.6, 2.5), "chassis"),
         ("launcher", (0, -1.6, 2.4), (0, -0.6, 2.4), "turret")]
bones += wheel_bones((-1.2, 1.2), (-2.8, -0.6, 3.0), 0.6)
m = Model("MRLTruck", bones)
m.part("Chassis", "box", (0, -0.3, 1.0), (2.4, 8.0, 0.5), "chassis", "paint", uv_scale=3)
m.part("Cab", "box", (0, 3.0, 2.0), (2.3, 2.0, 1.8), "chassis", "paint", uv_scale=3)
m.part("Engine", "box", (0, 4.3, 1.5), (2.0, 1.0, 1.0), "chassis", "rust", uv_scale=2)
m.part("TurretBase", "cyl", (0, -1.6, 1.5), (1.1, 1.1, 0.6), "turret", "plate", verts=24, uv_scale=2)
m.part("Cradle", "box", (0, -1.2, 2.3), (2.2, 3.6, 0.4), "launcher", "plate", uv_scale=3)
for r in range(3):          # 3 x 4 = 12 launcher tubes
    for c in range(4):
        m.part(f"Tube_{r}_{c}", "cyl", (-0.8 + c * 0.53, -1.2, 2.7 + r * 0.5), (0.22, 0.22, 3.4),
               "launcher", "rust", rot=(math.pi / 2, 0, 0), verts=16, uv_scale=1.5)
m.mirror_wheels("Wheel", (-1.2, 1.2), (-2.8, -0.6, 3.0), 0.6, 0.6, 0.45, "wheel_{0}_{1}")
m.export()
