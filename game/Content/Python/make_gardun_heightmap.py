"""Make the Gardun heightmap and terrain mesh from the public Copernicus DEM GLO-30.

Gardun is fictional, so the terrain is borrowed from real geography: the high arid valley and mountain ring
around Gardez, Paktia, Afghanistan (33.6 N, 69.2 E). Heights are re-based so the 2.5 km city core sits near
Z = 0 (the DEM is about 2,300 m above sea level; Unreal's 16-bit range could not hold that at useful scale).

Run with a normal Python 3 (NOT inside Unreal):
    python -m pip install rasterio numpy pillow
    python game/Content/Python/make_gardun_heightmap.py --download
    python game/Content/Python/make_gardun_heightmap.py --dem path/to/tile.tif   # your own DEM (SRTM works too)

Output (game/Content/Maps/Gardun/Source/, not committed):
    gardun_heightmap_r16.png  2017 x 2017, 16-bit. Raw 32768 = Z 0, 1 m = 128 raw units (Landscape Scale Z 100).
    gardun_terrain.obj        401 x 401 grid mesh in cm, Z up, for build_gardun_level.py.
    gardun_terrain.json       the area and base elevation that were used (read back by the builder).
Area: 7 km x 7 km. Unreal layout (cm): x -350000..350000 (east +), y -250000..450000 (south +, north is -Y,
as in docs/gardun-map.md). Row 0 of the DEM is north, so row 0 is y = -250000.

Source and licence: see Content/Maps/Gardun/SOURCES.txt.
Landscape heightmap help: https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine
Python tools: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python
"""
import argparse
import json
import os
import sys

LAT, LON = 33.60, 69.22          # centre of the 7 km square (Gardez valley); the Unreal origin is 1 km north of it
SIZE_M = 7000.0
RES = 2017                       # valid Landscape size
MESH_RES = 401                   # 401 x 401 verts, 17.5 m spacing
X_MIN, Y_MIN = -350000.0, -250000.0   # cm, north-west corner of the square in Unreal space
CORE_HALF = 125000.0             # city core half size (cm); its median height becomes Z = 0
TILE_URL = ("https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_N33_00_E069_00_DEM/"
            "Copernicus_DSM_COG_10_N33_00_E069_00_DEM.tif")
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "Maps", "Gardun", "Source", "gardun_heightmap_r16.png"))


def core_median(elev):
    """Median DEM height (m) inside the city core, which is centred on Unreal (0, 0)."""
    import numpy as np
    n = elev.shape[0]
    cm_per_px = SIZE_M * 100.0 / (n - 1)
    c0 = int((0 - CORE_HALF - X_MIN) / cm_per_px)
    c1 = int((0 + CORE_HALF - X_MIN) / cm_per_px)
    r0 = int((0 - CORE_HALF - Y_MIN) / cm_per_px)
    r1 = int((0 + CORE_HALF - Y_MIN) / cm_per_px)
    return float(np.median(elev[r0:r1 + 1, c0:c1 + 1]))


def write_terrain_obj(elev, path):
    """Grid mesh (OBJ, cm, Z up). Row 0 is north (-Y). Import help:
    https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-assets-using-interchange-in-unreal-engine"""
    import numpy as np
    n = MESH_RES
    idx = np.linspace(0, elev.shape[0] - 1, n).round().astype(int)
    h = elev[np.ix_(idx, idx)] * 100.0
    xs = np.linspace(X_MIN, X_MIN + SIZE_M * 100.0, n)
    ys = np.linspace(Y_MIN, Y_MIN + SIZE_M * 100.0, n)
    with open(path, "w") as f:
        f.write("# Gardun terrain, %d x %d grid, cm, Z up. Made by make_gardun_heightmap.py\n" % (n, n))
        f.write("o GardunTerrain\n")
        for r in range(n):
            for c in range(n):
                f.write("v %.0f %.0f %.0f\n" % (xs[c], ys[r], h[r, c]))
        for r in range(n):
            for c in range(n):
                f.write("vt %.5f %.5f\n" % (c / (n - 1) * 64, r / (n - 1) * 64))
        for r in range(n - 1):
            for c in range(n - 1):
                a = r * n + c + 1
                b, d, e = a + 1, a + n, a + n + 1
                # Y grows with the row, so wind the opposite way to a north-up (+Y north) grid.
                f.write("f %d/%d %d/%d %d/%d\n" % (a, a, b, b, d, d))
                f.write("f %d/%d %d/%d %d/%d\n" % (b, b, e, e, d, d))
    print("Wrote %s (%d x %d terrain mesh)" % (path, n, n))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--download", action="store_true", help="read the open Copernicus tile over HTTPS")
    ap.add_argument("--dem", help="local GeoTIFF instead of downloading")
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    if not (a.download or a.dem):
        ap.error("pass --download or --dem <file.tif>")
    try:
        import numpy as np
        import rasterio
        from rasterio.windows import from_bounds
        from PIL import Image
    except ImportError as e:
        sys.exit("Missing package (%s). Run: python -m pip install rasterio numpy pillow" % e)

    dlat = SIZE_M / 1000 / 2 / 111.32
    dlon = SIZE_M / 1000 / 2 / (111.32 * np.cos(np.radians(LAT)))
    src = a.dem or TILE_URL
    with rasterio.open(src) as ds:
        win = from_bounds(LON - dlon, LAT - dlat, LON + dlon, LAT + dlat, ds.transform)
        elev = ds.read(1, window=win, out_shape=(RES, RES), resampling=rasterio.enums.Resampling.bilinear)
    elev = elev.astype("float64")
    base = core_median(elev)
    rel = elev - base
    raw = np.clip(32768 + rel * 128.0, 0, 65535).astype("<u2")
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    Image.fromarray(raw).save(a.out)
    print("Wrote %s (base %.0f m, relief %.0f to %.0f m)" % (a.out, base, rel.min(), rel.max()))
    if rel.min() < -255 or rel.max() > 255:
        print("Warning: relief exceeds the 16-bit range at Scale Z 100 (+-256 m); heights are clipped in the PNG (the OBJ is not).")
    out_dir = os.path.dirname(a.out)
    write_terrain_obj(rel, os.path.join(out_dir, "gardun_terrain.obj"))
    with open(os.path.join(out_dir, "gardun_terrain.json"), "w") as f:
        json.dump({"dataset": "Copernicus DEM GLO-30", "tile": os.path.basename(src), "lat": LAT, "lon": LON,
                   "size_m": SIZE_M, "base_elevation_m": base, "x_min_cm": X_MIN, "y_min_cm": Y_MIN}, f, indent=2)
    print("Source and licence are recorded in Content/Maps/Gardun/SOURCES.txt.")


if __name__ == "__main__":
    main()
