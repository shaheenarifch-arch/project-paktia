"""Fictional towed artillery gun / howitzer (visual only). blender -b -P tools/blender/build_artillery.py"""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from havoc_common import Model, wheel_bones

bones = [("root", (0, 0, 0.5), (0, 0, 1), None), ("carriage", (0, 0, 0.9), (0, 1, 0.9), "root"),
         ("cradle", (0, 0.2, 1.4), (0, 1.2, 1.4), "carriage"),
         ("barrel", (0, 0.2, 1.4), (0, 4.0, 1.4), "cradle"),
         ("trail_L", (-0.3, -0.8, 0.7), (-1.6, -3.4, 0.2), "carriage"),
         ("trail_R", (0.3, -0.8, 0.7), (1.6, -3.4, 0.2), "carriage")]
bones += wheel_bones((-1.5, 1.5), (0.0,), 0.55)
m = Model("Howitzer", bones)
m.part("Axle", "cyl", (0, 0, 0.55), (0.12, 0.12, 3.0), "carriage", "plate", rot=(0, math.pi / 2, 0), uv_scale=1)
m.part("Carriage", "box", (0, 0.1, 1.0), (1.3, 1.4, 0.5), "carriage", "paint", uv_scale=2)
m.part("Shield", "box", (0, 0.9, 1.7), (2.0, 0.08, 1.1), "carriage", "plate", uv_scale=2)
m.part("Cradle", "box", (0, 0.5, 1.4), (0.7, 1.6, 0.6), "cradle", "paint", uv_scale=2)
m.part("Barrel", "cyl", (0, 2.2, 1.4), (0.14, 0.14, 4.0), "barrel", "rust", rot=(math.pi / 2, 0, 0), uv_scale=1)
m.part("MuzzleBrake", "cyl", (0, 4.25, 1.4), (0.22, 0.22, 0.5), "barrel", "plate", rot=(math.pi / 2, 0, 0), uv_scale=1)
m.part("Recoil", "cyl", (0, 1.0, 1.75), (0.08, 0.08, 1.4), "cradle", "plate", rot=(math.pi / 2, 0, 0), uv_scale=1)
m.part("TrailL", "box", (-0.95, -2.1, 0.45), (0.2, 3.2, 0.2), "trail_L", "paint", rot=(0, 0, math.radians(-22)), uv_scale=2)
m.part("TrailR", "box", (0.95, -2.1, 0.45), (0.2, 3.2, 0.2), "trail_R", "paint", rot=(0, 0, math.radians(22)), uv_scale=2)
m.mirror_wheels("Wheel", (-1.5, 1.5), (0.0,), 0.55, 0.55, 0.3, "wheel_{0}_{1}")
m.export()
