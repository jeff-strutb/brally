"""handicons.py: the Remaster Options' monitor and console icons, built by hand: each a few
hundred triangles of simple shapes in flat colours, as the game's own icons are (100 to 800
triangles), measured off orthographic renders and probes of the source models in
ports/common/assets (crt.blend; n64.blend, the console's proportions and colours, after "Nintendo
64" by Oliver_Budd_ID, thingiverse.com/thing:3534035, CC BY).  Units are the sources' own: z up,
the front toward -y.

    python3 ports/remastered/tools/handicons.py [OUT.c]

writes ports/remastered/tgrally/platform/os/rm_icons.c (or OUT.c): each icon in the game's icon
model format (tgr/model.h), big-endian, every address an offset from its start (rmoptions.c
places and rebases them).  Each colour is a part: a 4-texel RGBA16 texture of its colour, loaded
as the game's one-colour parts load theirs, then its vertices (normals in the colour bytes, for
the lighting) in loads of up to 32 with F3DEX 1 TRI2 commands; normals smoothed across edges under
45 degrees.  Last, the shadow the game's icons cast on the ring: the outline from above at z 0,
floating 250 units over it, blended in the second pass with the game's own commands. """
import math
import struct
import sys

import numpy as np

HOVER = 250.0


class Mesh:
    def __init__(self):
        self.v = []          # positions
        self.t = []          # (a, b, c, part, lit_up): counter-clockwise seen from outside

    def vert(self, p):
        self.v.append(np.array(p, float))
        return len(self.v) - 1

    def tri(self, a, b, c, part, lit=False):
        self.t.append((a, b, c, part, lit))

    def quad(self, a, b, c, d, part, lit=False):
        self.tri(a, b, c, part, lit)
        self.tri(a, c, d, part, lit)

    def poly(self, ids, part, lit=False):
        """a convex polygon, counter-clockwise from outside, as a fan"""
        for k in range(1, len(ids) - 1):
            self.tri(ids[0], ids[k], ids[k + 1], part, lit)

    def loft(self, rings, part, cap0=False, cap1=False):
        """rings of equal counts, each counter-clockwise seen from the next ring's side"""
        ids = [[self.vert(p) for p in r] for r in rings]
        n = len(rings[0])
        for a, b in zip(ids, ids[1:]):
            for k in range(n):
                self.quad(a[k], a[(k + 1) % n], b[(k + 1) % n], b[k], part)
        if cap0:
            self.poly(ids[0][::-1], part)
        if cap1:
            self.poly(ids[-1], part)
        return ids


def box(m, x0, x1, y0, y1, z0, z1, part):
    r0 = [(x0, y0, z0), (x0, y0, z1), (x1, y0, z1), (x1, y0, z0)]       # counter-clockwise from +y
    r1 = [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]
    m.loft([r0, r1], part, cap0=True, cap1=True)


def rrect(cx, cz, hw, z0, z1, rt, rb, y, seg=3):
    """a rounded rectangle across x and z at y, counter-clockwise seen from +y"""
    pts = []
    corners = [(cx + hw - rb, z0 + rb, rb, -90), (cx + hw - rt, z1 - rt, rt, 0),
               (cx - hw + rt, z1 - rt, rt, 90), (cx - hw + rb, z0 + rb, rb, 180)]
    for (ox, oz, r, a0) in corners:
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg)
            pts.append((ox + r * math.cos(a), y, oz + r * math.sin(a)))
    return pts[::-1]          # counter-clockwise seen from +y: x then z reversed


# ---- the monitor --------------------------------------------------------------------------------
BEIGE, SCREEN, PANEL = 0, 1, 2
CRT_COLOURS = {BEIGE: (0.894, 0.894, 0.800), SCREEN: (0.122, 0.122, 0.122), PANEL: (0.475, 0.478, 0.463)}


