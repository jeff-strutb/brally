#!/usr/bin/env python3
"""The Remastered meadow grass's blades (host_terrain.m): photoscanned
grass leaves (Poly Haven grass_medium_02, CC0) cut out of the scan's atlas
one by one and straightened into upright strips, tip at the top, side by
side in one 1024x1024 sheet of 96-texel columns:

  grass_blades_col.png   the leaves' colour
  grass_blades_dry.png   the same leaves dried (the scan's own dry variant)
  grass_blades_nma.png   the leaves' normal (rg) and their outline (b)

Each leaf's centre line is found row by row and smoothed, and the strip is
resampled along it, so a curved leaf in the scan becomes a straight one the
shader can bend however it likes.  Colour is grown out past the outline so
filtering at the edge never picks up the atlas's empty background.

usage: remaster_grass_atlas.py   (downloads the 2k maps once)
"""
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ENV = os.path.join(HERE, '..', '..', 'common', 'models', 'env', 'polyhaven')
SRC = os.path.join(ENV, 'models', 'grass_medium_02')
OUT = os.path.join(ENV, 'textures', 'grass_blades')
ASSET = 'grass_medium_02'
MAPS = {'Alpha': 'alpha', 'Diffuse': 'diff', 'dry_diff': 'dry', 'nor_gl': 'nor'}
COL, SIZE = 96, 1024
# the scan's separate leaves, by their label in the alpha map (the larger
# ones; 17 lies on its side)
KEEP = [1, 2, 3, 5, 6, 9, 10, 12, 14, 17]


def fetch():
    os.makedirs(SRC, exist_ok=True)
    files = None
    for k, v in MAPS.items():
        p = os.path.join(SRC, f'{ASSET}_{v}_2k.png')
        if os.path.exists(p):
            continue
        if files is None:
            with urllib.request.urlopen(f'https://api.polyhaven.com/files/{ASSET}') as r:
                files = json.load(r)
        print('fetch', p)
        urllib.request.urlretrieve(files[k]['2k']['png']['url'], p)


def load(v, mode):
    return np.array(Image.open(os.path.join(SRC, f'{ASSET}_{v}_2k.png')).convert(mode)).astype(np.float32)


def main():
    fetch()
    A = load('alpha', 'L')
    D, Y, N = load('diff', 'RGB'), load('dry', 'RGB'), load('nor', 'RGB')
    lab, _ = ndimage.label(A > 64)
    objs = ndimage.find_objects(lab)
    sheet = np.zeros((SIZE, SIZE, 10), np.float32)
    for col, k in enumerate(KEEP):
        s, m = objs[k - 1], lab == k
        a, d, y, n = A, D, Y, N
        if k == 17:
            m, a, d, y, n = m.T, A.T, D.transpose(1, 0, 2), Y.transpose(1, 0, 2), N.transpose(1, 0, 2)
            s = (s[1], s[0])
        r0, r1 = s[0].start, s[0].stop
        c = np.full(r1 - r0, np.nan)
        w = []
        for r in range(r0, r1):
            xs = np.nonzero(m[r])[0]
            if len(xs):
                c[r - r0] = (xs.min() + xs.max()) / 2
                w.append(xs.max() - xs.min() + 1)
        i = np.arange(len(c))
        c = ndimage.gaussian_filter1d(np.interp(i, i[~np.isnan(c)], c[~np.isnan(c)]), 6)
        half = np.percentile(w, 98) / 2 * 1.15
        strip = np.zeros((SIZE, COL, 10), np.float32)
        for j in range(SIZE):
            ri = min(max(int(round(r0 + j / (SIZE - 1) * (len(c) - 1))), r0), r1 - 1)
            xi = np.clip(np.round(c[ri - r0] + (np.arange(COL) - (COL - 1) / 2) / ((COL - 1) / 2) * half).astype(int),
                         0, A.shape[1] - 1)
            strip[j, :, 0] = a[ri, xi] * m[ri, xi]
            strip[j, :, 1:4], strip[j, :, 4:7], strip[j, :, 7:10] = d[ri, xi], y[ri, xi], n[ri, xi]
        if strip[:SIZE // 4, :, 0].sum() > strip[-SIZE // 4:, :, 0].sum():
            strip = strip[::-1]                       # the root, the wider end, at the bottom
        sheet[:, col * COL:(col + 1) * COL] = strip
    inside = sheet[:, :, 0] > 64
    _, idx = ndimage.distance_transform_edt(~inside, return_indices=True)
    for ch in range(1, 10):
        sheet[:, :, ch] = sheet[:, :, ch][idx[0], idx[1]]
    os.makedirs(OUT, exist_ok=True)
    u8 = lambda x: np.clip(x, 0, 255).astype(np.uint8)
    Image.fromarray(u8(sheet[:, :, 1:4])).save(os.path.join(OUT, 'grass_blades_col.png'))
    Image.fromarray(u8(sheet[:, :, 4:7])).save(os.path.join(OUT, 'grass_blades_dry.png'))
    Image.fromarray(u8(np.dstack([sheet[:, :, 7:9], sheet[:, :, 0:1]]))).save(os.path.join(OUT, 'grass_blades_nma.png'))
    print('wrote', OUT, len(KEEP), 'leaves')


if __name__ == '__main__':
    sys.exit(main())
