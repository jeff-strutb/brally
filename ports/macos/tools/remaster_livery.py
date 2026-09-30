#!/usr/bin/env python3
"""remaster_livery.py -- the Remastered car's livery, recreated from the
original car's own textures.

    remaster_livery.py <car.rca> <pack dir>

The original's livery is the red, white and blue ribbons painted over the
paint colour (the cyan key in its textures).  This draws the original model
(the .rca from the game's disc) orthographically from the side, top, front
and back, in the same car frame the Remastered model uses, and turns each
view into a clean decal image at high resolution:

  1. every pixel becomes red, white, blue or nothing: the paint key, the
     script, the numbers, badges and plates are nothing;
  2. badges go (lettering leaves holes in a blue shape; a ribbon has none,
     so each cluster of letter holes and the oval around it is painted over
     from the ribbon around it), number panels and plates go (solid
     rectangles once their digits are filled in), small shapes go, and the
     holes other lettering leaves inside a ribbon take the ribbon's colour;
  3. the 1999 texels become smooth curves: each colour's coverage is
     blurred and the strongest wins, then the edges are anti-aliased.

Writes livery_side.png, livery_top.png, livery_front.png and livery_rear.png
into the pack, over the extents car.cfg names (livery_side, livery_top), and
adds livery_front to car.cfg.
"""
import math, os, struct, sys
import numpy as np
from PIL import Image
from scipy import ndimage

B = 0x803C8000
RED, WHITE, BLUE = (200, 18, 36), (242, 242, 240), (22, 44, 150)


