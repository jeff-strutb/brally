#!/usr/bin/env python3
"""convert.py RAM DUMP F0 F1 OUT.c -- the proof of concept's data: the track's
objects with what a recording of the race adds (world2.py), cut into CELL-unit grid cells, each with
its own shared vertex list (int16, 1/8 unit) and triangles (three local
indices and an RGB555 colour: the texture's average times the vertex
shading), and the race's camera, one fixed-point matrix a game frame.

The camera matrix maps a cell vertex (vx, vy, vz in 1/8 units, origin at
ORG) to X, Y, W in 1/256 units, X and Y already scaled to the 160x128
screen's pixels (so x = X / W + 80, y = Y / W + 64): three rows of four
Q12 coefficients (the transform shifts by 12), the fourth a Q8 constant."""
import math
import sys

import world
import world2
from preview import tex_avg

CELL = 32                     # world units a grid cell
MAXEDGE = 10000               # longer edges are split (in effect off: the runtime subdivision cuts deep ones)
PXUNIT = 224                  # the race view's pixels across a unit at a distance of one (cot 15 deg x 80 / (4/3))
MINPX = 1.5                   # the whole track: a triangle smaller on the screen than this is not drawn
TEXMAX = 64                   # textures larger than this are halved until they fit
LEVELS = 8                    # brightness levels a texture is stored at (the shading, baked)
SW, SH = 160, 128             # the GBA bitmap mode 5 screen


def rgb555(c):
    r, g, b = (max(0, min(31, int(v) >> 3)) for v in c)
    return r | g << 5 | b << 10


def pow2(n):
    p = 1
    while p < n:
        p *= 2
    return p


class Textures:
    """the GBA's textures: each N64 tile image (mirrored copies made real,
    padded to powers of two, halved down to TEXMAX) at each brightness level
    a triangle uses, as RGB555 with bit 15 set where the texel is clear, its
    rows TEXMAX texels apart (a narrower image repeated across)"""

    def __init__(self, tex):
        self.tex, self.base, self.out, self.index = tex, {}, [], {}

    def image(self, key, ms, mt):
        k = (key, ms, mt)
        if k not in self.base:
            w, h, px = self.tex[key]
            rows = [[tuple(px[(y * w + x) * 4:(y * w + x) * 4 + 4]) for x in range(w)] for y in range(h)]
            if ms:
                rows = [row + row[::-1] for row in rows]
            if mt:
                rows = rows + rows[::-1]
            W, H = pow2(len(rows[0])), pow2(len(rows))
            rows = [[row[x % len(row)] for x in range(W)] for row in rows]
            rows = [rows[y % len(rows)] for y in range(H)]
            while W > TEXMAX:
                rows = [row[::2] for row in rows]
                W //= 2
            while H > TEXMAX:
                rows = rows[::2]
                H //= 2
            self.base[k] = (rows, len(rows[0]), len(rows))
        return self.base[k]

    def get(self, key, ms, mt, level):
        """-> (index, scale of u, scale of v)"""
        rows, W, H = self.image(key, ms, mt)
        w0, h0 = self.tex[key][0] * (2 if ms else 1), self.tex[key][1] * (2 if mt else 1)
        su, sv = W / pow2(w0), H / pow2(h0)
        k = (key, ms, mt, level)
        if k not in self.index:
            f = level / LEVELS
            data = []
            for row in rows:                          # each row TEXMAX texels (repeated): a fixed stride
                for x in range(TEXMAX):
                    r_, g, b, a = row[x % W]
                    c = rgb555((r_ * f, g * f, b * f))
                    data.append(c | 0x8000 if a < 128 else c)
            self.index[k] = len(self.out)
            self.out.append((W.bit_length() - 1, H.bit_length() - 1, any(d & 0x8000 for d in data), data))
        return self.index[k], su, sv


def split(pts):
    """the triangle cut at the midpoint of its longest edge until no edge is
    longer than MAXEDGE (a shared edge is cut the same way from both sides)"""
    todo, out = [list(pts)], []
    while todo:
        t = todo.pop()
        e = [sum((t[(i + 1) % 3][k] - t[i][k]) ** 2 for k in range(3)) for i in range(3)]   # x y z only
        i = max(range(3), key=lambda j: e[j])
        if e[i] <= MAXEDGE * MAXEDGE or len(out) > 64:
            out.append(t)
            continue
        a, b, c = t[i], t[(i + 1) % 3], t[(i + 2) % 3]
        m = tuple((a[k] + b[k]) / 2 for k in range(len(a)))
        todo += [[a, m, c], [m, b, c]]
    return out