def crt():
    m = Mesh()
    # the bezel: x +-0.858, y -0.95 (its face) to -0.60, z -0.674 to 0.921, its front top edge
    # bevelled (the face's top at 0.774, the top's front at -0.87)
    BX, Y0, Y1, Z0, Z1, ZF, YT = 0.858, -0.95, -0.60, -0.674, 0.921, 0.774, -0.87
    prof = [(Y0, Z0), (Y1, Z0), (Y1, Z1), (YT, Z1), (Y0, ZF)]          # counter-clockwise from +x
    R = [m.vert((BX, y, z)) for y, z in prof]
    L = [m.vert((-BX, y, z)) for y, z in prof]
    m.poly(R, BEIGE)
    m.poly(L[::-1], BEIGE)
    for a in range(4):                                                 # bottom, back, top, bevel
        m.quad(R[a], L[a], L[a + 1], R[a + 1], BEIGE)
    # the face, round the screen's opening (x +-0.67, z -0.455 to 0.735), set back 0.04
    SX, SZ0, SZ1, DEP = 0.67, -0.455, 0.735, 0.04
    o = [m.vert(p) for p in ((-BX, Y0, Z0), (BX, Y0, Z0), (BX, Y0, ZF), (-BX, Y0, ZF))]
    i = [m.vert(p) for p in ((-SX, Y0, SZ0), (SX, Y0, SZ0), (SX, Y0, SZ1), (-SX, Y0, SZ1))]
    for k in range(4):                                                 # the frame, seen from -y
        m.quad(o[k], o[(k + 1) % 4], i[(k + 1) % 4], i[k], BEIGE)
    j = [m.vert(p + np.array((0, DEP, 0))) for p in (m.v[q] for q in i)]
    for k in range(4):                                                 # the opening's sides
        m.quad(i[k], i[(k + 1) % 4], j[(k + 1) % 4], j[k], BEIGE)
    # the screen: bulging 0.03 at its middle
    nx, nz = 6, 5
    zc, hz = (SZ0 + SZ1) / 2, (SZ1 - SZ0) / 2
    g = [[m.vert((x, Y0 + DEP - 0.03 * (1 - (x / SX) ** 2) * (1 - ((z - zc) / hz) ** 2), z))
          for x in np.linspace(-SX, SX, nx + 1)] for z in np.linspace(SZ0, SZ1, nz + 1)]
    for r in range(nz):
        for c in range(nx):
            m.quad(g[r][c], g[r][c + 1], g[r + 1][c + 1], g[r + 1][c], SCREEN)
    # the housing behind it, tapering to the back: half width 0.763 to 0.575, top 0.87 to 0.44,
    # bottom -0.68 to -0.31 (measured side and top silhouettes), corners rounded
    rings = []
    for y in (-0.60, -0.30, 0.0, 0.30, 0.60, 0.88):
        hw = 0.744 - 0.1325 * (y + 0.45)
        zt = 0.69 - 0.285 * y + (0.02 if y < -0.5 else 0.0)
        zb = -0.53 + 0.25 * y
        rings.append(rrect(0, 0, hw, zb, zt, 0.09, 0.05, y))
    ids = m.loft(rings, BEIGE, cap1=True)
    # the back's small box and its grey panel: x +-0.48, z -0.33 to 0.30, to y 0.95
    box(m, -0.48, 0.48, 0.88, 0.95, -0.33, 0.30, BEIGE)
    p = [m.vert(q) for q in ((0.25, 0.952, -0.17), (-0.25, 0.952, -0.17), (-0.25, 0.952, 0.25), (0.25, 0.952, 0.25))]
    m.quad(p[0], p[1], p[2], p[3], PANEL)
    # under the housing: the lower block (to z -0.76) and the stand (base x +-0.47, y -0.80 to
    # 0.18, z -0.921 to -0.83; its neck x +-0.20)
    lo = [[(-0.74, -0.62, -0.76), (-0.74, -0.62, -0.62), (0.74, -0.62, -0.62), (0.74, -0.62, -0.76)],
          [(-0.60, 0.58, -0.76), (-0.60, 0.58, -0.36), (0.60, 0.58, -0.36), (0.60, 0.58, -0.76)]]
    m.loft(lo, BEIGE, cap0=True, cap1=True)
    box(m, -0.20, 0.20, -0.55, 0.05, -0.84, -0.75, BEIGE)
    base = []
    for z in (-0.921, -0.84):
        ring = []
        for (cx, cy, a0) in ((0.37, -0.70, -90), (0.37, 0.08, 0), (-0.37, 0.08, 90), (-0.37, -0.70, 180)):
            for k in range(3):
                a = math.radians(a0 + 45 * k)
                ring.append((cx + 0.1 * math.cos(a), cy + 0.1 * math.sin(a), z))
        base.append(ring)
    m.loft(base, BEIGE, cap0=True, cap1=True)
    return m, CRT_COLOURS