def load_rca(path):
    d = open(path, 'rb').read()
    a2o = lambda a: 0x8000 + (a - B)
    be = lambda o: struct.unpack('>I', d[o:o + 4])[0]
    inimg = lambda a: B <= a < B + len(d) - 0x8000

    def rgba16(v):
        return ((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31, 255 if v & 1 else 0

    def decode(addr, fmt, siz, w, h, pal):
        o = a2o(addr)
        rb = w * [4, 8, 16, 32][siz] // 8
        raw = bytearray(d[o:o + rb * h + 8])
        for y in range(1, h, 2):                     # odd rows are stored word-swapped (TMEM order)
            for k in range(y * rb, (y + 1) * rb - 7, 8):
                raw[k:k + 8] = raw[k + 4:k + 8] + raw[k:k + 4]
        px = np.zeros((h, w, 4), np.uint8)
        for y in range(h):
            for x in range(w):
                i = y * w + x
                if siz == 2:
                    v = struct.unpack('>H', raw[2 * i:2 * i + 2])[0]; c = rgba16(v)
                elif siz == 1:
                    c = pal[raw[i]]
                elif siz == 0:
                    v = raw[i // 2]; c = pal[v >> 4 if i % 2 == 0 else v & 15]
                else:
                    c = tuple(raw[4 * i:4 * i + 4])
                px[y, x] = c
        return px

    tex, tris = {}, []

    def run(addr, st):
        o = a2o(addr)
        while True:
            w0, w1 = be(o), be(o + 4); op = w0 >> 24; o += 8
            if op == 0x06:
                if inimg(w1): run(w1, st)
                if (w0 >> 16) & 0xFF == 1: return
            elif op == 0xB8: return
            elif op == 0x04:
                n = (w0 >> 10) & 0x3F; v0 = ((w0 >> 16) & 0xFF) // 2; vo = a2o(w1)
                for i in range(n):
                    x, y, z, f, s, t = struct.unpack('>hhhHhh', d[vo + 16 * i:vo + 16 * i + 12])
                    st['v'][v0 + i] = (np.array([x, y, z]) / 255.0, (s / 32.0, t / 32.0))
            elif op in (0xBF, 0xB1):
                ids = [[((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2]] if op == 0xBF else \
                      [[((w0 >> 16) & 0xFF) // 2, ((w0 >> 8) & 0xFF) // 2, (w0 & 0xFF) // 2],
                       [((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2]]
                for s3 in ids:
                    vs = [st['v'][k] for k in s3]
                    if None not in vs and st['tex'] and not st['texgen']:
                        tris.append((vs, st['tex']))
            elif op == 0xFD: st['timg'] = w1
            elif op == 0xF3: st['loaded'] = st['timg']
            elif op == 0xF0:
                po = a2o(st['timg'])
                st['pal'] = [rgba16(struct.unpack('>H', d[po + 2 * i:po + 2 * i + 2])[0]) for i in range(256) if po + 2 * i + 2 <= len(d)]
            elif op == 0xF5:
                if (w1 >> 24) & 7 == 0:
                    st['t0'] = dict(fmt=(w0 >> 21) & 7, siz=(w0 >> 19) & 3, line=(w0 >> 9) & 0x1FF, cms=(w1 >> 8) & 3, cmt=(w1 >> 18) & 3)
            elif op == 0xF2:
                if (w1 >> 24) & 7 == 0 and st.get('t0') and st.get('loaded'):
                    t0 = st['t0']
                    uls, ult, lrs, lrt = (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF
                    w = t0['line'] * 64 // [4, 8, 16, 32][t0['siz']] or ((lrs - uls) >> 2) + 1
                    h = ((lrt - ult) >> 2) + 1
                    key = (st['loaded'], t0['fmt'], t0['siz'], w, h, t0['cms'], t0['cmt'], id(st.get('pal')))
                    if key not in tex:
                        tex[key] = decode(st['loaded'], t0['fmt'], t0['siz'], w, h, st.get('pal', [(255, 0, 255, 255)] * 256))
                    st['tex'] = (key, uls / 4.0, ult / 4.0)
            elif op == 0xB7 and w1 & 0x40000: st['texgen'] = True
            elif op == 0xB6 and w1 & 0x40000: st['texgen'] = False

    for off in (0x8038, 0x8028, 0x8024):          # body, cabin, detail (no glass, no wheels)
        a = be(off)
        if a:
            run(a, dict(v=[None] * 64, tex=None, texgen=False))
    return tris, tex


def sample(tex, key, uo, vo, uv):
    t = tex[key]; h, w = t.shape[:2]; cms, cmt = key[5], key[6]
    def wrap(c, n, m):
        if m & 2: c = np.clip(c, 0, n - 1)
        elif m & 1:
            c = np.mod(c, 2 * n); c = np.where(c >= n, 2 * n - 1 - c, c)
        else: c = np.mod(c, n)
        return c.astype(int)
    return t[wrap(np.floor(uv[..., 1] - vo), h, cmt), wrap(np.floor(uv[..., 0] - uo), w, cms)]


def ortho(tris, tex, axis_u, axis_v, view, lo_u, span_u, lo_v, span_v, W, H):
    """The model seen along -view onto the (u, v) plane; v runs down the image."""
    col = np.zeros((H, W, 4), np.uint8); zb = np.full((H, W), -np.inf)
    for vs, (key, uo, vo) in tris:
        P = np.array([p for p, _ in vs]); UV = np.array([t for _, t in vs])
        n = np.cross(P[1] - P[0], P[2] - P[0])
        if n @ view <= 1e-9 and -(n @ view) <= 1e-9: continue
        X = (P @ axis_u - lo_u) / span_u * W; Y = (1 - (P @ axis_v - lo_v) / span_v) * H; Z = P @ view
        x0, x1 = max(0, int(X.min())), min(W - 1, int(X.max()) + 1); y0, y1 = max(0, int(Y.min())), min(H - 1, int(Y.max()) + 1)
        if x0 > x1 or y0 > y1: continue
        den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2])
        if abs(den) < 1e-9: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        l0 = ((Y[1] - Y[2]) * (gx - X[2]) + (X[2] - X[1]) * (gy - Y[2])) / den
        l1 = ((Y[2] - Y[0]) * (gx - X[2]) + (X[0] - X[2]) * (gy - Y[2])) / den
        l2 = 1 - l0 - l1; m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        z = l0 * Z[0] + l1 * Z[1] + l2 * Z[2]
        sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= z > sub
        if not m.any(): continue
        uv = l0[..., None] * UV[0] + l1[..., None] * UV[1] + l2[..., None] * UV[2]
        c = sample(tex, key, uo, vo, uv)
        sub[m] = z[m]; col[y0:y1 + 1, x0:x1 + 1][m] = c[m]
    return col


def classes(img):
    c = img[..., :3].astype(float) / 255
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    key = (r < 0.35) & (g > 0.6) & (b > 0.6)
    red = (r > 0.45) & (g < 0.3) & (b < 0.35)
    white = (r > 0.78) & (g > 0.78) & (b > 0.78)
    blue = (b > 0.3) & (r < 0.3) & (g < 0.3) & (b > r + 0.1)
    covered = img[..., 3] > 0
    k = np.zeros(c.shape[:2], np.int8)              # 0 nothing, 1 red, 2 white, 3 blue
    k[red & covered] = 1; k[white & covered & ~key] = 2; k[blue & covered] = 3
    return k


def inpaint(k, region):
    """fill `region` with the nearest colour outside it; with the ribbons'
    colours only when ribbon is most of what surrounds it"""
    rim = ndimage.binary_dilation(region, iterations=3) & ~region
    src = region
    if rim.any() and (k[rim] > 0).mean() > 0.6:
        src = region | (k == 0)
    _, (iy, ix) = ndimage.distance_transform_edt(src, return_indices=True)
    return np.where(region, k[iy, ix], k)


def clean(k, texel):
    # badges: lettering is the one thing that leaves holes inside a blue
    # shape (a ribbon has none); each cluster of letter holes, its outline
    # and a margin for the badge's rim are painted over from around them
    blue = k == 3
    letters = ndimage.binary_fill_holes(blue) & ~blue
    if letters.any():
        groups, n = ndimage.label(ndimage.binary_dilation(letters, iterations=int(texel * 1.5)))
        for i in range(1, n + 1):
            g = (groups == i) & letters
            if g.sum() < texel * texel * 0.5:
                continue
            ys, xs = np.nonzero(g)
            cy, cx = ys.mean(), xs.mean()
            ry, rx = (ys.max() - ys.min()) / 2 + texel * 2.5, (xs.max() - xs.min()) / 2 + texel * 2.5
            yy, xx = np.ogrid[:k.shape[0], :k.shape[1]]
            oval = ((yy - cy) / ry) ** 2 + ((xx - cx) / rx) ** 2 <= 1
            k = inpaint(k, oval)
    # number panels and plates: solid rectangles on their own; small shapes
    rib = k > 0
    lab, n = ndimage.label(rib)
    if n:
        area = ndimage.sum(rib, lab, range(1, n + 1))
        keep = np.zeros(n + 1, bool)
        for i, sl in enumerate(ndimage.find_objects(lab), 1):
            box = (sl[0].stop - sl[0].start) * (sl[1].stop - sl[1].start)
            solid = ndimage.binary_fill_holes(lab[sl] == i).sum()     # a panel's digits are holes
            keep[i] = area[i - 1] > 0.15 * area.max() and solid / box < 0.8
        k = np.where(keep[lab], k, 0)
    # holes lettering left inside a ribbon: take the nearest ribbon colour
    rib = k > 0
    holes = ndimage.binary_fill_holes(rib) & ~rib
    if holes.any():
        _, (iy, ix) = ndimage.distance_transform_edt(~rib, return_indices=True)
        k = np.where(holes, k[iy, ix], k)
    return k


def smooth(k, sigma, out_w, out_h):
    """class map -> an anti-aliased RGBA decal, the texel steps rounded off"""
    probs = []
    for c in (1, 2, 3):
        p = ndimage.gaussian_filter((k == c).astype(np.float32), sigma)
        probs.append(np.asarray(Image.fromarray(p).resize((out_w, out_h), Image.BICUBIC)))
    none = 1 - np.clip(sum(probs), 0, 1)
    P = np.stack([none] + probs, -1)
    win = P.argmax(-1)
    cov = np.clip((1 - none - 0.5) * 6 + 0.5, 0, 1)                  # the ribbons' edge, anti-aliased
    rgb = np.array([(0, 0, 0), RED, WHITE, BLUE], np.float32)[win]
    # colour boundaries inside the ribbons, softened over a pixel
    rgb = ndimage.uniform_filter(rgb, size=(3, 3, 1))
    return Image.fromarray(np.dstack([rgb, cov * 255]).astype(np.uint8), 'RGBA')


def main():
    rca, pack = sys.argv[1], sys.argv[2]
    cfg = open(os.path.join(pack, 'car.cfg')).read().split('\n')
    side = next(l for l in cfg if l.startswith('livery_side ')).split()[1:]
    top = next(l for l in cfg if l.startswith('livery_top ')).split()[1:]
    x0, xl, z0, zh = map(float, side); y0, yw = map(float, top[:2])
    tris, tex = load_rca(rca)
    print(f'{len(tris)} textured triangles, {len(tex)} textures')
    X, Y, Z = np.eye(3)
    S = 256                                                            # raster pixels per metre
    views = {
        'side': (X, Z, Y, x0, xl, z0, zh),                             # from the left; both sides carry the same design
        'top': (X, -Y, Z, x0, xl, -(y0 + yw), yw),
        'front': (-Y, Z, X, -(y0 + yw), yw, z0, zh),
        'rear': (Y, Z, -X, y0, yw, z0, zh),
    }
    for name, (au, av, vw, lu, su, lv, sv) in views.items():
        W, H = int(su * S), int(sv * S)
        img = ortho(tris, tex, au, av, vw, lu, su, lv, sv, W, H)
        texel = S * 0.025                                              # the original's texels, in raster pixels
        k = clean(classes(img), texel)
        dec = smooth(k, sigma=texel * 0.8, out_w=W * 4, out_h=H * 4)
        dec.save(os.path.join(pack, f'livery_{name}.png'))
        print(f'livery_{name}: {W * 4}x{H * 4}, ribbons over {100 * (k > 0).mean():.0f}% of the view')
    cfg = [l for l in cfg if not l.startswith('livery_front ')]
    cfg.insert(len(cfg) - 1 if cfg and cfg[-1] == '' else len(cfg), f'livery_front {y0:.4f} {yw:.4f}')
    open(os.path.join(pack, 'car.cfg'), 'w').write('\n'.join(cfg) + ('' if cfg[-1] == '' else '\n'))


if __name__ == '__main__':
    main()
