"""world.py -- the static world of a recorded race: every triangle the game
drew in RACE frames, placed in world space (a forced matrix's clip position
taken back through that frame's camera), kept when it was found in the same
place from at least MIN_FRAMES camera positions (cars, smoke, the sky and
the billboards that turn to the camera do not stay put); each triangle
carries the nearest and furthest it was drawn from (the game's detail
levels: a nearer and a further version of an object share its place)."""
import collections
import wdump

MIN_FRAMES = 3                # distinct camera places (CAMCELL units apart) it was seen from
CAMCELL = 4
PVSCELL = 32                  # the camera's grid cell for visibility (world units, absolute)
SNAP = 0.3                    # world units: reconstructions of one vertex closer than this are one


def inv4(m):
    a = [list(m[i * 4:i * 4 + 4]) + [1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for c in range(4):
        p = max(range(c, 4), key=lambda r: abs(a[r][c]))
        a[c], a[p] = a[p], a[c]
        d = a[c][c]
        a[c] = [x / d for x in a[c]]
        for r in range(4):
            if r != c:
                f = a[r][c]
                a[r] = [x - f * y for x, y in zip(a[r], a[c])]
    return [a[i][4 + j] for i in range(4) for j in range(4)]


def vmul(v, m):
    return [sum(v[k] * m[k * 4 + j] for k in range(4)) for j in range(4)]


def build(path, f0, f1):
    cams, tris, tex, camlist = wdump.read(path)
    cinv = {}
    invs, camcell, campos = {}, {}, {}
    dist = {}
    vis = collections.defaultdict(set)
    seen = collections.defaultdict(set)
    first = {}
    canon, grid = [], collections.defaultdict(list)

    def snap(p):
        """one id for every reconstruction of a vertex: the frames' camera
        inverses scatter it by a few hundredths of a unit"""
        g = tuple(int(c // SNAP) for c in p)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for i in grid[(g[0] + dx, g[1] + dy, g[2] + dz)]:
                        q = canon[i]
                        if abs(q[0] - p[0]) <= SNAP and abs(q[1] - p[1]) <= SNAP and abs(q[2] - p[2]) <= SNAP:
                            return i
        canon.append(tuple(p))
        grid[g].append(len(canon) - 1)
        return len(canon) - 1
    for fr, textured, world, key, r, ci in tris:
        if not f0 <= fr <= f1 or fr not in cams:
            continue
        if fr not in invs:
            invs[fr] = inv4(cams[fr][0])
            c = vmul([0.0, 0.0, 1.0, 0.0], invs[fr])   # the camera: clip x = y = w = 0
            campos[fr] = [v / c[3] for v in c[:3]] if abs(c[3]) > 1e-9 else [0.0, 0.0, 0.0]
            camcell[fr] = tuple(int(v // CAMCELL) for v in campos[fr])
        if ci not in cinv:
            cinv[ci] = inv4(camlist[ci][1])
        pts = []
        for q in range(3):
            x, y, z, w = r[q * 4:q * 4 + 4]
            if not (world >> q) & 1:
                if abs(w) < 1e-6:
                    break
                X, Y, Z, W = vmul([x, y, z, w], cinv[ci])
                if abs(W) < 1e-9:
                    break
                x, y, z = X / W, Y / W, Z / W
            pts.append((x, y, z))
        if len(pts) < 3:
            continue
        # wind it so it faces the camera that drew it: screen area < 0 (y down)
        P, vp = camlist[ci][1:]
        sc = []
        for x, y, z in pts:
            cx, cy, cz, cw = vmul([x, y, z, 1.0], P)
            sc.append((cx / cw * vp[0], -cy / cw * vp[1]) if abs(cw) > 1e-6 else (0.0, 0.0))
        (x0, y0), (x1, y1), (x2, y2) = sc
        if (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0) > 0:
            pts = [pts[0], pts[2], pts[1]]
            r = list(r)
            r[12:24] = r[12:16] + r[20:24] + r[16:20]
            r[24:30] = r[24:26] + r[28:30] + r[26:28]
        ids = [snap(p) for p in pts]
        if len(set(ids)) < 3:
            continue
        pts = [canon[i] for i in ids]
        k = tuple(sorted(ids))
        seen[k].add(camcell[fr])
        vis[k].add(tuple(int(v // PVSCELL) for v in campos[fr][:2]))
        cp = campos[fr]                               # the distance it was drawn at (on the ground)
        d = ((sum(p[0] for p in pts) / 3 - cp[0]) ** 2 + (sum(p[1] for p in pts) / 3 - cp[1]) ** 2) ** 0.5
        lo, hi = dist.get(k, (d, d))
        dist[k] = (min(lo, d), max(hi, d))
        if k not in first:
            first[k] = (pts, textured, key, r[12:24], r[24:30])
    keep = [first[k] + dist[k] + (vis[k],) for k in first if len(seen[k]) >= MIN_FRAMES]
    return cams, keep, tex
