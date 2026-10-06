#!/usr/bin/env python3
"""Pack the Remastered skies (port code, not byte-matched).

Each track's source sky is one wide photograph: the horizon along its bottom
edge, open sky above.  host_fx.m wraps it around the eye twice (each copy
spans 180 degrees of heading, elevation 0 at the bottom row to SKY_TOP at the
top row), so the picture's right edge must run on into its left edge.  That
join is made here: the last OV columns overlap the first OV, and each row
switches from one to the other along the path through the overlap where the
two differ least (a minimum-error boundary cut), feathered a few pixels.  The
result is enlarged 2x with Lanczos so the GPU's bilinear magnification starts
from a smoother picture.

usage: remaster_sky.py [--src DIR] [--out DIR] [--scale N]
  src: <env>_<weather>.png for every env in ENVS and weather in WEATHERS (default ports/common/models/sky/source)
  out: <env>_<weather>.png, sRGB RGB (default ports/common/models/sky/pack)
"""
import argparse, os
import numpy as np
from PIL import Image

ENVS = ["desert", "mountain", "coast", "mine", "amazon", "race", "bonus"]
# one sky per weather, in the order of g_brCarPhysWeather (0x104B15E8):
# sunny, fog, storm, snow, rain at night
WEATHERS = ["clear", "fog", "storm", "snow", "night"]


def seam_path(err):
    """Vertical path of least total error through err (h x w): column per row."""
    h, w = err.shape
    cost = err.copy()
    for y in range(1, h):
        prev = cost[y - 1]
        left = np.concatenate(([np.inf], prev[:-1]))
        right = np.concatenate((prev[1:], [np.inf]))
        cost[y] += np.minimum(prev, np.minimum(left, right))
    path = np.zeros(h, dtype=int)
    path[-1] = int(np.argmin(cost[-1]))
    for y in range(h - 2, -1, -1):
        x = path[y + 1]
        lo, hi = max(0, x - 1), min(w, x + 2)
        path[y] = lo + int(np.argmin(cost[y, lo:hi]))
    return path


def wrap(img, ov, feather=6):
    h, w, _ = img.shape
    a = img[:, w - ov:]          # what runs on past the right edge
    b = img[:, :ov]              # the left edge it has to meet
    err = ((a - b) ** 2).sum(axis=2)
    cut = seam_path(err)
    x = np.arange(ov)[None, :]
    t = np.clip((x - cut[:, None] + feather) / (2.0 * feather), 0.0, 1.0)[..., None]
    out = img[:, :w - ov].copy()
    out[:, :ov] = a * (1.0 - t) + b * t
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="ports/common/models/sky/source")
    ap.add_argument("--out", default="ports/common/models/sky/pack")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--overlap", type=float, default=1.0 / 6.0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for env in [e + "_" + w for e in ENVS for w in WEATHERS]:
        src = os.path.join(a.src, env + ".png")
        if not os.path.exists(src):
            print(f"sky: no {src}, skipped")
            continue
        img = np.asarray(Image.open(src).convert("RGB"), dtype=np.float64) / 255.0
        ov = int(img.shape[1] * a.overlap)
        t = wrap(img, ov)
        im = Image.fromarray(np.clip(t * 255.0 + 0.5, 0, 255).astype(np.uint8))
        if a.scale != 1:
            im = im.resize((im.width * a.scale, im.height * a.scale), Image.LANCZOS)
        dst = os.path.join(a.out, env + ".png")
        im.save(dst, optimize=True)
        print(f"sky: {env} {im.width}x{im.height} -> {dst}")


if __name__ == "__main__":
    main()