# ---- the console --------------------------------------------------------------------------------
CHARCOAL, GREY = 0, 1
N64_COLOURS = {CHARCOAL: (0.137, 0.137, 0.137), GREY: (0.85, 0.85, 0.85)}

# its top's height, probed straight down on the source (the median within 0.035 of each point):
# rows from the front (y -0.653) to the back (0.688), columns x -0.84 to 0.84
N64_X = np.linspace(-0.84, 0.84, 13)
N64_Y = np.array([-0.653, -0.55, -0.45, -0.35, -0.25, -0.15, -0.05, 0.05, 0.13, 0.21, 0.33, 0.45, 0.57, 0.688])
N64_TOP_TABLE = [            # (the edge columns 0.03 under their neighbours: the probe slides off the
    # rounded edge there; the grille's slots filled from their neighbours; made symmetric)
    [0.088, 0.118, 0.118, 0.142, 0.195, 0.226, 0.236, 0.226, 0.195, 0.142, 0.118, 0.118, 0.088],
    [0.082, 0.112, 0.127, 0.157, 0.200, 0.233, 0.244, 0.233, 0.200, 0.157, 0.127, 0.112, 0.082],
    [0.089, 0.119, 0.135, 0.163, 0.203, 0.239, 0.251, 0.239, 0.203, 0.163, 0.135, 0.119, 0.089],
    [0.097, 0.127, 0.141, 0.153, 0.207, 0.244, 0.256, 0.244, 0.207, 0.153, 0.141, 0.127, 0.097],
    [0.103, 0.133, 0.148, 0.171, 0.210, 0.243, 0.255, 0.243, 0.210, 0.171, 0.148, 0.133, 0.103],
    [0.107, 0.138, 0.154, 0.178, 0.215, 0.239, 0.245, 0.239, 0.215, 0.178, 0.154, 0.138, 0.107],
    [0.115, 0.145, 0.161, 0.183, 0.194, 0.186, 0.183, 0.186, 0.194, 0.183, 0.161, 0.145, 0.115],
    [0.120, 0.150, 0.166, 0.187, 0.176, 0.171, 0.169, 0.171, 0.176, 0.187, 0.166, 0.150, 0.120],
    [0.122, 0.152, 0.169, 0.183, 0.173, 0.168, 0.166, 0.168, 0.173, 0.183, 0.169, 0.152, 0.122],
    [0.127, 0.157, 0.171, 0.176, 0.152, 0.150, 0.149, 0.150, 0.152, 0.176, 0.171, 0.157, 0.127],
    [0.131, 0.161, 0.176, 0.161, 0.140, 0.140, 0.140, 0.140, 0.140, 0.161, 0.176, 0.161, 0.131],
    [0.134, 0.164, 0.180, 0.175, 0.162, 0.154, 0.152, 0.154, 0.162, 0.175, 0.180, 0.164, 0.134],
    [0.138, 0.168, 0.184, 0.172, 0.157, 0.150, 0.146, 0.150, 0.157, 0.172, 0.184, 0.168, 0.138],
    [0.135, 0.165, 0.180, 0.167, 0.153, 0.145, 0.143, 0.145, 0.153, 0.167, 0.180, 0.165, 0.135],
]
N64_SLOT = (-0.42, 0.42, 0.21, 0.33, 0.14)            # the cartridge slot: x, y, its floor
N64_SOCKETS = [(-0.462, -0.045), (-0.262, -0.045), (0.262, -0.045), (0.462, -0.045)]
N64_SOCKET_R, N64_FRONT = 0.075, -0.655
N64_BASE_Z, N64_DECK_Z = -0.226, 0.016               # the lower body's bottom and top


