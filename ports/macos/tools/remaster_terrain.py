#!/usr/bin/env python3
"""remaster_terrain.py -- the Remastered landscape of a track: one heightfield
that takes over the track's natural ground (verges, banks, meadows) and runs
on to the horizon, read from the retail .TRK.

Nothing the game simulates changes.  The heightfield is held to the track's
own surfaces wherever a car can be:

  natural ground the game draws (grass, rock and earth textures, facing up
      enough to be ground): the field passes through it (a vertex inside one
      of its triangles takes that triangle's height), and those triangles are
      hidden while Remastered is on ("hide" lines, as remaster_env_place.py's);
  everything else the game draws (the road, walls, buildings, water, signs):
      the field stays under it -- every vertex at most the lowest height of
      what is drawn within a cell of it, a little lower under the road, well
      under the lakes (their beds);
  the collision mesh where a car can get to (remaster_env_place.Reach): the
      field never rises above it, and where the game draws nothing over it the
      field is that surface itself (a car can drive there; now there is ground);
  past the edge of the mesh, where a car can go off: the field falls away.

Between those surfaces it is the smoothest field through them (a membrane);
away from the track mountains rise out of it, shaped by a droplet erosion
(remaster_terrain_erode.c).  Two grids: 1 m over the track and its
surroundings, 8 m out to the horizon.

Writes ports/common/models/terrain/<track>.ter (binary, see write_ter) and
ports/common/models/placements/<track>.terrain (its "hide" lines).
"""
import math, os, struct, subprocess, sys, time
import numpy as np
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remaster_env_place as P

ROOT = P.ROOT
OUT = os.path.join(P.MODELS, "terrain")
BASE = P.BASE
be32, off = P.be32, P.off

# Natural ground, per track: the textures (N64 address of the texels) that
# show grass, earth or rock -- tagged by eye from the decoded textures of the
# track (every texture with more than ~300 m2 drawn).  Road, water, walls of
# stone blocks, signs, chevrons, rails, tunnel mouths and cut-out cards are
# left out: the game keeps drawing them.
NATURAL = {
    "mountain": [0x800397E0, 0x8003A260, 0x800360A0, 0x80033BA0, 0x800425E0, 0x8005D130,
                 0x8003FBE0, 0x80043060, 0x800467E0, 0x80038560, 0x800442E0, 0x80062FB0,
                 0x80030720, 0x80033120, 0x80047CE0, 0x8004E5E0, 0x8004BBE0, 0x80074930,
                 0x8004B160, 0x80057BC0, 0x80061AB0, 0x8004DB60, 0x80038D60, 0x8002F220,
                 0x80057140, 0x80074130, 0x80069870, 0x8004D0E0, 0x8002D2A0, 0x800659B0,
                 0x80056C00, 0x800311A0, 0x800534A0, 0x80040660, 0x8006FF70, 0x800605B0,
                 0x8006EF70, 0x8004C660, 0x80075130, 0x80056180, 0x80031C20, 0x80061030,
                 0x8003ACE0, 0x8007A130, 0x80073BF0, 0x80062530, 0x80079130, 0x8004A6E0],
}
WATER = {"mountain": [0x80075930]}
# Grass fringes: natural textures with ragged cut-out edges, drawn as
# saw-tooth strips along the bank tops -- decoration, not ground (held to,
# their teeth would stand up out of the land); hidden, the grass replaces them
FRINGE = {"mountain": [0x8005D130, 0x80062FB0, 0x800659B0, 0x8006EF70, 0x8005FB30, 0x800605B0]}
# Painted backdrops: upright faces painted with forest or far rock, standing
# off the collision mesh (no car touches them); the landscape and its trees
# take their place.  Each triangle is checked: one within a metre of the
# collision mesh is a wall a car can hit, and stays.
BACKDROP = {"mountain": [0x80038560, 0x80074930]}
# Stone walls a car can hit (upright, on the collision mesh): rebuilt from
# scanned rock faces standing wholly behind the wall's plane
STONEWALL = {"mountain": [0x80057BC0, 0x80061AB0, 0x80057140, 0x8003A260, 0x800425E0, 0x8003FBE0, 0x800467E0]}
WALLROCKS = ["rock_face_01", "rock_face_02", "mountainside"]

def rcm_points(asset, variant="a", lod=2):
    """a baked model's vertex positions (its coarsest level), model units"""
    import glob
    pts = []
    for f in sorted(glob.glob(os.path.join(P.MODELS, asset, f"{variant}_lod{lod}_m*.rcm"))):
        b = open(f, "rb").read()
        nv = struct.unpack_from("<I", b, 4)[0]
        v = np.frombuffer(b, "<f4", nv * 12, 12).reshape(nv, 12)[:, :3]
        pts.append(v)
    return np.concatenate(pts) if pts else np.zeros((0, 3))

def front_axis(pts):
    """the model's flattest side (of +-x, +-y): the yaw that turns it to face +x"""
    best = None
    for yaw in (0.0, math.pi / 2, math.pi, -math.pi / 2):
        d = pts[:, 0] * math.cos(yaw) + pts[:, 1] * math.sin(yaw)
        front = d.max()
        flat = (d > front - 0.6).mean()          # share of the model within 0.6 of its front
        if best is None or flat > best[0]:
            best = (flat, yaw)
    return best[1]

# A real landscape under a track (remaster_terrain_dem.py's mosaic): where
# the track sits in it, found by a search over positions and headings for the
# best match of the real ground to the track's own held heights, with no road
# over open water -- Mountain beside Lake Silvaplana, at Surlej in the Upper
# Engadin, its lowest ground (its own lake) at 1795 m.
REAL = {
    "mountain": dict(npy="build/terrain/engadin.npy", e0=2772000.0, n0=1136000.0, cell=2.0,
                     E=2779100.0, N=1144700.0, angle=60.0, offset=1795.0,
                     near_margin=1500.0, far_half=9000.0, far_cell=6.0),
}

NEAR_CELL = 1.0
NEAR_MARGIN = 420.0
FAR_CELL = 4.0
FAR_HALF = 8192.0
GROUND_NZ = 0.3          # natural triangles at least this upward are ground the field takes over
UNDER_ROAD = 0.08        # the field under the road
UNDER_KEPT = 0.04        # under everything else the game draws
LAKE_DEPTH = 2.5         # a lake's bed under its water

def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)

# ---------------------------------------------------------------- the track

def triangles(b):
    """every triangle the instances draw: world corners, texture, instance,
    command offset in the instance's list, which triangle of the command"""
    ia, ci = be32(b, 0x60), be32(b, 0x64)
    pos, tex, inst, cmd, half = [], [], [], [], []
    for i in range(ci):
        r = off(ia) + i * 0x54
        M = np.array(struct.unpack_from(">16f", b, r), dtype=np.float64).reshape(4, 4)
        dl = be32(b, r + 0x44)
        if not dl:
            continue
        for o, t, tris, _tf in P.walk(b, off(dl)):
            for k, tr in enumerate(tris):
                w = (np.hstack([np.array(tr, float), np.ones((3, 1))]) @ M)[:, :3]
                pos.append(w); tex.append(t); inst.append(i); cmd.append(o); half.append(k)
    return np.array(pos), np.array(tex, np.int64), np.array(inst), np.array(cmd), np.array(half)

def collision(b):
    nf, nv = be32(b, 0x08), be32(b, 0x10)
    V = np.frombuffer(b, ">f4", nv * 3, off(be32(b, 0x14))).reshape(-1, 3).astype(float)
    F = np.frombuffer(b, ">u2", nf * 4, off(be32(b, 0x0C))).reshape(-1, 4)[:, :3].astype(int)
    return V[F]

def normals(T):
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    a = np.linalg.norm(n, axis=1)
    return n / np.maximum(a, 1e-12)[:, None], a / 2

# ------------------------------------------------------------- rasterising

class Grid:
    def __init__(self, x0, y0, cell, w, h):
        self.x0, self.y0, self.cell, self.w, self.h = x0, y0, cell, w, h
    def ij(self, x, y):
        return (x - self.x0) / self.cell, (y - self.y0) / self.cell

