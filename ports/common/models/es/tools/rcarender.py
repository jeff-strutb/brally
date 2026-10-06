#!/usr/bin/env python3
"""Offline renderer for a Boss Rally .rca car: walks the F3DEX display lists,
decodes the textures, and rasterises reference views (numpy z-buffer)."""
import struct, sys, math, json
import numpy as np
from PIL import Image

B = 0x803C8000
d = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
a2o = lambda a: 0x8000 + (a - B)
be = lambda o: struct.unpack('>I', d[o:o + 4])[0]
inimg = lambda a: B <= a < B + len(d) - 0x8000

def rgba16(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31, 255 if v & 1 else 0

def decode(addr, fmt, siz, w, h, pal):
    o = a2o(addr); px = np.zeros((h, w, 4), np.uint8)
    rb = w * [4, 8, 16, 32][siz] // 8
    raw = bytearray(d[o:o + rb * h + 8])
    if SWAP:
        for y in range(1, h, 2):
            for k in range(y * rb, (y + 1) * rb - 7, 8):
                raw[k:k + 8] = raw[k + 4:k + 8] + raw[k:k + 4]
    dd = bytes(raw); o = 0
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if siz == 2:  # 16b
                v = struct.unpack('>H', dd[o + 2 * i:o + 2 * i + 2])[0]; c = rgba16(v) if fmt == 0 else (v >> 8,) * 3 + (v & 255,)
            elif siz == 1:  # 8b
                v = dd[o + i]; c = pal[v] if fmt == 2 else ((v >> 4) * 17,) * 3 + ((v & 15) * 17,) if fmt == 3 else (v,) * 4
            elif siz == 0:  # 4b
                v = dd[o + i // 2]; v = v >> 4 if i % 2 == 0 else v & 15
                c = pal[v] if fmt == 2 else ((v >> 1) * 36,) * 3 + ((v & 1) * 255,) if fmt == 3 else (v * 17,) * 4
            else:  # 32b
                c = tuple(dd[o + 4 * i:o + 4 * i + 4])
            if c[0] < 90 and c[1] > 150 and c[2] > 150 and abs(c[1] - c[2]) < 60: c = (12, 125, 45, c[3])
            px[y, x] = c
    return px

import os
SWAP = os.environ.get('SWAP','1')=='1'
texcache = {}
tris = []  # (verts[3] xyz, uv[3], normals[3], tex key, texgen)

def run(addr, M, st):
    o = a2o(addr)
    while True:
        w0, w1 = be(o), be(o + 4); op = w0 >> 24; o += 8
        if op == 0x06:
            if inimg(w1): run(w1, M, st)
            if (w0 >> 16) & 0xFF == 1: return
        elif op == 0xB8: return
        elif op == 0x04:
            n = (w0 >> 10) & 0x3F; v0 = ((w0 >> 16) & 0xFF) // 2; vo = a2o(w1)
            for i in range(n):
                x, y, z, f, s, t = struct.unpack('>hhhHhh', d[vo + 16 * i:vo + 16 * i + 12])
                nx, ny, nz = struct.unpack('bbb', d[vo + 16 * i + 12:vo + 16 * i + 15])
                p = M @ np.array([x, y, z, 1.0])
                st['v'][v0 + i] = (p[:3], (s / 32.0, t / 32.0), (M[:3, :3] @ np.array([nx, ny, nz], float)))
        elif op in (0xBF, 0xB1):
            idx = [((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2] if op == 0xBF else \
                  [((w0 >> 16) & 0xFF) // 2, ((w0 >> 8) & 0xFF) // 2, (w0 & 0xFF) // 2]
            sets = [idx] if op == 0xBF else [idx, [((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2]]
            for s3 in sets:
                vs = [st['v'][k] for k in s3]
                if None in vs: continue
                tris.append((vs, st['tex'], st['texgen']))
        elif op == 0xFD: st['timg'] = (w1, (w0 >> 21) & 7, (w0 >> 19) & 3)
        elif op == 0xF3: st['loaded'] = st['timg']
        elif op == 0xF0:
            po = a2o(st['timg'][0]); st['pal'] = [rgba16(struct.unpack('>H', d[po + 2 * i:po + 2 * i + 2])[0]) for i in range(256) if po + 2 * i + 2 <= len(d)]
        elif op == 0xF5:
            tile = (w1 >> 24) & 7
            st['tiles'][tile] = dict(fmt=(w0 >> 21) & 7, siz=(w0 >> 19) & 3, line=(w0 >> 9) & 0x1FF, tmem=w0 & 0x1FF,
                                    pal=(w1 >> 20) & 15, cmt=(w1 >> 18) & 3, cms=(w1 >> 8) & 3)
        elif op == 0xF2:
            tile = (w1 >> 24) & 7
            if tile == 0:
                uls, ult, lrs, lrt = (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF
                t0 = st['tiles'].get(0)
                if t0 and st.get('loaded'):
                    w = ((lrs - uls) >> 2) + 1; h = ((lrt - ult) >> 2) + 1
                    bpp = [4, 8, 16, 32][t0['siz']]; lw = t0['line'] * 64 // bpp
                    if lw: w = max(w, lw) if lw >= w else w
                    key = (st['loaded'][0], t0['fmt'], t0['siz'], lw or w, h, t0['pal'], t0['cms'], t0['cmt'])
                    if key not in texcache:
                        pal = st.get('pal', [(255, 0, 255, 255)] * 256)
                        texcache[key] = decode(st['loaded'][0], t0['fmt'], t0['siz'], lw or w, h, pal)
                    st['tex'] = (key, uls / 4.0, ult / 4.0)
        elif op == 0xB7:
            if w1 & 0x40000: st['texgen'] = True
        elif op == 0xB6:
            if w1 & 0x40000: st['texgen'] = False

def newst(): return dict(v=[None] * 64, tiles={}, tex=None, texgen=False)

I = np.eye(4) / 255.0; I[3, 3] = 1
parts = {}
for name, off in [('body', 0x8038), ('cab', 0x8028), ('detail', 0x8024), ('glass', 0x8030)]:
    a = be(off)
    if a:
        n0 = len(tris); run(a, I, newst()); parts[name] = (n0, len(tris))
# wheels: bound of the wheel DL, placed at the arches found from the body
n0 = len(tris); run(be(0x80BC), np.eye(4) / 255.0 + np.diag([0, 0, 0, 1 - 1 / 255.0]), newst()); wheel = tris[n0:]; del tris[n0:]
wp = np.array([p for t in wheel for p, _, _ in t[0]])
print('wheel bounds', wp.min(0), wp.max(0))
bp = np.array([p for t in tris for p, _, _ in t[0]])
print('car bounds', bp.min(0), bp.max(0), 'tris', len(tris), 'textures', len(texcache))
WX = float(sys.argv[3]) if len(sys.argv) > 3 else 1.28
WY = float(sys.argv[4]) if len(sys.argv) > 4 else 0.72
wc = (wp.min(0) + wp.max(0)) / 2
for sx in (WX, -WX):
    for sy in (WY, -WY):
        for vs, tex, tg in wheel:
            nv = []
            for p, uv, nrm in vs:
                q = p - wc
                if sy < 0: q = q * np.array([1, -1, 1]); nrm = nrm * np.array([1, -1, 1])
                nv.append((q + np.array([sx, sy, wc[2] - wp.min(0)[2]]), uv, nrm))
            tris.append((nv, tex, tg))

def sample(key, uo, vo, uv):
    tex = texcache[key]; h, w = tex.shape[:2]
    u = (uv[..., 0] - uo); v = (uv[..., 1] - vo)
    cms, cmt = key[6], key[7]
    def wrap(c, n, m):
        if m & 2: c = np.clip(c, 0, n - 1)
        elif m & 1:
            c = np.mod(c, 2 * n); c = np.where(c >= n, 2 * n - 1 - c, c)
        else: c = np.mod(c, n)
        return c.astype(int)
    return tex[wrap(np.floor(v), h, cmt), wrap(np.floor(u), w, cms)].astype(float) / 255.0

def render(eye, target, fov, W, H, path):
    SS = 2; W2, H2 = W * SS, H * SS
    f = np.array(target) - np.array(eye); f /= np.linalg.norm(f)
    r = np.cross(f, [0, 0, 1]); r /= np.linalg.norm(r); u = np.cross(r, f)
    col = np.ones((H2, W2, 3)); zb = np.full((H2, W2), np.inf)
    fl = 0.5 * H2 / math.tan(math.radians(fov) / 2)
    L = np.array([0.4, -0.5, 0.75]); L /= np.linalg.norm(L)
    for vs, tex, tg in tris:
        P = np.array([p for p, _, _ in vs]); UV = np.array([uv for _, uv, _ in vs]); N = np.array([n for _, _, n in vs])
        c = P - eye; zc = c @ f
        if (zc <= 0.05).any(): continue
        sx = W2 / 2 + fl * (c @ r) / zc; sy = H2 / 2 - fl * (c @ u) / zc
        x0, x1 = int(max(0, math.floor(sx.min()))), int(min(W2 - 1, math.ceil(sx.max())))
        y0, y1 = int(max(0, math.floor(sy.min()))), int(min(H2 - 1, math.ceil(sy.max())))
        if x0 > x1 or y0 > y1: continue
        den = (sy[1] - sy[2]) * (sx[0] - sx[2]) + (sx[2] - sx[1]) * (sy[0] - sy[2])
        if abs(den) < 1e-9: continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((sy[1] - sy[2]) * (X - sx[2]) + (sx[2] - sx[1]) * (Y - sy[2])) / den
        l1 = ((sy[2] - sy[0]) * (X - sx[2]) + (sx[0] - sx[2]) * (Y - sy[2])) / den
        l2 = 1 - l0 - l1
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not m.any(): continue
        iz = l0 / zc[0] + l1 / zc[1] + l2 / zc[2]; z = 1 / iz
        sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= z < sub
        if not m.any(): continue
        w = np.stack([l0 / zc[0], l1 / zc[1], l2 / zc[2]], -1) * z[..., None]
        fn = np.cross(P[1] - P[0], P[2] - P[0]); fn /= (np.linalg.norm(fn) + 1e-12)
        if fn @ (eye - P[0]) < 0: fn = -fn
        nn = N / (np.linalg.norm(N, axis=1, keepdims=True) + 1e-9)
        n = w @ nn; n /= (np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9)
        n = np.where((n @ fn)[..., None] < 0, -n, n)
        shade = 0.45 + 0.55 * np.clip(n @ L, 0, 1)
        if tex and not tg:
            t = sample(tex[0], tex[1], tex[2], w @ UV)
            m &= t[..., 3] > 0.3
            rgb = t[..., :3]
        elif tex and tg:
            t = texcache[tex[0]]; rgb = np.broadcast_to(t[..., :3].reshape(-1, 3).mean(0) / 255.0, w.shape)
        else:
            rgb = np.full(w.shape, 0.6)
        rgb = rgb * shade[..., None]
        sub[m] = z[m]; col[y0:y1 + 1, x0:x1 + 1][m] = rgb[m]
    img = Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).resize((W, H), Image.LANCZOS)
    img.save(path)

C = np.array([0, 0, 0.6])
views = {
    'front_34': (C + [6.4, -4.6, 2.1]), 'side': (C + [0, -9.2, 0.4]), 'rear_34': (C + [-6.4, -4.6, 2.3]),
    'front': (C + [8.5, 0, 1.0]), 'rear': (C + [-8.5, 0, 1.1]), 'top_34': (C + [4.3, 5.3, 5.8]),
}
sel = sys.argv[5].split(',') if len(sys.argv) > 5 else list(views)
for k in sel:
    render(C + (views[k] - C) * 1.3, C, 26, 1024, 1024, f'{out}/{k}.png'); print('wrote', k)