def n64():
    m = Mesh()
    top = np.array(N64_TOP_TABLE)
    s0, s1, t0, t1, sz = N64_SLOT
    # the upper body: its top a grid of the measured heights, walls down to the lower body's top
    g = [[m.vert((x, y, top[r][c])) for c, x in enumerate(N64_X)] for r, y in enumerate(N64_Y)]
    for r in range(len(N64_Y) - 1):
        for c in range(len(N64_X) - 1):
            cx, cy = (N64_X[c] + N64_X[c + 1]) / 2, (N64_Y[r] + N64_Y[r + 1]) / 2
            part = GREY if (s0 < cx < s1 and t0 < cy < t1) else CHARCOAL
            m.quad(g[r][c], g[r][c + 1], g[r + 1][c + 1], g[r + 1][c], part)
    rim = ([g[0][c] for c in range(len(N64_X))] + [g[r][-1] for r in range(1, len(N64_Y))] +
           [g[-1][c] for c in range(len(N64_X) - 2, -1, -1)] + [g[r][0] for r in range(len(N64_Y) - 2, 0, -1)])
    low = [m.vert((m.v[q][0], m.v[q][1], N64_DECK_Z)) for q in rim]
    n = len(rim)
    for k in range(n):                                    # counter-clockwise from above: walls out
        m.quad(low[k], low[(k + 1) % n], rim[(k + 1) % n], rim[k], CHARCOAL)
    # the lower body: its outline from above (the front's wings wider), z -0.226 to 0.016
    half = [(-0.655, 0.0), (-0.655, 0.55), (-0.684, 0.75), (-0.66, 0.85), (-0.60, 0.92), (-0.50, 0.947),
            (-0.40, 0.932), (-0.30, 0.914), (-0.20, 0.89), (-0.10, 0.86), (0.0, 0.855), (0.62, 0.84),
            (0.67, 0.80), (0.688, 0.70), (0.688, 0.0)]
    out = [(x, y) for y, x in half] + [(-x, y) for y, x in reversed(half[1:-1])]
    ring0 = [(x, y, N64_BASE_Z) for x, y in out]
    ring1 = [(x, y, N64_DECK_Z) for x, y in out]
    b = [m.vert(p) for p in ring0]
    t = [m.vert(p) for p in ring1]
    n = len(out)
    for k in range(n):
        m.quad(b[k], b[(k + 1) % n], t[(k + 1) % n], t[k], CHARCOAL)
    c0 = m.vert((0, 0, N64_BASE_Z))
    c1 = m.vert((0, 0, N64_DECK_Z))
    for k in range(n):
        m.tri(c0, b[(k + 1) % n], b[k], CHARCOAL)
        m.tri(c1, t[k], t[(k + 1) % n], CHARCOAL)
    # the four controller sockets: solid grey faces on the front, shaded as though facing up
    for (sx, sz_) in N64_SOCKETS:
        c = m.vert((sx, N64_FRONT - 0.006, sz_))
        r = [m.vert((sx + N64_SOCKET_R * math.cos(a), N64_FRONT - 0.006, sz_ + N64_SOCKET_R * math.sin(a)))
             for a in np.linspace(0, 2 * math.pi, 13)[:-1]]
        for k in range(12):
            m.tri(c, r[k], r[(k + 1) % 12], GREY, lit=True)
    return m, N64_COLOURS