def tri_vertices(g, t):
    """grid vertices inside triangle t's xy, and the triangle's height there"""
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(d) < 1e-9:
        return None
    i0 = max(0, int(math.ceil((min(ax, bx, cx) - g.x0) / g.cell)))
    i1 = min(g.w - 1, int(math.floor((max(ax, bx, cx) - g.x0) / g.cell)))
    j0 = max(0, int(math.ceil((min(ay, by, cy) - g.y0) / g.cell)))
    j1 = min(g.h - 1, int(math.floor((max(ay, by, cy) - g.y0) / g.cell)))
    if i1 < i0 or j1 < j0:
        return None
    X, Y = np.meshgrid(g.x0 + np.arange(i0, i1 + 1) * g.cell, g.y0 + np.arange(j0, j1 + 1) * g.cell)
    l1 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d
    l2 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d
    l3 = 1 - l1 - l2
    m = (l1 >= -1e-6) & (l2 >= -1e-6) & (l3 >= -1e-6)
    if not m.any():
        return None
    Z = l1 * az + l2 * bz + l3 * cz
    jj, ii = np.nonzero(m)
    return jj + j0, ii + i0, Z[m]

def tri_near(g, t, reach):
    """grid vertices within `reach` of triangle t's xy, and the lowest the
    triangle comes near each (its height at the nearest point, less its rise
    over `reach`)"""
    p = t[:, :2]
    i0 = max(0, int(math.floor((p[:, 0].min() - reach - g.x0) / g.cell)))
    i1 = min(g.w - 1, int(math.ceil((p[:, 0].max() + reach - g.x0) / g.cell)))
    j0 = max(0, int(math.floor((p[:, 1].min() - reach - g.y0) / g.cell)))
    j1 = min(g.h - 1, int(math.ceil((p[:, 1].max() + reach - g.y0) / g.cell)))
    if i1 < i0 or j1 < j0:
        return None
    X, Y = np.meshgrid(g.x0 + np.arange(i0, i1 + 1) * g.cell, g.y0 + np.arange(j0, j1 + 1) * g.cell)
    Q = np.stack([X, Y], -1)
    # distance to the triangle in xy and the barycentric of the nearest point
    a, b_, c = p
    def seg(q, s0, s1):
        d = s1 - s0
        tt = np.clip(((q - s0) @ d) / max(d @ d, 1e-12), 0, 1)
        return s0 + tt[..., None] * d
    det = (b_[1] - c[1]) * (a[0] - c[0]) + (c[0] - b_[0]) * (a[1] - c[1])
    if abs(det) < 1e-9:
        # a vertical face: its lowest point along the nearest edge
        best = None
        for s0, s1, z0, z1 in ((a, b_, t[0, 2], t[1, 2]), (b_, c, t[1, 2], t[2, 2]), (c, a, t[2, 2], t[0, 2])):
            near = seg(Q, s0, s1)
            dist = np.linalg.norm(Q - near, axis=-1)
            zz = np.full(dist.shape, min(z0, z1))
            cand = np.where(dist <= reach, zz, np.inf)
            best = cand if best is None else np.minimum(best, cand)
        m = np.isfinite(best)
        jj, ii = np.nonzero(m)
        return jj + j0, ii + i0, best[m]
    l1 = ((b_[1] - c[1]) * (X - c[0]) + (c[0] - b_[0]) * (Y - c[1])) / det
    l2 = ((c[1] - a[1]) * (X - c[0]) + (a[0] - c[0]) * (Y - c[1])) / det
    l3 = 1 - l1 - l2
    inside = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
    near = np.where(inside[..., None], Q, Q)
    dist = np.zeros(X.shape)
    if not inside.all():
        cands = [seg(Q, a, b_), seg(Q, b_, c), seg(Q, c, a)]
        ds = [np.linalg.norm(Q - cq, axis=-1) for cq in cands]
        k = np.argmin(np.stack(ds), 0)
        cq = np.choose(k[..., None], cands)
        dist = np.where(inside, 0.0, np.min(np.stack(ds), 0))
        near = np.where(inside[..., None], Q, cq)
    m = dist <= reach
    if not m.any():
        return None
    nx, ny = near[..., 0], near[..., 1]
    L1 = ((b_[1] - c[1]) * (nx - c[0]) + (c[0] - b_[0]) * (ny - c[1])) / det
    L2 = ((c[1] - a[1]) * (nx - c[0]) + (a[0] - c[0]) * (ny - c[1])) / det
    Z = L1 * t[0, 2] + L2 * t[1, 2] + (1 - L1 - L2) * t[2, 2]
    # the plane's rise over `reach`
    gz = np.array([[a[0] - c[0], a[1] - c[1]], [b_[0] - c[0], b_[1] - c[1]]])
    try:
        grad = np.linalg.solve(gz, np.array([t[0, 2] - t[2, 2], t[1, 2] - t[2, 2]]))
    except np.linalg.LinAlgError:
        grad = np.zeros(2)
    Z = Z - np.linalg.norm(grad) * reach
    Z = np.maximum(Z, t[:, 2].min())
    jj, ii = np.nonzero(m)
    return jj + j0, ii + i0, Z[m]

# ------------------------------------------------------------------ solving

def down(a, mode):
    h, w = a.shape
    h2, w2 = (h + 1) // 2, (w + 1) // 2
    p = np.pad(a, ((0, h2 * 2 - h), (0, w2 * 2 - w)), mode="edge")
    q = p.reshape(h2, 2, w2, 2)
    if mode == "min":
        return q.min((1, 3))
    # mean of the fixed (finite) values
    f = np.isfinite(q)
    s = np.where(f, q, 0).sum((1, 3)); n = f.sum((1, 3))
    return np.where(n > 0, s / np.maximum(n, 1), np.nan)

def up(a, shape):
    z = ndimage.zoom(a, 2, order=1)
    out = np.empty(shape)
    h, w = shape
    z = np.pad(z, ((0, max(0, h - z.shape[0])), (0, max(0, w - z.shape[1]))), mode="edge")
    out[:] = z[:h, :w]
    return out

def membrane(fixed, U, iters=(4000, 400, 300, 200, 160, 120, 100, 80, 80, 60, 60), L=None):
    """the smoothest field through `fixed` (nan: free) under `U`: Jacobi over a
    pyramid, coarse to fine"""
    if L is None:
        L = np.full(fixed.shape, -np.inf)
    levels = [(fixed, U, L)]
    while min(levels[-1][0].shape) > 48:
        f, u, l = levels[-1]
        levels.append((down(f, "mean"), down(u, "min"), -down(-l, "min")))
    H = None
    for k in range(len(levels) - 1, -1, -1):
        f, u, l = levels[k]
        fm = np.isfinite(f)
        if H is None:
            H = np.full(f.shape, np.nanmean(f) if fm.any() else 0.0)
        else:
            H = up(H, f.shape)
        fv = np.where(fm, f, 0)
        n = iters[min(len(levels) - 1 - k, len(iters) - 1)] if k else max(iters[-1], 60)
        for _ in range(n):
            p = np.pad(H, 1, mode="edge")
            H = 0.25 * (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:])
            H = np.where(fm, fv, np.minimum(np.maximum(H, l), u))
        log(f"  membrane level {k} {f.shape} {n} iterations")
    return H

