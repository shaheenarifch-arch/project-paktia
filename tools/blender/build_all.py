"""Run every model script headless:  python3 tools/blender/build_all.py  (needs `blender` on PATH)."""
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
for s in ("mrl_truck", "artillery", "tank_enemy", "ied", "tanker_ship"):
    subprocess.run(["blender", "-b", "-P", os.path.join(here, f"build_{s}.py")], check=True)
