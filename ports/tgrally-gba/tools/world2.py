"""world2.py -- the race's world from the track's own objects (trackobjs.py:
exact world positions from a game-memory snapshot) with what a recording
of the race (wdump.py) adds: each triangle's colour as drawn (texture and
shading), the camera cells each object was drawn from (the game's
visibility lists) and the distances it was drawn at (its detail levels).

    build(ram, dump, f0, f1) -> cams, keep, tex
      keep: (pts, textured, tex key, rgba[12], st[6], dmin, dmax, camera cells, centre, tile, (object, triangle, flags))
"""
import collections
import math

import trackobjs
import wdump
import world

PVSCELL = world.PVSCELL


def build(ram, dump, f0, f1):
    objs = trackobjs.objects(ram)
    cams, tris, tex, camlist = wdump.read(dump)
    owner = {}
    for i, flags, ts in objs:
        for k, (addrs, pts, rgba) in enumerate(ts):
            owner[tuple(sorted(addrs))] = (i, k)
    look = {}                                         # (object, triangle) -> how it was drawn
    drawn = collections.defaultdict(set)              # frame -> objects
    for fr, textured, wb, key, r, ci in tris:
        if not f0 <= fr <= f1:
            continue
        o = owner.get(tuple(sorted(r[30:33])))
        if o is None:
            continue
        drawn[fr].add(o[0])
        if o not in look:
            # the vertex order of the record against the object's
            order = list(r[30:33])
            look[o] = (textured, key, r[12:24], r[24:30], order, r[33:45])
    centre = {i: (ts and None) for i, f, ts in objs}
    mats = {}
    for i, flags, ts in objs:
        xs = [p[0] for t in ts for p in t[1]]
        ys = [p[1] for t in ts for p in t[1]]
        centre[i] = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2) if xs else (0.0, 0.0)
    vis = collections.defaultdict(set)
    rng = {}
    for fr, os in drawn.items():
        if fr not in cams:
            continue
        inv = world.inv4(cams[fr][0])
        c = world.vmul([0.0, 0.0, 1.0, 0.0], inv)
        if abs(c[3]) < 1e-9:
            continue
        cp = (c[0] / c[3], c[1] / c[3])
        cell = (int(cp[0] // PVSCELL), int(cp[1] // PVSCELL))
        for o in os:
            vis[o].add(cell)
            d = math.hypot(centre[o][0] - cp[0], centre[o][1] - cp[1])
            lo, hi = rng.get(o, (d, d))
            rng[o] = (min(lo, d), max(hi, d))
    keep = []
    for i, flags, ts in objs:
        if i not in vis:
            continue                                  # never drawn in this race
        for k, (addrs, pts, rgba) in enumerate(ts):
            how = look.get((i, k))
            if how is None:
                textured, key, col, st, tile = 0, 0, [c / 255.0 for v in rgba for c in v], [0.0] * 6, None
            else:
                textured, key, col0, st0, order, tile = how
                # the recorded colours follow the record's vertex order: put them in the object's
                col, st = [], []
                for a in addrs:
                    j = order.index(a) if a in order else 0
                    col += list(col0[j * 4:j * 4 + 4])
                    st += list(st0[j * 2:j * 2 + 2])
            keep.append((pts, textured, key, col, st, rng[i][0], rng[i][1], vis[i], centre[i], tile, (i, k, flags)))
    return cams, keep, tex