def thinplate(H, fixed, U, L, iters=(600, 300, 200, 120, 80, 60, 40)):
    """the field bent as little as possible through `fixed` under `U`, from
    `H` (the membrane): damped Jacobi on the biharmonic stencil (omega 0.5,
    under the 0.625 it needs to converge), coarse to fine.  A membrane cones
    to every isolated held point; a thin plate rounds over it."""
    levels = [(fixed, U, H, L)]
    while min(levels[-1][0].shape) > 48:
        f, u, hh, l = levels[-1]
        levels.append((down(f, "mean"), down(u, "min"), down(hh, "mean"), -down(-l, "min")))
    G = None
    for k in range(len(levels) - 1, -1, -1):
        f, u, hh, l = levels[k]
        fm = np.isfinite(f); fv = np.where(fm, f, 0)
        if G is None:
            G = hh.copy()
        else:
            # carry the coarse correction, not the coarse field
            G = hh + up(G - levels[k + 1][2], f.shape)
        n = iters[min(len(levels) - 1 - k, len(iters) - 1)]
        for _ in range(n):
            p = np.pad(G, 2, mode="edge")
            c = p[2:-2, 2:-2]
            n4 = p[1:-3, 2:-2] + p[3:-1, 2:-2] + p[2:-2, 1:-3] + p[2:-2, 3:-1]
            d4 = p[1:-3, 1:-3] + p[1:-3, 3:-1] + p[3:-1, 1:-3] + p[3:-1, 3:-1]
            f4 = p[:-4, 2:-2] + p[4:, 2:-2] + p[2:-2, :-4] + p[2:-2, 4:]
            tgt = (8 * n4 - 2 * d4 - f4) / 20.0
            G = c + 0.5 * (tgt - c)
            G = np.where(fm, fv, np.minimum(np.maximum(G, l), u))
        log(f"  thin plate level {k} {f.shape} {n} iterations")
    return G

def uplift(H0, Uf, fixed, cell, steps):
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "remaster_terrain_uplift.c")
    exe = os.path.join(ROOT, "build", "terrain", "uplift")
    if not os.path.exists(exe) or os.path.getmtime(exe) < os.path.getmtime(src):
        subprocess.check_call(["clang", "-O3", "-ffast-math", "-o", exe, src, "-lm"])
    tmp = os.path.join(ROOT, "build", "terrain")
    H0.astype(np.float32).tofile(os.path.join(tmp, "u_in.f32"))
    Uf.astype(np.float32).tofile(os.path.join(tmp, "u_up.f32"))
    fixed.astype(np.uint8).tofile(os.path.join(tmp, "u_fix.u8"))
    subprocess.check_call([exe, str(H0.shape[1]), str(H0.shape[0]), str(cell), str(steps),
                           os.path.join(tmp, "u_in.f32"), os.path.join(tmp, "u_up.f32"), os.path.join(tmp, "u_fix.u8"),
                           os.path.join(tmp, "u_out.f32"), os.path.join(tmp, "u_area.f32")])
    out = np.fromfile(os.path.join(tmp, "u_out.f32"), np.float32).reshape(H0.shape).astype(np.float64)
    area = np.fromfile(os.path.join(tmp, "u_area.f32"), np.float32).reshape(H0.shape)
    return out, area

# ---------------------------------------------------------------- landscape

def fbm(X, Y, seed, octaves, scale, ridged=False, gain=0.5, lac=2.03):
    rng = np.random.default_rng(seed)
    out = np.zeros(X.shape); amp = 1.0; norm = 0.0; f = 1.0 / scale
    for o in range(octaves):
        ox, oy, ang = rng.uniform(-1e4, 1e4), rng.uniform(-1e4, 1e4), rng.uniform(0, math.pi)
        ca, sa = math.cos(ang), math.sin(ang)
        x = (X * ca - Y * sa) * f + ox; y = (X * sa + Y * ca) * f + oy
        n = vnoise(x, y, seed * 31 + o)
        if ridged:
            n = 1.0 - np.abs(n * 2 - 1); n = n * n
        out += n * amp; norm += amp; amp *= gain; f *= lac
    return out / norm

def vnoise(x, y, seed):
    xi, yi = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = x - xi, y - yi
    s = np.uint64((seed * 2654435761) & 0xFFFFFFFF)
    def h(i, j):
        v = (i.astype(np.uint64) * np.uint64(374761393) + j.astype(np.uint64) * np.uint64(668265263) + s) & np.uint64(0xFFFFFFFF)
        v = ((v ^ (v >> np.uint64(13))) * np.uint64(1274126177)) & np.uint64(0xFFFFFFFF)
        return (v ^ (v >> np.uint64(16))).astype(np.float64) / 4294967295.0
    sx, sy = fx * fx * fx * (fx * (fx * 6 - 15) + 10), fy * fy * fy * (fy * (fy * 6 - 15) + 10)
    a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * sx
    b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * sx
    return a + (b - a) * sy

def erode(H, strength, cell, droplets, seed, flow_out=False):
    """remaster_terrain_erode.c over H (row-major float32); `strength` 0..1 per
    cell scales what a droplet may take or leave there"""
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "remaster_terrain_erode.c")
    exe = os.path.join(ROOT, "build", "terrain", "erode")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    if not os.path.exists(exe) or os.path.getmtime(exe) < os.path.getmtime(src):
        subprocess.check_call(["clang", "-O3", "-ffast-math", "-o", exe, src, "-lm"])
    tmp = os.path.join(ROOT, "build", "terrain")
    H.astype(np.float32).tofile(os.path.join(tmp, "in.f32"))
    strength.astype(np.float32).tofile(os.path.join(tmp, "mask.f32"))
    subprocess.check_call([exe, str(H.shape[1]), str(H.shape[0]), str(cell), str(droplets), str(seed),
                           os.path.join(tmp, "in.f32"), os.path.join(tmp, "mask.f32"), os.path.join(tmp, "out.f32"),
                           os.path.join(tmp, "flow.f32"), os.path.join(tmp, "dep.f32")])
    out = np.fromfile(os.path.join(tmp, "out.f32"), np.float32).reshape(H.shape).astype(np.float64)
    flow = np.fromfile(os.path.join(tmp, "flow.f32"), np.float32).reshape(H.shape)
    dep = np.fromfile(os.path.join(tmp, "dep.f32"), np.float32).reshape(H.shape)
    return out, flow, dep

# ------------------------------------------------------------------- output

def write_ter(path, near, Hn, far, Hf, nmask, fflow, fdep, nflow, fcls, nroad):
    """TER2 header (little-endian): magic, near x0 y0 cell w h, far x0 y0
    cell w h; then near heights (float32 w*h, row y from y0 up), far heights,
    near class bytes (w*h: 0 free, 1 held to the track's ground, 2 under what
    the game draws, 3 the road, 4 a lake bed, 5 the track's ground beyond a car's reach: a band), far flow and deposit (float32),
    near flow (float32), far class bytes
    (1 open water: a real lake's surface), near distance to the road (bytes,
    decimetres, 255 at 25.5 m and beyond)."""
    with open(path, "wb") as f:
        f.write(b"TER3")
        f.write(struct.pack("<3f2i", near.x0, near.y0, near.cell, near.w, near.h))
        f.write(struct.pack("<3f2i", far.x0, far.y0, far.cell, far.w, far.h))
        f.write(Hn.astype("<f4").tobytes()); f.write(Hf.astype("<f4").tobytes())
        f.write(nmask.astype(np.uint8).tobytes())
        f.write(fflow.astype("<f4").tobytes()); f.write(fdep.astype("<f4").tobytes())
        f.write(nflow.astype("<f4").tobytes())
        f.write(fcls.astype(np.uint8).tobytes())
        f.write(nroad.astype(np.uint8).tobytes())

def point_tri(p, a, b, c):
    """distance from p to triangle abc (Ericson's closest point)"""
    ab, ac, ap = b - a, c - a, p - a
    d1, d2 = ab @ ap, ac @ ap
    if d1 <= 0 and d2 <= 0: return np.linalg.norm(ap)
    bp = p - b; d3, d4 = ab @ bp, ac @ bp
    if d3 >= 0 and d4 <= d3: return np.linalg.norm(bp)
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0: return np.linalg.norm(p - (a + d1 / (d1 - d3) * ab))
    cp = p - c; d5, d6 = ab @ cp, ac @ cp
    if d6 >= 0 and d5 <= d6: return np.linalg.norm(cp)
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0: return np.linalg.norm(p - (a + d2 / (d2 - d6) * ac))
    va = d3 * d6 - d5 * d4
    if va <= 0 and d4 - d3 >= 0 and d5 - d6 >= 0:
        return np.linalg.norm(p - (b + (d4 - d3) / ((d4 - d3) + (d5 - d6)) * (c - b)))
    den = 1 / (va + vb + vc)
    return np.linalg.norm(p - (a + ab * vb * den + ac * vc * den))

