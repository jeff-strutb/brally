#!/usr/bin/env python3
"""lightviz.py -- a picture of what the lighting's capture rebuilt from a frame.

    lightviz.py DUMP.obj OUT.png

DUMP.obj is BR_LIGHT_DUMP's (platform/light/light_capture.c): the frame's
camera triangles in world space, the game's camera, and the frame the game
drew beside it (DUMP.obj.png).  Three panels, left to right: the game's
frame; the captured triangles drawn again through the game's camera, lit
flat by a sun from their own face normals (their colour the game's vertex
colour); the same triangles from a raised viewpoint behind and above the
camera, to show they are real 3D geometry.  A plain z-buffer rasteriser in
numpy and Pillow; no other dependency.
"""
import sys

import numpy as np
from PIL import Image

W, H = 640, 480
SUN = np.array([3.0, 1.0, 1.1]) / np.linalg.norm([3.0, 1.0, 1.1])


def load(path):
    vp, vpt, v, c = None, None, [], []
    for line in open(path):
        if line.startswith('# camera vp'):
            vp = np.array(list(map(float, line.split()[3:19]))).reshape(4, 4)
        elif line.startswith('# camera viewport'):
            vpt = list(map(float, line.split()[3:7]))
        elif line.startswith('v '):
            f = list(map(float, line.split()[1:]))
            v.append(f[:3])
            c.append(f[3:6])
    return vp, vpt, np.array(v).reshape(-1, 3, 3), np.array(c).reshape(-1, 3, 3)


def raster(tris, cols, project, w=W, h=H):
    """tris N x 3 x 3 world, cols N x 3 colour 0..1; project(p) -> (x, y, depth)"""
    img = np.full((h, w, 3), 0.08)
    zb = np.full((h, w), np.inf)
    for t, col in zip(tris, cols):
        xy = [project(p) for p in t]
        if any(q is None for q in xy):
            continue
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = xy
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            continue
        lo_x, hi_x = int(max(0, np.floor(min(x0, x1, x2)))), int(min(w - 1, np.ceil(max(x0, x1, x2))))
        lo_y, hi_y = int(max(0, np.floor(min(y0, y1, y2)))), int(min(h - 1, np.ceil(max(y0, y1, y2))))
        if lo_x > hi_x or lo_y > hi_y:
            continue
        ys, xs = np.mgrid[lo_y:hi_y + 1, lo_x:hi_x + 1]
        px, py = xs + 0.5, ys + 0.5
        b0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
        b1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
        b2 = 1 - b0 - b1
        inside = (b0 >= 0) & (b1 >= 0) & (b2 >= 0)
        z = b0 * z0 + b1 * z1 + b2 * z2
        sub = zb[lo_y:hi_y + 1, lo_x:hi_x + 1]
        win = inside & (z < sub)
        sub[win] = z[win]
        img[lo_y:hi_y + 1, lo_x:hi_x + 1][win] = col
    return (np.clip(img, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)


def shade(tris, vcol):
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    n *= np.where(n[:, 2:3] < 0, -1, 1)                 # facing up or toward the sky
    lam = np.clip(n @ SUN, 0, 1)[:, None]
    base = np.mean(vcol, axis=1)
    base = np.where(base.max(axis=1, keepdims=True) < 0.02, 0.7, base)   # no vertex colour: grey
    return base * (0.25 + 0.75 * lam)


def main():
    path, out = sys.argv[1], sys.argv[2]
    vp, (sx, ox, sy, oy), tris, vcol = load(path)
    cols = shade(tris, vcol)

    def game_cam(p):
        c = np.append(p, 1.0) @ vp
        if c[3] <= 1e-6:
            return None
        return c[0] / c[3] * sx + ox, c[1] / c[3] * sy + oy, c[3]

    # the camera's position and forward from its view x projection: the
    # point clip maps to w = 0 with x = y = 0, and the direction w grows along
    m3 = vp[:3, [0, 1, 3]].T
    eye = np.linalg.solve(m3, -vp[3, [0, 1, 3]])
    fwd = vp[:3, 3] / np.linalg.norm(vp[:3, 3])
    up = np.array([0.0, 0.0, 1.0])
    pos = eye - fwd * 40 + up * 25
    look = eye + fwd * 30
    f = look - pos
    f /= np.linalg.norm(f)
    r = np.cross(f, up)
    r /= np.linalg.norm(r)
    u = np.cross(r, f)

    def raised(p):
        d = p - pos
        z = d @ f
        if z <= 0.5:
            return None
        k = (W / 2) / np.tan(np.radians(35))
        return W / 2 + (d @ r) / z * k, H / 2 - (d @ u) / z * k, z

    panels = [Image.open(path + '.png').convert('RGB').resize((W, H)),
              Image.fromarray(raster(tris, cols, game_cam)),
              Image.fromarray(raster(tris, cols, raised))]
    sheet = Image.new('RGB', (W * 3 + 16, H), (20, 20, 20))
    for i, p in enumerate(panels):
        sheet.paste(p, (i * (W + 8), 0))
    sheet.save(out)
    print('%s: %d triangles, camera at %.1f %.1f %.1f' % (out, len(tris), *eye))


if __name__ == '__main__':
    main()
