"""trackobjs.py -- the loaded track's objects, straight from a game-memory
snapshot (TGR_RAMDUMP): each object's world matrix and its display list
walked to triangles, every vertex placed in the world exactly as the game
places it (BrTrackDraw: an object flagged 0x2000 is the model scaled by
m[0][0] and moved to m[3]; any other is the model through m).

    objects(path) -> [(index, flags, [(addr triple, world pts[3], vertex rgba[3]), ...])]
"""
import struct

HDR = 0x25C00                      # D_80025C00, the loaded track's header (physical)


def objects(path):
    d = open(path, 'rb').read()
    seg = struct.unpack_from('<16I', d, 0)
    ram = d[64:]

    def u32(a):
        return struct.unpack_from('>I', ram, a & 0x7FFFFF)[0]

    def phys(a):
        return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF

    objs, n = u32(HDR + 0x60), u32(HDR + 0x64)
    out = []
    for i in range(n):
        o = (objs & 0x7FFFFF) + i * 0x54
        m = struct.unpack_from('>16f', ram, o)
        dl = u32(o + 0x44)
        flags = struct.unpack_from('>H', ram, o + 0x4C)[0]
        if not dl:
            continue
        tris, vbuf = [], [None] * 32
        stack, pc, guard = [], phys(dl), 0
        while guard < 20000:
            guard += 1
            w0, w1 = struct.unpack_from('>II', ram, pc)
            pc += 8
            op = w0 >> 24
            if op == 0x04:                                   # G_VTX
                cnt, v0, a = (w0 >> 10) & 0x3F, ((w0 >> 16) & 0xFF) // 2, phys(w1)
                for k in range(cnt):
                    if v0 + k < 32:
                        x, y, z = struct.unpack_from('>hhh', ram, a + k * 16)
                        rgba = ram[a + k * 16 + 12:a + k * 16 + 16]
                        vbuf[v0 + k] = (a + k * 16, (x, y, z), tuple(rgba))
            elif op in (0xBF, 0xB1):                         # G_TRI1, G_TRI2
                idx = [(((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2)]
                if op == 0xB1:
                    idx = [(((w0 >> 16) & 0xFF) // 2, ((w0 >> 8) & 0xFF) // 2, (w0 & 0xFF) // 2)] + idx
                for a, b, c in idx:
                    vs = [vbuf[j] if j < 32 else None for j in (a, b, c)]
                    if None in vs:
                        continue
                    pts = []
                    for _, (x, y, z), _ in vs:
                        if flags & 0x2000:
                            s = m[0]
                            pts.append((x * s + m[12], y * s + m[13], z * s + m[14]))
                        else:
                            pts.append((x * m[0] + y * m[4] + z * m[8] + m[12],
                                        x * m[1] + y * m[5] + z * m[9] + m[13],
                                        x * m[2] + y * m[6] + z * m[10] + m[14]))
                    tris.append((tuple(v[0] for v in vs), pts, [v[2] for v in vs]))
            elif op == 0x06:                                 # G_DL
                if ((w0 >> 16) & 0xFF) == 0:
                    stack.append(pc)
                pc = phys(w1)
            elif op == 0xB8:                                 # G_ENDDL
                if not stack:
                    break
                pc = stack.pop()
        out.append((i, flags, tris))
    return out