def off_collision(t, C, lo, hi, gap):
    """is no point of triangle t (corners, centre, edge middles) within `gap`
    of the collision mesh?"""
    pts = [t[0], t[1], t[2], t.mean(0), (t[0] + t[1]) / 2, (t[1] + t[2]) / 2, (t[2] + t[0]) / 2]
    for q in pts:
        near = np.nonzero(np.all((lo - gap <= q) & (hi + gap >= q), axis=1))[0]
        for f in near:
            if point_tri(q, *C[f]) < gap:
                return False
    return True

def lakes(H, cell, below):
    """open water in a real elevation model: its surveyed lake surfaces, dead
    flat over a large area under `below` metres"""
    gy, gx = np.gradient(H, cell)
    flat = (np.abs(gx) + np.abs(gy)) < 0.003
    flat &= ndimage.maximum_filter(H, size=5) - ndimage.minimum_filter(H, size=5) < 0.15
    flat &= H < below
    flat = ndimage.binary_opening(flat, iterations=3)
    lab, n = ndimage.label(flat)
    if n:
        sizes = ndimage.sum(flat, lab, range(1, n + 1)) * cell * cell
        keep = np.isin(lab, 1 + np.nonzero(sizes > 20000)[0])
        return ndimage.binary_closing(keep, iterations=2) & flat | keep
    return flat

