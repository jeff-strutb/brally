"""wdump.py -- read a TGR_WORLDDUMP file (ports/tgrally/platform/gfx/rcp.c)."""
import struct


def read(path):
    """-> (cams {frame: (proj[16], vp[6])}, tris [(frame, textured, world bits, key, rec[30] + vertex
    addresses[3] + tile (s0, t0, sscale, tscale, clamp s/t, mirror s/t, mask s/t, clamp w/h), camera index)], tex {key: (w, h, rgba)}, camlist [(frame, proj, vp)]): cams holds
    a frame's main projection (the one most of its triangles use); each triangle names its own"""
    d = open(path, 'rb').read()
    i, n = 0, len(d)
    cams, tris, tex, camlist = {}, [], {}, []
    while i < n:
        k = d[i:i + 1]
        i += 1
        if k == b'C':
            fr = struct.unpack_from('<I', d, i)[0]
            proj = struct.unpack_from('<16f', d, i + 4)
            vp = struct.unpack_from('<6f', d, i + 68)
            camlist.append((fr, proj, vp))
            i += 4 + 64 + 24
        elif k == b'T':
            fr, textured, world, key = struct.unpack_from('<IiiQ', d, i)
            rec = (struct.unpack_from('<30f', d, i + 20) + struct.unpack_from('<3I', d, i + 140)
                   + struct.unpack_from('<4f4B4h', d, i + 152))
            tris.append((fr, textured, world, key, rec, len(camlist) - 1))
            i += 20 + 120 + 12 + 28
        elif k == b'X':
            key, w, h = struct.unpack_from('<Qii', d, i)
            i += 16
            tex[key] = (w, h, d[i:i + w * h * 4])
            i += w * h * 4
        else:
            raise ValueError('bad record at %d' % (i - 1))
    use = {}
    for t in tris:
        use[(t[0], t[5])] = use.get((t[0], t[5]), 0) + 1
    best = {}
    for (fr, ci), n in use.items():
        if n > best.get(fr, (0, -1))[0]:
            best[fr] = (n, ci)
    cams = {fr: camlist[ci][1:] for fr, (n, ci) in best.items()}
    return cams, tris, tex, camlist