def main():
    ram, dump, f0, f1, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
    whole = dump.endswith('-')
    if whole:                             # the whole track from the snapshot alone (no recorded race)
        import trackworld
        cams = {}
        keep, tex = trackworld.build(ram)
        two = []                          # a triangle drawn both ways round: its other face too
        for k in keep:
            if k[10][2] & 0x10000:
                pts, tx, key, col, st, dlo, dhi, vis, centre, tile, src = k
                two.append(([pts[0], pts[2], pts[1]], tx, key, col[0:4] + col[8:12] + col[4:8],
                            st[0:2] + st[4:6] + st[2:4], dlo, dhi, vis, centre, tile, src))
        keep = keep + two
    else:
        cams, keep, tex = world2.build(ram, dump, f0, f1)
    xs = [p[0] for t in keep for p in t[0]]
    ys = [p[1] for t in keep for p in t[0]]
    zs = [p[2] for t in keep for p in t[0]]
    org = (math.floor(min(xs)), math.floor(min(ys)), math.floor(min(zs)))
    gw = int((max(xs) - org[0]) // CELL) + 1
    gh = int((max(ys) - org[1]) // CELL) + 1
    cells = {}
    avg = {}
    texs = Textures(tex)
    for pts, tx, key, col, st, dlo, dhi, vis, centre, tile, src in keep:
        cxw = sum(p[0] for p in pts) / 3
        cyw = sum(p[1] for p in pts) / 3
        c = (int((cxw - org[0]) // CELL), int((cyw - org[1]) // CELL))
        a = avg.setdefault(key, tex_avg(tex, key)) if tx and key in tex else (255, 255, 255)
        rgb = [a[i] * (col[i] + col[4 + i] + col[8 + i]) / 3 for i in range(3)]
        cen = (int(centre[0] - org[0]), int(centre[1] - org[1]))   # the object's: its detail range is measured there
        dl, dh = max(0, int(dlo) - 8) // 8, min(255, (int(dhi) + 8 + 7) // 8)
        if whole:                                 # no recorded draw distances: past where it is too small
            size = max(math.dist(pts[i], pts[(i + 1) % 3]) for i in range(3))
            dh = min(dh, int(size * PXUNIT / MINPX / 8) + 2)
        ti, uv = 0xFFFF, [(0.0, 0.0)] * 3
        if tx and key in tex and tile:
            s0, t0, ss, ts, cs, ct, ms, mt = tile[:8]
            shade = sum(col[i * 4 + k] for i in range(3) for k in range(3)) / 9
            level = max(1, min(LEVELS, int(round(shade * LEVELS))))
            ti, su, sv = texs.get(key, ms, mt, level)
            uv = [((st[j * 2] * ss - s0) * su * 16, (st[j * 2 + 1] * ts - t0) * sv * 16) for j in range(3)]
            W, H = 16 << texs.out[ti][0], 16 << texs.out[ti][1]   # one period, Q4
            du = math.floor(min(u for u, v in uv) / W) * W        # small numbers: same texels
            dv = math.floor(min(v for u, v in uv) / H) * H
            uv = [(u - du, v - dv) for u, v in uv]
        for piece in split([tuple(p) + tuple(t) for p, t in zip(pts, uv)]):
            q = [tuple(int(round((p[i] - org[i]) * 8)) for i in range(3)) for p in piece]
            puv = [max(-32768, min(32767, int(round(p[k])))) for p in piece for k in (3, 4)]
            cells.setdefault(c, []).append((q, rgb555(rgb), cen, dl, dh, vis, ti, puv, src))   # vis: absolute camera cells
    verts, tris, table, srcs = [], [], [], []
    maxv = 0
    for cy in range(gh):
        for cx in range(gw):
            ts = cells.get((cx, cy), [])
            vmap = {}
            vstart, tstart = len(verts), len(tris)
            for q, col, cen, dlo, dhi, cc, ti, puv, src in ts:
                idx = []
                for v in q:
                    if v not in vmap:
                        vmap[v] = len(vmap)
                        verts.append(v)
                    idx.append(vmap[v])
                # its plane (vertex units): the camera in front of it (n . cam > d) is what the screen's
                # winding says, the projection keeping one handedness all race (det > 0)
                e1 = [q[1][k] - q[0][k] for k in range(3)]
                e2 = [q[2][k] - q[0][k] for k in range(3)]
                nrm = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
                ln = math.sqrt(sum(c * c for c in nrm)) or 1.0
                nq = [int(round(c / ln * 4096)) for c in nrm]
                dq = sum(nq[k] * q[0][k] for k in range(3))
                tris.append((idx[0], idx[1], idx[2], col, cen[0], cen[1], dlo, dhi, ti) + tuple(puv) + tuple(nq) + (dq,))
                srcs.append(src)
            maxv = max(maxv, len(vmap))
            lo = min((t[3] for t in ts), default=0)
            hi = max((t[4] for t in ts), default=0)
            if vmap:                                       # its bounding sphere (vertex units)
                cv = list(vmap)
                cc = [(min(v[k] for v in cv) + max(v[k] for v in cv)) // 2 for k in range(3)]
                rr = int(math.ceil(max(math.sqrt(sum((v[k] - cc[k]) ** 2 for k in range(3))) for v in cv))) + 2
            else:
                cc, rr = [0, 0, 0], 0
            table.append((vstart, len(vmap), lo, hi, tstart, len(ts)) + tuple(cc) + (rr,))
    # visibility: for each camera cell, the cells and their triangles the game drew from there
    pvs_of = {}
    for (cx, cy), ts in cells.items():
        for j, t in enumerate(ts):
            for cc in t[5]:
                pvs_of.setdefault(cc, {}).setdefault(cy * gw + cx, []).append(j)
    pvs, pvs_at = [], []
    if not pvs_of:
        pvs_of = {(0, 0): {}}
    px0 = min(c[0] for c in pvs_of)
    py0 = min(c[1] for c in pvs_of)
    pw = max(c[0] for c in pvs_of) - px0 + 1
    ph = max(c[1] for c in pvs_of) - py0 + 1
    for cy in range(py0, py0 + ph):
        for cx in range(px0, px0 + pw):
            lists = pvs_of.get((cx, cy))
            if not lists:
                pvs_at.append(0xFFFFFFFF)
                continue
            pvs_at.append(len(pvs))
            for wc in sorted(lists):
                pvs += [wc, len(lists[wc])] + lists[wc]
            pvs.append(0xFFFF)
    # the camera: one a game frame (the game draws every other retrace)
    frames = []
    for fr in sorted(cams):
        if not f0 <= fr <= f1:
            continue
        P, vp = cams[fr]
        sx, sy = vp[0] * SW / 320.0, vp[1] * SH / 240.0
        rows = []
        for col, scale in ((0, sx), (1, -sy), (3, 1.0)):
            coef = [int(round(P[i * 4 + col] * scale / 8 * 16 * 65536)) for i in range(3)]
            const = sum(org[i] * P[i * 4 + col] for i in range(3)) + P[12 + col]
            rows.append(coef + [int(round(const * scale * 256))])
        # where it is and which way it looks, for choosing the cells (world units from ORG)
        A = [[P[r * 4 + c] for r in range(3)] for c in (0, 1, 3)]
        b = [-P[12 + c] for c in (0, 1, 3)]
        M = [A[i][:] + [b[i]] for i in range(3)]
        for c in range(3):
            p = max(range(c, 3), key=lambda r: abs(M[r][c]))
            M[c], M[p] = M[p], M[c]
            for r in range(3):
                if r != c:
                    k = M[r][c] / M[c][c]
                    M[r] = [M[r][j] - k * M[c][j] for j in range(4)]
        pos = [M[i][3] / M[i][i] - org[i] for i in range(3)]
        fw = [P[i * 4 + 3] for i in range(3)]
        n = math.hypot(fw[0], fw[1]) or 1.0
        cam = [int(round(pos[k] * 8)) for k in range(3)]         # vertex units
        # the screen's four edges as planes (vertex units, normals Q12): a point is on the
        # screen's side when X + 80 W, 80 W - X, Y + 64 W, 64 W - Y are all >= 0 (X, Y, W the
        # transform's rows, as main.c computes them)
        planes = []
        for a, b in ((0, 80), (0, -80), (1, 64), (1, -64)):
            sg = 1 if b > 0 else -1
            g = [(sg * rows[a][k] + abs(b) * rows[2][k]) / 4096.0 for k in range(3)]
            h = sg * rows[a][3] + abs(b) * rows[2][3]
            gl = math.sqrt(sum(c * c for c in g)) or 1.0
            planes.append([int(round(c / gl * 4096)) for c in g] + [int(math.floor(h / gl))])
        frames.append((rows, (int(pos[0]), int(pos[1])), (int(fw[0] / n * 256), int(fw[1] / n * 256)), cam, planes))
    with open(out.rsplit('.', 1)[0] + '.src', 'w') as f:      # g_tris index -> object, its triangle, flags
        for o, k, fl in srcs:
            f.write('%d %d %d\n' % (o, k, fl))
    with open(out, 'w') as f:
        f.write('/* generated by ports/tgrally-gba/tools/convert.py from %s, frames %d..%d */\n' % (dump, f0, f1))
        f.write('#include "world.h"\n')
        f.write('const int g_grid_w = %d, g_grid_h = %d, g_nframes = %d, g_maxv = %d;\n' % (gw, gh, len(frames), maxv))
        f.write('const Cell g_cells[%d] = {\n' % len(table))
        f.write(',\n'.join('{%d,%d,%d,%d,%d,%d,{%d,%d,%d},%d}' % t for t in table) + '};\n')
        f.write('const V3 g_verts[%d] = {\n' % len(verts))
        f.write(',\n'.join('{%d,%d,%d,0}' % v for v in verts) + '};\n')
        f.write('const Tri g_tris[%d] = {\n' % len(tris))
        f.write(',\n'.join('{%d,%d,%d,%d,%d,%d,%d,%d,%d,{%d,%d,%d,%d,%d,%d},{%d,%d,%d},0,%d}' % t for t in tris) + '};\n')
        for i, (wb, hb, alpha, data) in enumerate(texs.out):
            f.write('static const uint16_t tex%d[%d] = {%s};\n' % (i, len(data), ','.join('%d' % d for d in data)))
        f.write('const Tex g_tex[%d] = {\n' % max(1, len(texs.out)))
        f.write(',\n'.join('{tex%d,%d,%d,%d,%d,%d}' % (i, ((1 << wb) - 1) << 1, ((1 << hb) - 1) << 7, wb, hb, alpha)
                            for i, (wb, hb, alpha, data) in enumerate(texs.out)) + '};\n')
        f.write('const int g_org[3] = {%d, %d, %d}, g_pvs_x0 = %d, g_pvs_y0 = %d, g_pvs_w = %d, g_pvs_h = %d, g_pvs_cell = %d;\n'
                % (org[0], org[1], org[2], px0, py0, pw, ph, world.PVSCELL))
        f.write('const uint32_t g_pvs_at[%d] = {%s};\n' % (len(pvs_at), ','.join('%uu' % v for v in pvs_at)))
        f.write('const uint16_t g_pvs[%d] = {%s};\n' % (max(1, len(pvs)), ','.join('%d' % v for v in pvs) or '0xFFFF'))
        f.write('const Frame g_frames[%d] = {\n' % max(1, len(frames)))
        if not frames:
            f.write('{{0}}')
        f.write(',\n'.join('{{%s},{%d,%d},{%d,%d},{%d,%d,%d},{%s}}' % (','.join('%d' % x for r in rows for x in r), p[0], p[1], d[0], d[1],
                                                                  cam[0], cam[1], cam[2], ','.join('{%d,%d,%d,%d}' % tuple(pl) for pl in planes))
                           for rows, p, d, cam, planes in frames) + '};\n')
    if maxv > 128:
        sys.exit('convert.py: a cell has %d vertices: gba/main.c keeps 128 (s_pv)' % maxv)
    print('%s: %d tris, %d verts, grid %dx%d of %d units, %d frames, max %d verts a cell, %d camera cells, %d visibility words' %
          (out, len(tris), len(verts), gw, gh, CELL, len(frames), maxv, len(pvs_of), len(pvs)))
    print('textures: %d (%d images at up to %d levels), %d KB' % (len(texs.out), len(texs.base), LEVELS,
                                                                 sum(len(t[3]) * 2 for t in texs.out) // 1024))


if __name__ == '__main__':
    main()
