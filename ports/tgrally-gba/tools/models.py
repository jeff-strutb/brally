"""models.py -- the game's models (drawing/modeldraw.c BrModelDraw: the front
end's icons, the title's logos) as they stand in a game-memory snapshot
(TGR_RAMDUMP), with their textures as the game's renderer decoded them in a
recording of the same screens (TGR_WORLDDUMP): every triangle of every part's
display list, its vertices (position, normal: the parts are lit), and the
morph animations (startup/boot.c BrAnimUpdate) that rewrite some of them.

    load(ram, addr) -> Model
    texture(model, tris) -> fills each triangle's texture from the recording

Byte order: a model is loaded data, so it is as the cartridge holds it (big
endian), its pointers already relocated (BrModelLoad)."""
import struct


class Snapshot:
    def __init__(self, path):
        d = open(path, 'rb').read()
        self.seg = struct.unpack_from('<16I', d, 0)
        self.ram = d[64:]

    def be32(self, a):
        return struct.unpack_from('>I', self.ram, a & 0x7FFFFF)[0]

    def le32(self, a):
        return struct.unpack_from('<I', self.ram, a & 0x7FFFFF)[0]

    def phys(self, a):
        return (self.seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF

    def bytes(self, a, n):
        return self.ram[a & 0x7FFFFF:(a & 0x7FFFFF) + n]


class Model:
    """parts: [(flags, pos (3 floats), [triangle]...)]; a triangle: three vertex
    addresses (physical); verts: address -> (x, y, z, s, t, nx, ny, nz); anims: [(out
    address, n, flags, start, end, [(time, [(x, y, z)], [(nx, ny, nz)])])]"""

    def __init__(self):
        self.parts, self.verts, self.anims = [], {}, []


def load(snap, addr):
    m = Model()
    count, alist = snap.be32(addr), snap.be32(addr + 4)
    for p in range(count):
        o = addr + 8 + p * 0x14
        dl = snap.be32(o)
        flags = struct.unpack('>H', snap.bytes(o + 4, 2))[0]
        pos = struct.unpack('>3f', snap.bytes(o + 8, 12))
        tris = walk(snap, dl, m.verts) if dl else []
        m.parts.append((flags, pos, tris))
    if alist:
        n = snap.be32(alist)
        for k in range(n):
            a = snap.be32(alist + 4 + 4 * k)
            nv, out, _, nkeys = (snap.be32(a + 4 * j) for j in range(4))
            flags, key = struct.unpack('>HH', snap.bytes(a + 16, 4))
            start, end, time = struct.unpack('>3f', snap.bytes(a + 20, 12))
            keys = []
            for j in range(nkeys):
                kp = snap.be32(a + 32 + 4 * j)
                t = struct.unpack('>f', snap.bytes(kp, 4))[0]
                pos = [struct.unpack('>3h', snap.bytes(kp + 4 + 6 * v, 6)) for v in range(nv)]
                nrm = [struct.unpack('3b', snap.bytes(kp + 4 + 6 * nv + 3 * v, 3)) for v in range(nv)]
                keys.append((t, pos, nrm))
            m.anims.append((snap.phys(out), nv, flags, start, end, keys))
    return m


def walk(snap, dl, verts):
    """a display list's triangles (F3DEX 1.21: G_VTX, G_TRI1, G_TRI2, G_DL, G_ENDDL)"""
    tris, vbuf, stack, pc = [], [None] * 32, [], snap.phys(dl)
    for _ in range(20000):
        w0, w1 = struct.unpack_from('>II', snap.ram, pc)
        pc += 8
        op = w0 >> 24
        if op == 0x04:
            cnt, v0, a = (w0 >> 10) & 0x3F, ((w0 >> 16) & 0xFF) // 2, snap.phys(w1)
            for k in range(cnt):
                if v0 + k < 32:
                    va = a + k * 16
                    x, y, z, _, s, t = struct.unpack_from('>3hH2h', snap.ram, va)
                    nx, ny, nz = struct.unpack_from('3b', snap.ram, va + 12)
                    verts[va] = (x, y, z, s, t, nx, ny, nz)
                    vbuf[v0 + k] = va
        elif op in (0xBF, 0xB1):
            idx = [(((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2)]
            if op == 0xB1:
                idx = [(((w0 >> 16) & 0xFF) // 2, ((w0 >> 8) & 0xFF) // 2, (w0 & 0xFF) // 2)] + idx
            for a, b, c in idx:
                vs = [vbuf[j] if j < 32 else None for j in (a, b, c)]
                if None not in vs:
                    tris.append(tuple(vs))
        elif op == 0x06:
            if ((w0 >> 16) & 0xFF) == 0:
                stack.append(pc)
            pc = snap.phys(w1)
        elif op == 0xB8:
            if not stack:
                break
            pc = stack.pop()
    return tris


def looks(wtris):
    """the recording's triangles by their vertices: (sorted addresses) -> (textured, key,
    st by address, tile)"""
    out = {}
    for fr, textured, wb, key, r, ci in wtris:
        k = tuple(sorted(r[30:33]))
        if k not in out or (textured and not out[k][0]):
            st = {r[30 + j]: (r[24 + 2 * j], r[25 + 2 * j]) for j in range(3)}
            out[k] = (textured, key, st, r[33:45], r[12:24])
    return out
