#!/usr/bin/env python3
"""remaster_dent.py -- how the Remastered car takes the original's dents.

    remaster_dent.py <car.rca> <pack dir>

The original dents a car by moving its model's own vertices in place
(BrRippleApply 0x1000C4E0: an impact picks one of eight zones and pushes the
vertices in that zone's box, the zone's running total capping it).  It walks,
for each detail level, the model record's lists at +0x8018 + i*4 (i = 0..8),
top level only, taking every G_VTX's vertices in order.  host_car.m walks the
level-0 lists the same way at run time, compares each vertex with where it
was before the first dent, and moves the Remastered model by the difference.

This writes, for body.rcm and glass.rcm, which original vertices each
Remastered vertex follows: its eight nearest among the body, cabin, detail
and glass lists (not the headlight beams), weighted by a Gaussian of the
distance (sigma 0.22 m) and normalised.  <mesh>_dent.bin: "RDN1", vertex
count, the source vertex count, then per vertex u16 index[8], f32 weight[8].

dent_rest.bin is the walk's vertices before any dent, in the game's own units
(the model's integers, 255 to the metre): "RDR1", count, then f32 xyz each.
The dents are measured against it, so the car can be switched to Remastered
after it has already been knocked.
"""
import os, struct, sys
import numpy as np
from scipy.spatial import cKDTree

B = 0x803C8000
SLOTS = [0x8018 + 4 * i for i in range(9)]
FOLLOW = {0x8024, 0x8028, 0x8030, 0x8038}          # detail, cabin, glass, body
K, SIGMA = 8, 0.22


def rest_vertices(rca):
    """the level-0 vertices in the dent walk's order, metres, and which to follow"""
    d = open(rca, 'rb').read()
    a2o = lambda a: 0x8000 + (a - B)
    be = lambda o: struct.unpack('>I', d[o:o + 4])[0]
    P, use = [], []
    for slot in SLOTS:
        a = be(slot)
        if not a:
            continue
        o = a2o(a)
        while True:
            w0, w1 = be(o), be(o + 4); op = w0 >> 24
            if op == 0x04:
                n = (w0 >> 10) & 0x3F; vo = a2o(w1)
                for i in range(n):
                    P.append(struct.unpack('>hhh', d[vo + 16 * i:vo + 16 * i + 6]))
                    use.append(slot in FOLLOW)
            if op == 0xB8:
                break
            o += 8
    return np.array(P, float) / 255.0, np.array(use)


def read_rcm_pos(path):
    d = open(path, 'rb').read()
    nv, ni = struct.unpack('<II', d[4:12])
    return np.frombuffer(d[12:12 + nv * 48], np.float32).reshape(nv, 12)[:, :3].astype(float)


def main():
    rca, pack = sys.argv[1], sys.argv[2]
    P, use = rest_vertices(rca)
    idx_all = np.nonzero(use)[0]
    with open(os.path.join(pack, 'dent_rest.bin'), 'wb') as f:
        f.write(b'RDR1' + struct.pack('<I', len(P)) + (P * 255.0).astype('<f4').tobytes())
    tree = cKDTree(P[idx_all])
    print(f'{len(P)} original vertices in the dent walk, {len(idx_all)} followed')
    for name in ('body', 'glass', 'body_lod1', 'glass_lod1', 'body_lod2', 'glass_lod2'):
        path = os.path.join(pack, f'{name}.rcm')
        if not os.path.exists(path):
            continue
        V = read_rcm_pos(path)
        dist, j = tree.query(V, k=K)
        w = np.exp(-(dist / SIGMA) ** 2 / 2)
        w /= w.sum(1, keepdims=True) + 1e-12
        idx = idx_all[j].astype(np.uint16)
        with open(os.path.join(pack, f'{name}_dent.bin'), 'wb') as f:
            f.write(b'RDN1' + struct.pack('<II', len(V), len(P)))
            rec = np.zeros(len(V), dtype=[('i', '<u2', K), ('w', '<f4', K)])
            rec['i'] = idx; rec['w'] = w
            f.write(rec.tobytes())
        print(f'{name}_dent.bin: {len(V)} vertices, median nearest source {np.median(dist[:, 0]):.3f} m')


if __name__ == '__main__':
    main()
