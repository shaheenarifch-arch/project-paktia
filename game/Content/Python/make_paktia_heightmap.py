"""Make the Paktia Highlands heightmap and terrain mesh from the public Copernicus DEM GLO-30.

Run with a normal Python 3 (NOT inside Unreal):
    python -m pip install rasterio numpy pillow
    python game/Content/Python/make_paktia_heightmap.py --download
    python game/Content/Python/make_paktia_heightmap.py --dem path/to/tile.tif   # your own DEM (SRTM works too)

Output (game/Content/Maps/PaktiaHighlands/Source/):
  paktia_heightmap_r16.png  2017 x 2017, 16-bit; the lowest point of the area is raw 0 (1 m = 128 raw at Landscape Z scale 100)
  paktia_terrain.obj        401 x 401 grid mesh, cm, Z up, centred on (0, 0): what build_paktia_highlands_level.py imports
Source and licence: Content/Maps/PaktiaHighlands/SOURCES.txt.
Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine
"""
import argparse
import os
import sys

LAT, LON = 33.60, 69.25          # Paktia province highlands
SIZE_KM = 20.0
RES = 2017
MESH_RES = 401                   # 50 m spacing over 20 km
TILE_URL = ("https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_N33_00_E069_00_DEM/"
            "Copernicus_DSM_COG_10_N33_00_E069_00_DEM.tif")
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "Maps", "PaktiaHighlands", "Source", "paktia_heightmap_r16.png"))


def write_terrain_obj(elev, path):
    import numpy as np
    n = MESH_RES
    idx = np.linspace(0, elev.shape[0] - 1, n).round().astype(int)
    h = elev[np.ix_(idx, idx)] * 100.0                  # metres -> cm
    half = SIZE_KM * 100000.0 / 2
    xs = np.linspace(-half, half, n)
    ys = np.linspace(half, -half, n)                    # row 0 is north
    with open(path, "w") as f:
        f.write("# Paktia Highlands terrain, %d x %d grid, cm, Z up. Made by make_paktia_heightmap.py\n" % (n, n))
        f.write("o PaktiaTerrain\n")
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
                f.write("f %d/%d %d/%d %d/%d\n" % (a, a, d, d, b, b))
                f.write("f %d/%d %d/%d %d/%d\n" % (b, b, d, d, e, e))
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

    dlat = SIZE_KM / 2 / 111.32
    dlon = SIZE_KM / 2 / (111.32 * np.cos(np.radians(LAT)))
    with rasterio.open(a.dem or TILE_URL) as ds:
        win = from_bounds(LON - dlon, LAT - dlat, LON + dlon, LAT + dlat, ds.transform)
        elev = ds.read(1, window=win, out_shape=(RES, RES), resampling=rasterio.enums.Resampling.bilinear)
    elev = elev.astype("float64")
    elev -= elev.min()                                  # lowest point = 0 m
    raw = np.clip(elev * 128.0, 0, 65535).astype("<u2")
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    Image.fromarray(raw).save(a.out)
    print("Wrote %s (relief %.0f m)" % (a.out, elev.max()))
    write_terrain_obj(elev, os.path.join(os.path.dirname(a.out), "paktia_terrain.obj"))


if __name__ == "__main__":
    main()