# ---- the game's format --------------------------------------------------------------------------
def corner_normals(m):
    P = np.array(m.v)
    T = np.array([t[:3] for t in m.t])
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    un = fn / (np.linalg.norm(fn, axis=1, keepdims=True) + 1e-12)
    key = {}
    for i, t in enumerate(m.t):
        for v in t[:3]:
            key.setdefault((tuple(np.round(P[v], 5)), t[3]), []).append(i)
    out = []
    lim = math.cos(math.radians(45))
    for i, t in enumerate(m.t):
        cn = []
        for v in t[:3]:
            if t[4]:
                cn.append(np.array((0.0, 0.0, 1.0)))
                continue
            acc = np.zeros(3)
            for j in key[(tuple(np.round(P[v], 5)), t[3])]:
                if un[j] @ un[i] >= lim:
                    acc += fn[j]
            cn.append(acc / (np.linalg.norm(acc) + 1e-12))
        out.append(cn)
    return out


def srgb5(c):
    return [int(round(min(max(x, 0), 1) * 31)) for x in c]


def model(m, colours, size_key, size_val):
    P = np.array(m.v)
    used = sorted({t[3] for t in m.t})
    mn, mx = P[[v for t in m.t for v in t[:3]]].min(0), P[[v for t in m.t for v in t[:3]]].max(0)
    scale = size_val / ((mx[2] - mn[2]) if size_key == 'HEIGHT' else max(mx[0] - mn[0], mx[1] - mn[1]))
    off = np.array([-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2] + HOVER / scale])
    cn = corner_normals(m)
    blob = bytearray(8 + 20 * (len(used) + 1))
    struct.pack_into('>II', blob, 0, len(used) + 1, 0)

    def align8():
        while len(blob) % 8:
            blob.append(0)
    ntri = 0
    for pi, part in enumerate(used):
        r5, g5, b5 = srgb5(colours[part])
        align8()
        tex = len(blob)
        blob += struct.pack('>4H', *([(r5 << 11) | (g5 << 6) | (b5 << 1) | 1] * 4))
        # loads of up to 32 corners (a position and normal each), triangles in the order built
        loads, cur, idx, tl = [], [], {}, []
        for i, t in enumerate(m.t):
            if t[3] != part:
                continue
            ks = [(t[k], tuple(np.round(cn[i][k], 3))) for k in range(3)]
            new = [k for k in dict.fromkeys(ks) if k not in idx]
            if len(cur) + len(new) > 32:
                loads.append((cur, tl))
                cur, idx, tl = [], {}, []
                new = list(dict.fromkeys(ks))
            for k in new:
                idx[k] = len(cur)
                cur.append(k)
            tl.append([idx[k] for k in ks])
        if tl:
            loads.append((cur, tl))
        vofs = []
        for vs, _tl in loads:
            align8()
            vofs.append(len(blob))
            for (vi, nrm) in vs:
                p = (P[vi] + off) * scale
                blob += struct.pack('>hhhHhhbbbB', *(int(round(x)) for x in p), 0, 0, 0,
                                    *(int(round(x * 127)) for x in nrm), 255)
        align8()
        dl = len(blob)
        for w0, w1 in ((0xFD100000, tex), (0xE6000000, 0), (0xF3000000, 0x07003000), (0xF5100200, 0x00F00020),
                       (0xF2002002, 0x0000E002), (0xBB000001, 0xFFFFFFFF)):
            blob += struct.pack('>II', w0, w1)
        for (vs, tl), vo in zip(loads, vofs):
            n = len(vs)
            blob += struct.pack('>II', 0x04000000 | (n << 10) | (n * 16 - 1), vo)
            for k in range(0, len(tl), 2):
                a = tl[k]
                w0 = 0xB1000000 | (a[0] * 2 << 16) | (a[1] * 2 << 8) | (a[2] * 2)
                if k + 1 < len(tl):
                    b = tl[k + 1]
                    blob += struct.pack('>II', w0, (b[0] * 2 << 16) | (b[1] * 2 << 8) | (b[2] * 2))
                else:
                    blob += struct.pack('>II', 0xBF000000, (a[0] * 2 << 16) | (a[1] * 2 << 8) | (a[2] * 2))
            ntri += len(tl)
        blob += struct.pack('>II', 0xB8000000, 0)
        # flags 4: both sides drawn (BrModelDraw clears the culling for it)
        struct.pack_into('>IHHfff', blob, 8 + 20 * pi, dl, 4, 0, 0.0, 0.0, 0.0)
    # the shadow: the outline from above (the hull of every vertex) at z 0, at most 32 points
    foot = sorted({tuple(int(round(x)) for x in ((P[v] + off) * scale)[:2]) for t in m.t for v in t[:3]})

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo_, hi_ = [], []
    for q in foot:
        while len(lo_) >= 2 and cross(lo_[-2], lo_[-1], q) <= 0:
            lo_.pop()
        lo_.append(q)
    for q in reversed(foot):
        while len(hi_) >= 2 and cross(hi_[-2], hi_[-1], q) <= 0:
            hi_.pop()
        hi_.append(q)
    ring = lo_[:-1] + hi_[:-1]
    if len(ring) > 32:
        ring = [ring[int(i * len(ring) / 32)] for i in range(32)]
    align8()
    stex = len(blob)
    blob += bytes.fromhex('7777777777777777')
    align8()
    sv = len(blob)
    for (x, y) in ring:
        blob += struct.pack('>hhhHhhbbbB', x, y, 0, 0, 0, 0, 0, 0, 127, 0)
    align8()
    sdl = len(blob)
    for w0, w1 in ((0xFD100000, stex), (0xE6000000, 0), (0xF3000000, 0x07003000), (0xF5800200, 0x00F00040),
                   (0xF2002002, 0x0003E002), (0xBB000001, 0xFFFFFFFF), (0xE7000000, 0),
                   (0xB900031D, 0x0C1849D8), (0xFCFFA004, 0xFFFD93F8), (0xFA001700, 0),
                   (0x04000000 | (len(ring) << 10) | (len(ring) * 16 - 1), sv)):
        blob += struct.pack('>II', w0, w1)
    for k in range(1, len(ring) - 1):
        blob += struct.pack('>II', 0xBF000000, (0 << 16) | (k * 2 << 8) | ((k + 1) * 2))
    for w0, w1 in ((0xE7000000, 0), (0xB900031D, 0x0C192038), (0xFC26A004, 0x1FFC93F8), (0xB8000000, 0)):
        blob += struct.pack('>II', w0, w1)
    struct.pack_into('>IHHfff', blob, 8 + 20 * len(used), sdl, 8, 0, 0.0, 0.0, 0.0)
    return bytes(blob), ntri


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'ports/remastered/tgrally/platform/os/rm_icons.c'
    icons = []
    for name, build, key, val in (('crt', crt, 'HEIGHT', 1160), ('n64', n64, 'WIDTH', 2080)):
        m, colours = build()
        b, ntri = model(m, colours, key, val)
        print(f'handicons: {name} {ntri} triangles, {len(b)} bytes')
        icons.append((name, b))
    with open(out, 'w') as f:
        f.write("/* rm_icons.c: the Remaster Options' menu icons, built by ports/remastered/tools/handicons.py\n")
        f.write(" * (measured off ports/common/assets: crt.blend; n64.blend, after Nintendo 64 by Oliver_Budd_ID,\n")
        f.write(" * CC BY): each the game's own icon model format (tgr/model.h), big-endian, every address an\n")
        f.write(" * offset from its start (rmoptions.c places and rebases them).  Generated: do not edit. */\n")
        for name, b in icons:
            f.write(f'const unsigned int rm_icon_{name}_len = {len(b)};\n')
            f.write(f'const unsigned char rm_icon_{name}[{len(b)}] = {{\n')
            for i in range(0, len(b), 24):
                f.write('    ' + ','.join(str(x) for x in b[i:i + 24]) + ',\n')
            f.write('};\n')


if __name__ == '__main__':
    main()
