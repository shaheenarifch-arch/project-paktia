"""Fictional roadside IED: visual hazard prop ONLY (a dirt mound with a debris bundle; no
functional or construction detail). blender -b -P tools/blender/build_ied.py"""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from havoc_common import Model

m = Model("RoadsideIED", [("root", (0, 0, 0), (0, 0, 0.3), None)])
m.part("Mound", "sphere", (0, 0, 0.0), (0.9, 0.7, 0.35), "root", "sand", verts=24, uv_scale=1)
m.part("Bundle", "box", (0.1, 0.0, 0.3), (0.5, 0.35, 0.25), "root", "rust", rot=(0, 0, 0.4), uv_scale=0.6)
m.part("Rock", "sphere", (-0.5, 0.2, 0.1), (0.25, 0.2, 0.18), "root", "rock", verts=12, uv_scale=0.6)
m.part("Debris", "box", (0.6, -0.3, 0.12), (0.35, 0.12, 0.05), "root", "plate", rot=(0, 0, -0.6), uv_scale=0.5)
m.export()
