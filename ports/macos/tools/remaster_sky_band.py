#!/usr/bin/env python3
"""Pack a Remastered horizon band (port code): the distant land along a
track's horizon, drawn by host_fx.m over its sky panorama.

The source is a photograph of the land with a flat pure magenta sky above
it.  The magenta is keyed out to alpha (by how far each pixel is from it,
feathered over a few levels) and pulled out of the edge pixels' colour
(despill: what remains once the key colour's share is removed), the empty
rows above the highest peak are cropped, and the result is enlarged 2x with
Lanczos.

host_fx.m lays the band round the eye in copies of 90 degrees of heading,
every other one mirrored (so each join meets its own edge), with square
pixels: the bottom row 9 degrees below the horizon, the top row as far above
as the picture's height says.

usage: remaster_sky_band.py <source.png> <out.png> [--scale N]
"""
import argparse
import numpy as np
from scipy import ndimage
from PIL import Image

KEY = np.array([1.0, 0.0, 1.0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args()
    im = np.asarray(Image.open(a.src).convert("RGB"), dtype=np.float64) / 255.0
    # magenta-ness: red and blue both high above green
    m = np.minimum(im[..., 0], im[..., 2]) - im[..., 1]
    alpha = 1.0 - np.clip((m - 0.12) / (0.4 - 0.12), 0.0, 1.0)
    # despill: the pixel as a mix of the land's colour and the key, solved
    # for the land's colour
    al = np.maximum(alpha, 1e-3)[..., None]
    rgb = np.clip((im - (1.0 - al) * KEY) / al, 0.0, 1.0)
    # the source's compression noise speckles the edge with key colour: the
    # matte is eroded about a pixel, and every pixel short of solid takes the
    # colour of the nearest solid one
    # (eroded by pulling a slightly blurred matte in by half its ramp: a box
    # erosion leaves a stair-stepped skyline)
    alpha = np.clip((ndimage.gaussian_filter(alpha, 1.0) - 0.6) / 0.4, 0.0, 1.0)
    solid = alpha >= 0.99
    _, (iy, ix) = ndimage.distance_transform_edt(~solid, return_indices=True)
    rgb = np.where(solid[..., None], rgb, rgb[iy, ix])
    rows = np.where(alpha.max(axis=1) > 0.02)[0]
    top = max(0, int(rows[0]) - 4)
    out = np.dstack([rgb, alpha])[top:]
    img = Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "RGBA")
    if a.scale > 1:
        img = img.resize((img.width * a.scale, img.height * a.scale), Image.LANCZOS)
    img.save(a.out)
    print(f"{a.out}: {img.width}x{img.height}, cropped {top} rows of sky")


if __name__ == "__main__":
    main()
