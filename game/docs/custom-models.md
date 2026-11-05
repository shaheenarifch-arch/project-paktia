# Custom Blender models

Models Fab has no good free asset for, built from code so they can be rebuilt and revised.
Fictional names, visual-only shapes; no real-world specs or targeting data. Episode 04 keeps
`real_world_hypothetical` rules; ISIS stays AI-only.

| Model | Script | Output | Asset role | Moving parts (bones) |
| --- | --- | --- | --- | --- |
| Multi-barrel rocket launcher truck | `tools/blender/build_mrl_truck.py` | `Content/Custom/MRLTruck/SK_MRLTruck.fbx` | `Vehicle.MRL.Enemy` | `turret`, `launcher` (12 tubes), `wheel_<side>_<row>` x6 |
| Towed artillery gun / howitzer | `build_artillery.py` | `Content/Custom/Howitzer/SK_Howitzer.fbx` | `Vehicle.Artillery` | `cradle`, `barrel`, `trail_L/R`, `wheel_*` |
| Enemy main battle tank | `build_tank_enemy.py` | `Content/Custom/EnemyTank/SK_EnemyTank.fbx` | `Vehicle.Tank.Enemy` | `turret`, `barrel`, `track_L/R` (road wheels weighted to tracks) |
| Roadside IED (hazard prop only) | `build_ied.py` | `Content/Custom/RoadsideIED/SK_RoadsideIED.fbx` | `Props.IED` | `root` only |
| Oil tanker ship | `build_tanker_ship.py` | `Content/Custom/OilTanker/SK_OilTanker.fbx` | `Vessel.Tanker` | `deckcrane_L/R`, `radar`, `rudder`, `propeller` |

Each part is rigidly weighted to one bone (vertex group + armature), so bones can be driven in
an Animation Blueprint or Control Rig. Wheel bones follow `wheel_<x index>_<row index>`.

## Textures

Real CC0 photo-scanned materials only (Poly Haven; Megascans from the Fab library are also
allowed). The scripts download the 1k diffuse / normal / roughness maps from the Poly Haven API on
first run, so the first build needs internet access. Sources and licences are in
`Content/Custom/SOURCES.txt`; add a row there whenever you change a texture in
`tools/blender/havoc_common.py` (`TEXTURES`).

## Build

Needs Blender 4.x (<https://www.blender.org/download/>); API: <https://docs.blender.org/api/current/>.

```bash
blender -b -P tools/blender/build_tank_enemy.py      # one model
python3 tools/blender/build_all.py                    # all five
```

Each run writes the FBX plus texture copies to `game/Content/Custom/<Model>/` and a preview
render to `game/docs/renders/<Model>.png` (Workbench, works headless). Edit the script and re-run
to revise a model; sizes are in metres.

## Import into UE 5.5

1. In the Content Browser open `Content/Custom/<Model>/`, then **Import** the `SK_<Model>.fbx`
   (or drag it in). Choose **Skeletal Mesh**, keep **Import Mesh** and **Import Textures** on,
   leave **Import Animations** off.
2. Build the materials from the copied JPGs (base colour sRGB; normal map and roughness with
   compression Normal / Masks, sRGB off), or let the FBX material import do it and fix the flags.
3. Right-click the mesh > **Copy Reference** and paste the path into `Config/HavocAssets.ini`
   if it differs from the default role path (roles already point at `/Game/Custom/...`).

Help: <https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-content-pipeline> ·
<https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-skeletal-meshes-in-unreal-engine>

## Status

Scripts were written without Blender available to the author's environment, so the FBX files and
render screenshots have not been generated yet. Run `build_all.py` and commit the outputs from
`Content/Custom/` and `docs/renders/` (FBX/JPG/PNG are covered by Git LFS if configured).
