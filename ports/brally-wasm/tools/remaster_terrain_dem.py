#!/usr/bin/env python3
"""remaster_terrain_dem.py -- real landscape for remaster_terrain.py: swisstopo's
swissALTI3D (the open 2 m elevation model of Switzerland, free since 2021)
over a square of LV95 kilometres, fetched tile by tile from the federal STAC
service and joined into one grid.

    remaster_terrain_dem.py fetch E0 N0 E1 N1      kilometres, LV95 (e.g. 2772 1136 2796 1160)
    remaster_terrain_dem.py mosaic E0 N0 E1 N1 OUT.npy

Tiles land in ports/common/models/terrain/dem/alti2m/ (gitignored, ~1 MB a
square kilometre).  The mosaic is float32 metres above sea level, 2 m cells,
row 0 the southern edge (y up, as the game's world).
"""
import concurrent.futures, json, os, re, sys, urllib.request
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEM = os.path.join(ROOT, "ports", "common", "models", "terrain", "dem", "alti2m")
STAC = "https://data.geo.admin.ch/api/stac/v0.9/collections/ch.swisstopo.swissalti3d/items"

def lv95_to_wgs84(e, n):
    """swisstopo's approximate formulas (metre-level)"""
    y = (e - 2600000) / 1e6; x = (n - 1200000) / 1e6
    lon = 2.6779094 + 4.728982 * y + 0.791484 * y * x + 0.1306 * y * x * x - 0.0436 * y ** 3
    lat = 16.9023892 + 3.238272 * x - 0.270978 * y * y - 0.002528 * x * x - 0.0447 * y * y * x - 0.0140 * x ** 3
    return lon * 100 / 36, lat * 100 / 36

def tiles(e0, n0, e1, n1):
    lo = lv95_to_wgs84(e0 * 1000, n0 * 1000); hi = lv95_to_wgs84(e1 * 1000, n1 * 1000)
    bbox = f"{min(lo[0], hi[0]) - 0.02},{min(lo[1], hi[1]) - 0.02},{max(lo[0], hi[0]) + 0.02},{max(lo[1], hi[1]) + 0.02}"
    url = f"{STAC}?bbox={bbox}&limit=100"
    best = {}
    while url:
        d = json.load(urllib.request.urlopen(url, timeout=60))
        for f in d["features"]:
            m = re.match(r"swissalti3d_(\d{4})_(\d{4})-(\d{4})", f["id"])
            if not m:
                continue
            yr, e, n = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if not (e0 <= e < e1 and n0 <= n < n1):
                continue
            href = next((a["href"] for k, a in f["assets"].items() if k.endswith("_2_2056_5728.tif")), None)
            if href and ((e, n) not in best or best[(e, n)][0] < yr):
                best[(e, n)] = (yr, href)
        url = next((l["href"] for l in d.get("links", []) if l.get("rel") == "next"), None)
    return best

def fetch(e0, n0, e1, n1):
    os.makedirs(DEM, exist_ok=True)
    t = tiles(e0, n0, e1, n1)
    print(f"{len(t)} tiles of {(e1 - e0) * (n1 - n0)}")
    def one(kv):
        (e, n), (yr, href) = kv
        p = os.path.join(DEM, f"{e}-{n}.tif")
        if os.path.exists(p) and os.path.getsize(p) > 1000:
            return 0
        urllib.request.urlretrieve(href, p + ".part")
        os.replace(p + ".part", p)
        return 1
    with concurrent.futures.ThreadPoolExecutor(12) as ex:
        n = sum(ex.map(one, t.items()))
    print(f"fetched {n} new")

def mosaic(e0, n0, e1, n1, out):
    W, H = (e1 - e0) * 500, (n1 - n0) * 500
    a = np.full((H, W), np.nan, np.float32)
    for e in range(e0, e1):
        for n in range(n0, n1):
            p = os.path.join(DEM, f"{e}-{n}.tif")
            if not os.path.exists(p):
                continue
            t = np.array(Image.open(p), np.float32)[::-1]       # north-up file -> row 0 south
            a[(n - n0) * 500:(n - n0 + 1) * 500, (e - e0) * 500:(e - e0 + 1) * 500] = t
    bad = ~np.isfinite(a) | (a < -100)
    if bad.any():
        print(f"{bad.sum()} cells missing: filled from the nearest")
        from scipy import ndimage
        idx = ndimage.distance_transform_edt(bad, return_distances=False, return_indices=True)
        a = a[tuple(idx)]
    np.save(out, a)
    print(f"{out}: {W}x{H}, {a.min():.0f}..{a.max():.0f} m")

if __name__ == "__main__":
    cmd = sys.argv[1]
    box = [int(x) for x in sys.argv[2:6]]
    if cmd == "fetch":
        fetch(*box)
    else:
        mosaic(*box, sys.argv[6])
