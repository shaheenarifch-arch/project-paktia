"""Make the Kharg Island 16-bit PNG heightmap from the public Copernicus DEM GLO-30.

Run with a normal Python 3 (NOT inside Unreal):
    python -m pip install rasterio numpy pillow
    python game/Content/Python/make_kharg_heightmap.py --download
    python game/Content/Python/make_kharg_heightmap.py --dem path/to/tile.tif   # use your own DEM (SRTM works too)

Output: game/Content/Maps/Kharg/Source/kharg_heightmap_r16.png (2017 x 2017, 16-bit).
Sea level is raw value 32768 (Landscape Z = 0). Water is set to -15 m so the ocean has depth.
Scale: with Landscape Scale Z = 100, 1 m = 128 raw units. build_kharg_level.py uses the same numbers.

Source and licence: see Content/Maps/Kharg/SOURCES.txt.
Landscape heightmap help: https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine
Python tools: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python
"""
import argparse
import os
import sys

LAT, LON = 29.25, 50.32          # Kharg Island
SIZE_KM = 20.0
RES = 2017                       # valid Landscape size (2017 x 2017)
TILE_URL = ("https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_N29_00_E050_00_DEM/"
            "Copernicus_DSM_COG_10_N29_00_E050_00_DEM.tif")
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "Maps", "Kharg", "Source", "kharg_heightmap_r16.png"))


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
    src = a.dem or TILE_URL
    with rasterio.open(src) as ds:
        win = from_bounds(LON - dlon, LAT - dlat, LON + dlon, LAT + dlat, ds.transform)
        elev = ds.read(1, window=win, out_shape=(RES, RES), resampling=rasterio.enums.Resampling.bilinear)
    elev = elev.astype("float64")
    elev[elev <= 0.5] = -15.0                           # sea
    raw = np.clip(32768 + elev * 128.0, 0, 65535).astype("<u2")
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    Image.fromarray(raw).save(a.out)
    print("Wrote %s (max land height %.0f m)" % (a.out, elev.max()))
    print("Record source and licence in Content/Maps/Kharg/SOURCES.txt (already pre-filled for Copernicus).")


if __name__ == "__main__":
    main()
