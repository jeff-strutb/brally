#!/usr/bin/env python3
"""remaster_livery.py -- the Remastered car's livery, drawn as a design.

    remaster_livery.py <pack dir>

The original ES wears red, white and blue ribbons over its paint: an arch
over the doors with a wave rising at each end, a split stripe down the
bonnet, bands over the roof.  Its textures draw them panel by panel for a
128-pixel texture, and projected onto a real car's curved panels those
shapes break into blobs.  So the livery keeps the original's composition and
is drawn afresh, as smooth ribbons laid out in the car's own frame (metres:
x forward, y left, z up, the origin at hub height), supersampled and
anti-aliased.

Writes livery_side.png (x across, z up), livery_top.png (x across, y down
the image), livery_front.png and livery_rear.png (y across, z up) into the
pack, over the extents car.cfg names, and adds livery_front to car.cfg.
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw

RED, WHITE, BLUE = (196, 20, 38, 255), (244, 244, 242, 255), (24, 48, 150, 255)
PX = 512                     # pixels per metre, before the 4x supersample is averaged down
SS = 4


def canvas(w_m, h_m):
    img = Image.new('RGBA', (int(w_m * PX * SS), int(h_m * PX * SS)), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def ribbon(d, pts, width, colour, to_px):
    """a band of `width` metres along a polyline of (u, v) metres"""
    P = np.array(pts, float)
    t = np.gradient(P, axis=0); t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-12
    n = np.stack([-t[:, 1], t[:, 0]], 1) * width / 2
    a, b = P + n, P - n
    d.polygon([to_px(*p) for p in a] + [to_px(*p) for p in b[::-1]], fill=colour)


def tricolour(d, centre, width, to_px, order=(RED, WHITE, BLUE), gap=0.012):
    """three ribbons side by side along the centre line, `width` each"""
    C = np.array(centre, float)
    t = np.gradient(C, axis=0); t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-12
    n = np.stack([-t[:, 1], t[:, 0]], 1)
    for k, col in zip((1, 0, -1), order):
        ribbon(d, C + n * k * (width + gap), width, col, to_px)


def curve(f, a, b, n=240):
    s = np.linspace(a, b, n)
    return np.array([f(v) for v in s])


def main():
    pack = sys.argv[1]
    cfg = open(os.path.join(pack, 'car.cfg')).read().split('\n')
    x0, xl, z0, zh = map(float, next(l for l in cfg if l.startswith('livery_side ')).split()[1:])
    y0, yw = map(float, next(l for l in cfg if l.startswith('livery_top ')).split()[1:3])
    save = lambda img, name: img.resize((img.width // SS, img.height // SS), Image.LANCZOS).save(os.path.join(pack, name))

    # the sides: an arch over the doors, a wave rising over each wheel
    img, d = canvas(xl, zh)
    side = lambda x, z: ((x - x0) * PX * SS, (z0 + zh - z) * PX * SS)
    arch = curve(lambda s: (s, -0.02 + 0.62 * (1 - (s / 0.95) ** 2)), 0.95, -0.95)
    tricolour(d, arch, 0.075, side)
    front = curve(lambda s: (s, 0.02 + 0.40 * ((1.95 - s) / 0.75) ** 1.4), 1.95, 1.20)
    tricolour(d, front, 0.06, side)
    rear = curve(lambda s: (s, 0.05 + 0.55 * ((s + 1.60) / 0.45) ** 1.2 if s > -1.60 else 0.05), -1.60, -2.02)
    tricolour(d, rear[::-1], 0.06, side)
    save(img, 'livery_side.png')

    # the top: three stripes down the bonnet, fanning towards the screen; a
    # band across the roof; a pinstripe along the boot's edge
    img, d = canvas(xl, yw)
    top = lambda x, y: ((x - x0) * PX * SS, (y - y0) * PX * SS)
    for k, col in zip((1, 0, -1), (RED, WHITE, BLUE)):
        ribbon(d, curve(lambda s: (s, k * (0.10 + 0.16 * ((2.1 - s) / 1.5) ** 1.5)), 2.10, 0.62), 0.10, col, top)
    for k, col in zip((1, 0, -1), (RED, WHITE, BLUE)):
        xc = -0.25 + k * 0.17
        ribbon(d, curve(lambda s: (xc + 0.06 * np.sin(s * 3.0), s), -0.95, 0.95), 0.15, col, top)
    for k, col in zip((1, 0, -1), (RED, WHITE, BLUE)):
        ribbon(d, curve(lambda s: (-1.62 - k * 0.035, s), -0.80, 0.80), 0.028, col, top)
    save(img, 'livery_top.png')

    # front and back: a tricolour pinstripe at the height of the lights
    for name, zc in (('front', 0.30), ('rear', 0.44)):
        img, d = canvas(yw, zh)
        fb = lambda y, z: ((y - y0) * PX * SS, (z0 + zh - z) * PX * SS)
        for k, col in zip((1, 0, -1), (RED, WHITE, BLUE)):
            ribbon(d, curve(lambda s: (s, zc + k * 0.035), y0 + 0.08, y0 + yw - 0.08), 0.028, col, fb)
        save(img, f'livery_{name}.png')

    cfg = [l for l in cfg if not l.startswith('livery_front ')]
    while cfg and cfg[-1] == '':
        cfg.pop()
    cfg.append(f'livery_front {y0:.4f} {yw:.4f}')
    open(os.path.join(pack, 'car.cfg'), 'w').write('\n'.join(cfg) + '\n')
    print('livery drawn')


if __name__ == '__main__':
    main()