def real_landscape(real, near, M, fixed, cls, U, Lb):
    """the real landscape around the track: the elevation model in the track's
    frame, bent gently (a membrane of the difference, gone 1.5 km out) so it
    meets the track's held ground; the near field blends from the track's
    own solve into it over the first 200 m"""
    dem = np.load(os.path.join(ROOT, real["npy"]), mmap_mode="r")
    hard = np.isfinite(fixed) & (cls == 1)
    jj, ii = np.nonzero(hard)
    c0 = np.array([near.x0 + ii.mean() * near.cell, near.y0 + jj.mean() * near.cell])
    a = math.radians(real["angle"]); ca, sa = math.cos(a), math.sin(a)
    def dem_at(X, Y, order=3):
        dx, dy = X - c0[0], Y - c0[1]
        E = real["E"] + ca * dx - sa * dy; N = real["N"] + sa * dx + ca * dy
        out = np.empty(X.shape, np.float64)
        rows = (N - real["n0"]) / real["cell"]; cols = (E - real["e0"]) / real["cell"]
        step = max(1, X.shape[0] // 16)
        for r0 in range(0, X.shape[0], step):
            out[r0:r0 + step] = ndimage.map_coordinates(dem, [rows[r0:r0 + step], cols[r0:r0 + step]], order=order, mode="nearest")
        return out - real["offset"]
    fc, fh = real["far_cell"], real["far_half"]
    far = Grid(c0[0] - fh, c0[1] - fh, fc, int(2 * fh / fc) + 1, int(2 * fh / fc) + 1)
    log(f"real landscape: far grid {far.w} x {far.h} at {fc:.0f} m, near {near.w} x {near.h}")
    Xf, Yf = np.meshgrid(far.x0 + np.arange(far.w) * fc, far.y0 + np.arange(far.h) * fc)
    Df = dem_at(Xf, Yf)
    # the track's own ground on the far grid, and the difference to bend away
    ours = cls > 0
    k = int(round(fc / near.cell))
    fi0 = int(round((near.x0 - far.x0) / fc)); fj0 = int(round((near.y0 - far.y0) / fc))
    sub = M[::k, ::k]; tsub = ndimage.maximum_filter(ours.astype(np.uint8), size=k)[::k, ::k] > 0
    hs, ws = sub.shape
    resid = np.full((far.h, far.w), np.nan)
    resid[fj0:fj0 + hs, fi0:fi0 + ws] = np.where(tsub, sub - Df[fj0:fj0 + hs, fi0:fi0 + ws], np.nan)
    tf = np.isfinite(resid)
    df = ndimage.distance_transform_edt(~tf) * fc
    # the real lakes stay at their surveyed level, and the bend is gone 400 m
    # out: the land rises or falls to the track like a bank, the valley stays
    lake_f = lakes(Df + real["offset"], fc, 1815.0) & (df > 30.0)
    resid[(df > 400.0) | lake_f] = 0.0
    log(f"  track vs the real ground: median {np.nanmedian(resid[tf]):.1f} m, 90% within {np.nanpercentile(np.abs(resid[tf]), 90):.1f} m")
    Rf = membrane(resid, np.full(resid.shape, np.inf), iters=(3000, 300, 200, 150, 100, 80, 60))
    Hf = Df + Rf
    # near: the real ground (bent), blended in from the track's own solve
    log("  near: sampling the real ground")
    Xn, Yn = np.meshgrid(near.x0 + np.arange(near.w) * near.cell, near.y0 + np.arange(near.h) * near.cell)
    Dn = dem_at(Xn, Yn) + ndimage.map_coordinates(Rf, [(Yn - far.y0) / fc, (Xn - far.x0) / fc], order=1)
    dn = ndimage.distance_transform_edt(~ours) * near.cell
    w = np.clip((dn - 10.0) / 90.0, 0, 1); w = w * w * (3 - 2 * w)
    Hn = M * (1 - w) + Dn * w
    held = np.isfinite(fixed)
    Hn = np.where(held, fixed, np.clip(Hn, Lb, U))
    # the far grid under the near one carries the near one's heights
    Hf[fj0:fj0 + hs, fi0:fi0 + ws] = Hn[::k, ::k][:hs, :ws]
    # water: the real lakes (not on the track's own ground)
    lake_n = lakes(dem_at(Xn, Yn, order=1) + real["offset"], near.cell, 1815.0) & (dn > 20.0) & ~held & (cls == 0)
    cls[lake_n] = 6
    log(f"  lakes: {lake_f.sum() * fc * fc / 1e6:.2f} km2 on the far grid, {lake_n.sum() / 1e6:.3f} km2 near")
    # the water courses, for the ground's materials: drops run, nothing moves
    log("  water courses")
    _, fflow, fdep = erode(Hf, np.zeros(Hf.shape), fc, 3_000_000, 7)
    _, nflow, _ = erode(Hn, np.zeros(Hn.shape), near.cell, 3_000_000, 9)
    return Hn, Hf, far, fflow, fdep, nflow, lake_f.astype(np.uint8)

# What grows on the landscape, per track: the forest's trees (asset, variants,
# share), the saplings at its edges, the rocks on steep and broken ground.
SCATTER_T = {
    "mountain": dict(
        # Norway spruce, the Engadin's own tree, with hemlock for the firs'
        # softer shape and a few of the older pack's for variety
        trees=[("sf_norway_spruce_23", "a", 1.0), ("sf_norway_spruce_17", "a", 1.0), ("sf_norway_spruce_02", "a", 1.0),
               ("sf_norway_spruce_18", "a", 1.0), ("sf_norway_spruce_33", "a", 1.0),
               ("sf_mountain_hemlock_07", "a", 0.6), ("sf_mountain_hemlock_23", "a", 0.6),
               ("sf_realistic_fir_trees_pack_lod", "abcdefghi", 0.5)],
        # ferns: the forest floor and its edges, and the shade along the road
        under=[("sf_male_fern_03", "a", 1.0), ("sf_male_fern_46", "a", 1.0), ("sf_polypody_fern_25", "a", 0.6)],
        edge=[("fir_sapling_medium", "abc", 0.6), ("pine_sapling_medium", "abc", 0.4)],
        rocks=[("rock_moss_set_01", ["rock01", "rock02", "rock03", "rock04", "rock05", "rock06"], 0.7),
               ("boulder_01", ["a"], 0.3)],
        cliffs=[("rock_face_01", ["a"], 1.0), ("rock_face_02", ["a"], 1.0), ("namaqualand_cliff_01", ["a"], 0.8),
                ("namaqualand_cliff_02", ["a"], 0.6), ("mountainside", ["a"], 0.8)],
        treeline=420.0, spacing=3.3, tree_h=(14.0, 26.0)),
}

def scatter(track, near, Hn, cls, nflow, real, wall_rocks=()):
    """trees and rocks over the landscape: forests below the tree line on
    ground a car cannot reach (8 m clear of the track's own), thinning to
    their edges and opening into meadows; rocks where the land is steep or
    broken.  Writes placements/<track>.scatter (binary, see below)."""
    cfg = SCATTER_T.get(track)
    if not cfg:
        return
    rng = np.random.default_rng(5)
    ours = (cls >= 1) & (cls <= 4)
    clear = ndimage.distance_transform_edt(~ours) * near.cell
    gy, gx = np.gradient(Hn, near.cell)
    slope = 1.0 - 1.0 / np.sqrt(1 + gx * gx + gy * gy)
    X0, Y0 = near.x0, near.y0
    def at(a, x, y):
        return a[np.clip(((y - Y0) / near.cell).astype(int), 0, near.h - 1), np.clip(((x - X0) / near.cell).astype(int), 0, near.w - 1)]
    # candidates on a jittered grid
    sp = cfg["spacing"]
    gxs = np.arange(near.x0 + sp, near.x0 + (near.w - 2) * near.cell - sp, sp)
    gys = np.arange(near.y0 + sp, near.y0 + (near.h - 2) * near.cell - sp, sp)
    CX, CY = np.meshgrid(gxs, gys)
    CX = (CX + rng.uniform(-0.45, 0.45, CX.shape) * sp).ravel(); CY = (CY + rng.uniform(-0.45, 0.45, CY.shape) * sp).ravel()
    z = at(Hn, CX, CY); sl = at(slope, CX, CY); cl = at(clear, CX, CY); c = at(cls, CX, CY)
    fl = np.log2(1 + at(nflow, CX, CY))
    cover = fbm(CX, CY, 51, 4, 520.0) + 0.12 * (fbm(CX, CY, 52, 3, 90.0) - 0.5)
    tl = cfg["treeline"] + (fbm(CX, CY, 53, 3, 900.0) - 0.5) * 160
    dens = np.clip((cover - 0.36) / 0.1, 0, 1) * np.clip((tl - z) / 80.0, 0, 1) * np.clip((0.62 - sl) / 0.15, 0, 1)
    dens = np.clip((cover - 0.40) / 0.12, 0, 1) * np.clip((tl - z) / 80.0, 0, 1) * np.clip((0.62 - sl) / 0.15, 0, 1) \
           * np.clip((cl - 8.0) / 10.0, 0, 1) * ((c == 0) | (c == 5))
    pick = rng.random(dens.shape) < dens * 0.92
    edge = (dens > 0.05) & (dens < 0.55)
    recs = []          # (asset index, variant, x, y, z, yaw, scale)
    assets = []
    def aidx(name, var):
        key = (name, var)
        if key not in assets:
            assets.append(key)
        return assets.index(key)
    def choose(pool):
        tot = sum(w for _, _, w in pool); x = rng.random() * tot
        for n, vs, w in pool:
            x -= w
            if x <= 0:
                return n, vs[rng.integers(len(vs))]
        return pool[-1][0], pool[-1][1][0]
    tree_names = {t[0] for t in cfg["trees"]}
    for i in np.nonzero(pick)[0]:
        n, v = choose(cfg["edge"] if edge[i] and rng.random() < 0.6 else cfg["trees"])
        h = rng.uniform(*cfg["tree_h"]) * (0.55 if n not in tree_names else 1.0) * (0.7 + 0.3 * dens[i])
        recs.append((aidx(n, v), CX[i], CY[i], z[i] - 0.15, rng.uniform(0, 2 * math.pi), h))
    ntree = len(recs)
    # ferns: in clumps on the forest floor and along its edges, and in the
    # verges' patches of shade; small (60-110 cm), many
    if cfg.get("under"):
        sp3 = 1.6
        gx3 = np.arange(near.x0 + sp3, near.x0 + (near.w - 2) * near.cell - sp3, sp3)
        gy3 = np.arange(near.y0 + sp3, near.y0 + (near.h - 2) * near.cell - sp3, sp3)
        FX, FY = np.meshgrid(gx3, gy3)
        FX = (FX + rng.uniform(-0.5, 0.5, FX.shape) * sp3).ravel(); FY = (FY + rng.uniform(-0.5, 0.5, FY.shape) * sp3).ravel()
        fz_ = at(Hn, FX, FY); fs_ = at(slope, FX, FY); fc_ = at(cls, FX, FY); fcl = at(clear, FX, FY)
        fcov = fbm(FX, FY, 51, 4, 520.0) + 0.12 * (fbm(FX, FY, 52, 3, 90.0) - 0.5)
        ftl = cfg["treeline"] + (fbm(FX, FY, 53, 3, 900.0) - 0.5) * 160
        forest = np.clip((fcov - 0.36) / 0.12, 0, 1) * np.clip((ftl - fz_) / 80.0, 0, 1)
        clumps = fbm(FX, FY, 56, 3, 14.0)
        verge = np.clip(1.0 - np.abs(fcl - 6.0) / 4.0, 0, 1) * (fbm(FX, FY, 57, 3, 30.0) > 0.55)
        want = (forest * 0.5 + verge * 0.35) * np.clip((clumps - 0.45) / 0.15, 0, 1) * np.clip((0.55 - fs_) / 0.15, 0, 1) \
               * np.clip((fcl - 2.5) / 1.5, 0, 1) * ((fc_ == 0) | (fc_ == 5))
        for i in np.nonzero(rng.random(want.shape) < want)[0]:
            n, v = choose(cfg["under"])
            recs.append((aidx(n, v), FX[i], FY[i], fz_[i] - 0.05, rng.uniform(0, 2 * math.pi), rng.uniform(0.6, 1.1)))
    nfern = len(recs) - ntree
    # rocks: steep, broken or stony ground, also above the tree line
    rk = np.clip((sl - 0.32) / 0.2, 0, 1) * 0.5 + np.clip((fl - 4.5) / 2.0, 0, 1) * 0.25 * (sl > 0.12)
    rk = rk * np.clip((cl - 6.0) / 8.0, 0, 1) * ((c == 0) | (c == 5)) * (fbm(CX, CY, 54, 3, 260.0) > 0.45)
    pr = rng.random(rk.shape) < rk * 0.5
    for i in np.nonzero(pr)[0]:
        n, v = choose(cfg["rocks"])
        recs.append((aidx(n, v), CX[i] + rng.uniform(-1, 1), CY[i] + rng.uniform(-1, 1), z[i] - 0.3, rng.uniform(0, 2 * math.pi),
                     rng.uniform(0.8, 2.6) * (2.2 if n == "boulder_01" else 1.0)))
    # outcrops: the scanned cliffs and rock faces on steep ground, big, sunk
    # into the slope, turned to face down it
    nrock = len(recs)
    sp2 = 11.0
    gx2 = np.arange(near.x0 + sp2, near.x0 + (near.w - 2) * near.cell - sp2, sp2)
    gy2 = np.arange(near.y0 + sp2, near.y0 + (near.h - 2) * near.cell - sp2, sp2)
    QX, QY = np.meshgrid(gx2, gy2)
    QX = (QX + rng.uniform(-0.45, 0.45, QX.shape) * sp2).ravel(); QY = (QY + rng.uniform(-0.45, 0.45, QY.shape) * sp2).ravel()
    qs = at(slope, QX, QY); qc = at(cls, QX, QY); ql = at(clear, QX, QY); qz = at(Hn, QX, QY)
    gxq, gyq = at(gx, QX, QY), at(gy, QX, QY)
    want = np.clip((qs - 0.42) / 0.2, 0, 1) * ((qc == 0) | (qc == 5)) * np.clip((ql - 6.0) / 6.0, 0, 1)
    want *= fbm(QX, QY, 55, 3, 180.0) > 0.42
    for i in np.nonzero(rng.random(want.shape) < want * 0.8)[0]:
        n, v = choose(cfg["cliffs"])
        sc = rng.uniform(1.4, 3.8)
        yaw = math.atan2(-gyq[i], -gxq[i]) + rng.uniform(-0.6, 0.6)   # facing down the slope
        recs.append((aidx(n, v), QX[i], QY[i], qz[i] - 1.6 * sc, yaw, sc))
    nout = len(recs)
    upright = set()
    for (name, x, y, z, yaw, sc) in wall_rocks:
        upright.add(len(recs))
        recs.append((aidx(name, "a"), x, y, z, yaw, sc))
    log(f"scatter: {ntree} trees, {nfern} ferns, {nrock - ntree - nfern} rocks, {nout - nrock} outcrops, {len(recs) - nout} wall rocks, {len(assets)} models")
    # the forest's crowns on a 2 m grid (the ground under them, the far
    # forest seen past the trees' own draw distance): CANOPY1, as
    # remaster_env_place.py writes for host_env.m
    cc = 2.0
    cw, ch = int(near.w * near.cell / cc), int(near.h * near.cell / cc)
    can = np.zeros((ch, cw), np.float32)
    for r in recs[:ntree]:
        i, j = int((r[1] - near.x0) / cc), int((r[2] - near.y0) / cc)
        if 0 <= i < cw and 0 <= j < ch:
            can[j, i] += 1.0
    can = ndimage.gaussian_filter(can, 1.6) * 10.0
    with open(os.path.join(OUT, track + ".canopy"), "wb") as f:
        f.write(f"CANOPY1 {near.x0} {near.y0} {cc} {cw} {ch}\n".encode())
        f.write(np.clip(can * 255, 0, 255).astype(np.uint8).tobytes())
    # SCT1: magic, assets (count, then "name variant\0" each), records
    # (count; u16 asset, u16 pad, f32 x y z yaw height): the height is the
    # model's wanted height in metres (the loader scales by its own)
    pdir = os.path.join(P.MODELS, "placements")
    with open(os.path.join(pdir, track + ".scatter"), "wb") as f:
        f.write(b"SCT1"); f.write(struct.pack("<i", len(assets)))
        for n, v in assets:
            f.write(f"{n} {v}".encode() + b"\0")
        f.write(struct.pack("<i", len(recs)))
        a = np.array(recs, dtype=np.float64)
        rec = np.zeros(len(recs), dtype=[("a", "<u2"), ("pad", "<u2"), ("x", "<f4"), ("y", "<f4"), ("z", "<f4"), ("yaw", "<f4"), ("h", "<f4")])
        rec["pad"] = [1 if i in upright else 0 for i in range(len(recs))]   # 1: stands upright (a wall)
        rec["a"] = a[:, 0]; rec["x"] = a[:, 1]; rec["y"] = a[:, 2]; rec["z"] = a[:, 3]; rec["yaw"] = a[:, 4]; rec["h"] = a[:, 5]
        f.write(rec.tobytes())

def main(track):
    b = open(os.path.join(P.TRACKS, track + ".trk"), "rb").read()
    nat = set(NATURAL[track]); water = set(WATER.get(track, ()))
    cards = set(P.RULES.get(track, {}))
    log(track, "reading")
    pos, tex, inst, cmd, half = triangles(b)
    road_c, road_w = P.road_points(b)
    C = collision(b)
    n, area = normals(pos)
    nz = n[:, 2]
    # road textures: most of their area within the racing line's half width
    from scipy.spatial import cKDTree
    kd = cKDTree(road_c[:, :2])
    cc = pos.mean(1)
    dist, k = kd.query(cc[:, :2])
    inband = (dist <= road_w[k] + 0.5) & (np.abs(cc[:, 2] - road_c[k, 2]) < 3)
    roadtex = set()
    for t in np.unique(tex):
        m = tex == t
        if area[m & inband].sum() >= 0.5 * area[m].sum():
            roadtex.add(int(t))
    is_card = np.isin(tex, list(cards) + list(FRINGE.get(track, [])))
    # scenery far from the road (a lone card or sign out in the landscape)
    # does not shape the ground
    by_road = (dist - road_w[k]) < 120.0
    is_nat = np.isin(tex, list(nat)) & ~np.isin(tex, list(roadtex)) & ~is_card
    ground = is_nat & (nz >= GROUND_NZ)
    # painted backdrops off the collision mesh: hidden, not shaping the ground
    clo, chi = C.min(1), C.max(1)
    backdrop = np.zeros(len(pos), bool)
    for i in np.nonzero(np.isin(tex, list(BACKDROP.get(track, [])) + list(nat)) & ~ground & ~is_card)[0]:
        backdrop[i] = off_collision(pos[i], C, clo, chi, 1.0)
    log(f"{backdrop.sum()} backdrop triangles off the collision mesh hidden")
    # stone walls on the collision mesh: rebuilt in scanned rock
    stone = np.zeros(len(pos), bool)
    wall_rocks = []
    wall_feet = []     # (a point on the wall's base, along, toward the road, base height, length, depth)
    if STONEWALL.get(track):
        models = {a: rcm_points(a) for a in WALLROCKS}
        faces = {a: front_axis(v) for a, v in models.items() if len(v)}
        rngw = np.random.default_rng(11)
        cand = np.nonzero(np.isin(tex, list(STONEWALL[track]) + list(nat)) & (np.abs(nz) < 0.45) & ~ground & ~is_card & ~backdrop)[0]
        for i in cand:
            t = pos[i]
            if off_collision(t, C, clo, chi, 1.0):
                continue
            nrm = n[i].copy(); nh = nrm[:2] / max(np.linalg.norm(nrm[:2]), 1e-6)
            # toward the road: the face's horizontal normal, turned if it points away
            ctr = t.mean(0)
            dd, kk = kd.query(ctr[:2])
            if np.dot(road_c[kk, :2] - ctr[:2], nh) < 0:
                nh = -nh
            stone[i] = True
            # along the triangle's base, a rock every ~3.5 m, as tall as the wall there
            zlo, zhi = t[:, 2].min(), t[:, 2].max()
            along = np.array([-nh[1], nh[0]])
            proj = t[:, :2] @ along
            a0, a1 = proj.min(), proj.max()
            m = max(1, int((a1 - a0) / 3.5))
            wall_feet.append((ctr[:2] + along * (a0 - ctr[:2] @ along), along, nh, zlo, a1 - a0,
                              min(3.0, 0.6 * (zhi - zlo) + 1.0)))
            for q in range(m):
                sa = a0 + (q + 0.5) * (a1 - a0) / m
                # the plane's point at this position along it (at the base)
                base = ctr[:2] + along * (sa - ctr[:2] @ along)
                name = WALLROCKS[rngw.integers(len(WALLROCKS))]
                v = models[name]
                hmod = v[:, 2].max() - v[:, 2].min()
                sc = max(zhi - zlo, 1.0) * rngw.uniform(1.05, 1.35) / hmod
                yaw = math.atan2(nh[1], nh[0]) + faces[name] - 0.0 + rngw.uniform(-0.15, 0.15)
                # the model turned by yaw: its front-most point toward the road
                cy, sy = math.cos(yaw), math.sin(yaw)
                wx = (v[:, 0] * cy - v[:, 1] * sy) * sc; wy = (v[:, 0] * sy + v[:, 1] * cy) * sc
                front = (wx * nh[0] + wy * nh[1]).max()
                p = base - nh * (front + 0.05)
                wall_rocks.append((name, p[0], p[1], zlo - 0.4 * sc, yaw, sc))
        log(f"{stone.sum()} stone-wall triangles rebuilt with {len(wall_rocks)} rocks")
    is_water = np.isin(tex, list(water))
    is_road = np.isin(tex, list(roadtex))
    log(f"{len(pos)} triangles: {ground.sum()} natural ground ({area[ground].sum():.0f} m2) taken over, "
        f"{is_road.sum()} road, {is_water.sum()} water, {is_card.sum()} cards, {(~ground & ~is_card).sum()} kept")

    lo = np.minimum(pos.reshape(-1, 3).min(0), C.reshape(-1, 3).min(0))
    hi = np.maximum(pos.reshape(-1, 3).max(0), C.reshape(-1, 3).max(0))
    real = REAL.get(track)
    margin = real["near_margin"] if real else NEAR_MARGIN
    x0 = math.floor(lo[0] - margin); y0 = math.floor(lo[1] - margin)
    near = Grid(x0, y0, NEAR_CELL, int((hi[0] + margin - x0) / NEAR_CELL) + 1,
                int((hi[1] + margin - y0) / NEAR_CELL) + 1)
    log(f"near grid {near.w} x {near.h} at {near.x0},{near.y0}")
    fixed = np.full((near.h, near.w), np.nan)
    U = np.full((near.h, near.w), np.inf)
    cls = np.zeros((near.h, near.w), np.uint8)

    # natural ground: the field through it (where two levels overlap, the lower)
    for t in pos[ground]:
        r = tri_vertices(near, t)
        if r is None:
            continue
        jj, ii, z = r
        cur = fixed[jj, ii]
        fixed[jj, ii] = np.where(np.isnan(cur), z, np.minimum(cur, z))
        cls[jj, ii] = 1
    # what the game keeps drawing: the field under it, within a cell of it
    for kind, sel, under in (("road", is_road, UNDER_ROAD), ("water", is_water, LAKE_DEPTH),
                             # walls, posts and signs stand on the ground: only what faces up
                             # (decks, floors, kerbs, rock ledges) keeps the field under it
                             ("kept", ~ground & ~is_card & ~is_road & ~is_water & (nz >= GROUND_NZ) & by_road, UNDER_KEPT)):
        for t in pos[sel]:
            r = tri_near(near, t, NEAR_CELL * 0.75)
            if r is None:
                continue
            jj, ii, z = r
            U[jj, ii] = np.minimum(U[jj, ii], z - under)
            cls[jj, ii] = np.maximum(cls[jj, ii], 4 if kind == "water" else 3 if kind == "road" else 2)
    # the collision mesh where a car can get to
    log("reach")
    reach = P.Reach(b, road_c)
    R = np.array(sorted(reach.faces))
    cn, _ = normals(C)
    for f in R:
        t = C[f]
        r = tri_near(near, t, NEAR_CELL * 0.75)
        if r is not None:
            jj, ii, z = r
            U[jj, ii] = np.minimum(U[jj, ii], z + 0.01)
        if cn[f, 2] >= 0.5:
            r = tri_vertices(near, t)
            if r is not None:
                jj, ii, z = r
                free = np.isnan(fixed[jj, ii]) & (cls[jj, ii] == 0)
                fixed[jj[free], ii[free]] = z[free]
                cls[jj[free], ii[free]] = 1
    # past the edge of the mesh where a car can go off: the field falls away
    edges = {}
    key = lambda p: (round(p[0], 2), round(p[1], 2), round(p[2], 2))
    for f in R:
        t = C[f]
        for u, v in ((0, 1), (1, 2), (2, 0)):
            e = tuple(sorted((key(t[u]), key(t[v]))))
            edges.setdefault(e, []).append(f)
    allc = {}
    for f in range(len(C)):
        t = C[f]
        for u, v in ((0, 1), (1, 2), (2, 0)):
            allc.setdefault(tuple(sorted((key(t[u]), key(t[v])))), 0)
            allc[tuple(sorted((key(t[u]), key(t[v]))))] += 1
    nb = 0
    # the wedges go in their own map first: where the game's own ground lies
    # out there (a bank top, the far side of a ditch: the band below), the
    # land follows it, and a wedge dug between its vertices left a sawtooth
    # of spikes up to 16 m tall (measured on mountain, 2026-10-05)
    Wd = np.full(fixed.shape, np.inf)
    for e, fs in edges.items():
        if allc.get(e, 0) != 1 or cn[fs[0], 2] < 0.5:
            continue
        p0, p1 = np.array(e[0]), np.array(e[1])
        ctr = C[fs[0]].mean(0)
        mid = (p0 + p1) / 2
        out_dir = mid[:2] - ctr[:2]
        if np.linalg.norm(out_dir) < 1e-6:
            continue
        out_dir /= np.linalg.norm(out_dir)
        # a wedge beyond the edge: 1 m down per metre out, 0.5 m below at the edge
        L = np.linalg.norm(p1[:2] - p0[:2])
        steps = max(2, int(L / NEAR_CELL) + 1)
        for s in range(steps):
            q = p0 + (p1 - p0) * s / (steps - 1)
            for dd in np.arange(NEAR_CELL, 16.0, NEAR_CELL):
                x, y = q[:2] + out_dir * dd
                i, j = int(round((x - near.x0) / near.cell)), int(round((y - near.y0) / near.cell))
                if 0 <= i < near.w and 0 <= j < near.h and cls[j, i] == 0:
                    Wd[j, i] = min(Wd[j, i], q[2] - 0.5 - dd)
        nb += 1
    log(f"{nb} open edges a car can go off")
    # a stone wall rebuilt from rocks: the land stays at the wall's foot
    # through the rocks' depth, so the rocks are the wall.  Otherwise the
    # ground on the old wall's top (held in its band) stood a wall's height
    # over the road a cell away, and along a diagonal wall the 1 m grid made
    # that step a staircase: a row of green cones between the rocks
    WF = np.full(fixed.shape, np.inf)
    for (b0, along, nh, zlo, L, dep) in wall_feet:
        for sa in np.arange(-0.5, L + 0.5, near.cell * 0.5):
            for sd in np.arange(-0.2, dep, near.cell * 0.5):
                x, y = b0 + along * sa - nh * sd
                i, j = int(round((x - near.x0) / near.cell)), int(round((y - near.y0) / near.cell))
                if 0 <= i < near.w and 0 <= j < near.h:
                    WF[j, i] = min(WF[j, i], zlo + 0.3)
    # a held vertex is never above what is drawn beside it
    hm = np.isfinite(fixed)
    fixed[hm] = np.minimum(fixed[hm], U[hm])
    # held ground a car can be on stays held to the centimetre; the rest
    # (bank tops, slopes behind walls, the far side of a ditch) is a band the
    # land may move in: at most 30 cm above, 20 cm below
    Rlo = np.full(fixed.shape, np.inf); Rhi = np.full(fixed.shape, -np.inf)
    for f in R:
        r = tri_near(near, C[f], NEAR_CELL * 0.75)
        if r is None:
            continue
        jj, ii, z = r
        np.minimum.at(Rlo, (jj, ii), z - 0.6)
        np.maximum.at(Rhi, (jj, ii), z + 1.2)
    hard = hm & (fixed >= Rlo) & (fixed <= Rhi)
    soft = hm & ~hard
    # the stone walls' feet (above): free ground and the band under the
    # rocks go down to the foot; ground a car can reach keeps its place
    wf = np.isfinite(WF) & ~hard
    fixed[wf & hm] = np.nan; soft &= ~wf; hm &= ~wf
    U = np.minimum(U, np.where(wf, WF, np.inf))
    cls[wf & (cls == 1)] = 0
    log(f"stone walls: the land held at their feet over {wf.sum()} cells")
    near_band = ndimage.distance_transform_edt(~soft) * near.cell < 3.0
    U = np.minimum(U, np.where(near_band, np.inf, Wd))
    log(f"wedges: {np.isfinite(Wd).sum()} cells, {(np.isfinite(Wd) & near_band).sum()} left to the game's own ground beside them")
    Lb = np.full(fixed.shape, -np.inf)
    Lb[soft] = fixed[soft] - 0.2      # props (rails, trees, rocks) stand on the old ground
    U[soft] = np.minimum(U[soft], fixed[soft] + 0.3)
    target = fixed.copy()
    fixed[soft] = np.nan
    cls[soft] = 5
    log(f"held ground: {hard.sum()} vertices a car can reach (held), {soft.sum()} beyond (a band)")

    log("near membrane")
    M = membrane(np.where(soft, target, fixed), U, L=Lb)
    log("near thin plate")
    M = thinplate(M, fixed, U, Lb)

    if real:
        Hn, Hf, far, fflow, fdep, nflow, fcls = real_landscape(real, near, M, fixed, cls, U, Lb)
    else:
        fcls = None
        # ---- the far landscape -------------------------------------------------
        cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
        far = Grid(cx - FAR_HALF, cy - FAR_HALF, FAR_CELL, int(2 * FAR_HALF / FAR_CELL) + 1, int(2 * FAR_HALF / FAR_CELL) + 1)
        log(f"far grid {far.w} x {far.h}")
        Xf, Yf = np.meshgrid(far.x0 + np.arange(far.w) * far.cell, far.y0 + np.arange(far.h) * far.cell)
        # the track's ground, held, on the far grid (the near field's held cells)
        ours = (cls > 0)
        dn = ndimage.distance_transform_edt(~ours) * NEAR_CELL     # metres from the track's own surfaces
        ff = np.full((far.h, far.w), np.nan)
        k = int(FAR_CELL / NEAR_CELL)
        fi0 = int(round((near.x0 - far.x0) / far.cell)); fj0 = int(round((near.y0 - far.y0) / far.cell))
        sub = M[::k, ::k]; tsub = ndimage.maximum_filter(ours.astype(np.uint8), size=k)[::k, ::k] > 0
        hsub, wsub = sub.shape
        ff[fj0:fj0 + hsub, fi0:fi0 + wsub] = np.where(tsub, sub, np.nan)
        Mf = membrane(ff, np.full(ff.shape, np.inf), iters=(3000, 300, 200, 150, 100, 80, 60))
        tf = np.isfinite(ff)
        df = ndimage.distance_transform_edt(~tf) * FAR_CELL
        # mountains: grown by uplift against river erosion on a 16 m grid (the
        # track's ground held as the base level the rivers run to), uplift rising
        # with the distance from the track -- foothills close, the high range on
        # the horizon -- and varying along the ranges
        Xc, Yc, dfc, tfc, Mfc = Xf[::2, ::2], Yf[::2, ::2], df[::2, ::2], tf[::2, ::2], Mf[::2, ::2]
        band = fbm(Xc, Yc, 31, 4, 3800.0)
        rough = fbm(Xc, Yc, 32, 6, 700.0)
        near_up = np.clip((dfc - 120.0) / 900.0, 0, 1); near_up = near_up * near_up * (3 - 2 * near_up)
        far_up = np.clip((dfc - 2500.0) / 4500.0, 0, 1)
        Ufield = 1e-3 * (0.22 * near_up + 0.6 * far_up * (0.35 + 1.3 * band)) * (0.6 + 0.8 * rough)
        H0 = Mfc + (rough - 0.5) * 30.0 * near_up + (fbm(Xc, Yc, 33, 5, 180.0) - 0.5) * 12.0 * near_up
        log("far uplift")
        Hc, area = uplift(H0, Ufield, tfc, FAR_CELL * 2, 200)
        Hf = ndimage.zoom(Hc, 2, order=3)[:far.h, :far.w]
        Hf = np.pad(Hf, ((0, far.h - Hf.shape[0]), (0, far.w - Hf.shape[1])), mode="edge")
        # erosion of the detail on the 8 m grid: none on the track's own ground
        strength = np.clip((df - 40.0) / 300.0, 0, 1)
        bj, bi = np.meshgrid(np.arange(far.h), np.arange(far.w), indexing="ij")
        border = np.minimum.reduce([bi, bj, far.w - 1 - bi, far.h - 1 - bj])
        strength *= np.clip((border - 4) / 24.0, 0, 1)
        log("far erosion")
        Hf, fflow, fdep = erode(Hf, strength, FAR_CELL, 4_000_000, 7)
        Hf = np.where(tf, Mf, Hf)

        # ---- the near field: the membrane, and the land rising out of it ------
        Xn, Yn = np.meshgrid(near.x0 + np.arange(near.w) * near.cell, near.y0 + np.arange(near.h) * near.cell)
        fi = (Xn - far.x0) / far.cell; fj = (Yn - far.y0) / far.cell
        Hf_at = ndimage.map_coordinates(Hf, [fj, fi], order=1)
        Mf_at = ndimage.map_coordinates(Mf, [fj, fi], order=1)
        w = np.clip((dn - 25.0) / 300.0, 0, 1); w = w * w * (3 - 2 * w)
        detail = (fbm(Xn, Yn, 21, 6, 160.0) - 0.5) * 14.0 + (fbm(Xn, Yn, 22, 4, 28.0) - 0.5) * 2.2
        dk = np.clip((dn - 2.0) / 40.0, 0, 1)
        Hn = M + w * (Hf_at - Mf_at) + detail * dk
        held = np.isfinite(fixed)
        Hn = np.where(held, fixed, np.clip(Hn, Lb, U))
        # the near grid's edge meets the far field
        edge = np.minimum.reduce([np.arange(near.w)[None, :].repeat(near.h, 0), (near.w - 1 - np.arange(near.w))[None, :].repeat(near.h, 0),
                                  np.arange(near.h)[:, None].repeat(near.w, 1), (near.h - 1 - np.arange(near.h))[:, None].repeat(near.w, 1)])
        eb = np.clip(edge * NEAR_CELL / 80.0, 0, 1)
        Hn = np.where(held | np.isfinite(U), Hn, Hn * eb + Hf_at * (1 - eb))
        log("near erosion")
        nstrength = 0.5 * np.clip((dn - 6.0) / 60.0, 0, 1) * (~held) * (~np.isfinite(U)) * eb
        Hn2, nflow, ndep = erode(Hn, nstrength.astype(np.float64), NEAR_CELL, 350_000, 9)
        Hn = np.where(held, fixed, np.clip(Hn2, Lb, U))
        # the far field under the near one carries the near one's heights
        sub = Hn[::k, ::k]
        Hf[fj0:fj0 + sub.shape[0], fi0:fi0 + sub.shape[1]] = sub


    held = np.isfinite(fixed)
    # check: every held vertex where it was, nothing above what is drawn
    err = np.abs(Hn[held] - fixed[held]).max() if held.any() else 0
    over = (Hn - U)[np.isfinite(U)].max() if np.isfinite(U).any() else 0
    log(f"held vertices {held.sum()} max error {err:.4f} m; highest over a bound {over:.4f} m")

    os.makedirs(OUT, exist_ok=True)
    if fcls is None:
        fcls = np.zeros((far.h, far.w), np.uint8)
    road = (cls == 3)
    nroad = np.clip(ndimage.distance_transform_edt(~road) * near.cell * 10.0, 0, 255)
    write_ter(os.path.join(OUT, track + ".ter"), near, Hn, far, Hf, cls, fflow, fdep, nflow, fcls, nroad)
    # the natural ground the field takes over, hidden: per instance, the
    # command offset (+1 for the first triangle of a two-triangle command, +2
    # for the second, +0 for a one-triangle command)
    hides = {}
    # the painted conifer strips too (host_env.m's forest replaces them; a
    # strip its placements missed would stand alone in the new landscape)
    for i in np.nonzero(ground | backdrop | stone | is_card)[0]:
        w0 = be32(b, off(be32(b, off(be32(b, 0x60)) + inst[i] * 0x54 + 0x44)) + cmd[i])
        tag = 0 if (w0 >> 24) == 0xBF else 1 + half[i]
        hides.setdefault(int(inst[i]), []).append(int(cmd[i]) + tag)
    pdir = os.path.join(P.MODELS, "placements")
    with open(os.path.join(pdir, track + ".terrain"), "w") as f:
        f.write(f"track {track} {be32(b, 0x08)} {be32(b, 0x10)} {be32(b, 0x64)}\n")
        for i in sorted(hides):
            f.write(f"hide {i} " + " ".join(str(o) for o in sorted(set(hides[i]))) + "\n")
    scatter(track, near, Hn, cls, nflow, real, wall_rocks)
    log(f"wrote {track}.ter and {track}.terrain ({sum(len(v) for v in hides.values())} triangles hidden)")
    np.savez_compressed(os.path.join(ROOT, "build", "terrain", track + "_debug.npz"), Hn=Hn.astype(np.float32),
                        Hf=Hf.astype(np.float32), cls=cls, fixed=fixed.astype(np.float32))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mountain")
