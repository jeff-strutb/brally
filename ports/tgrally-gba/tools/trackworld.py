"""trackworld.py -- the whole loaded track, textures and all, straight from a game-memory
snapshot (TGR_RAMDUMP, or tools/simref.py's ram.bin): every object (trackobjs.py's walk)
with its display list's texture state followed as the RDP follows it -- the image
(G_SETTIMG), the tiles (G_SETTILE, G_SETTILESIZE), the loads into TMEM (G_LOADBLOCK,
G_LOADTILE, G_LOADTLUT, with the odd-line word swap) and G_TEXTURE's scale -- and each
textured triangle's texture decoded from TMEM as the port's RDP model decodes it
(ports/tgrally/platform/gfx/rcp.c: texel, tile_dims, fill_tile), its coordinates and
tile mapping as the port's world recording keeps them.

    build(snapshot) -> (keep, tex)
      keep: (world pts[3], textured, tex key, rgba[12] 0..1, st[6], 0, 0, set(), centre,
             tile (s0, t0, sscale, tscale, clamp s/t, mirror s/t, mask s/t, clamp w/h),
             (object, triangle, flags))    -- convert.py's tuple
      tex: key -> (w, h, rgba bytes)"""
import struct

HDR = 0x25C00                           # D_80025C00, the loaded track's header (physical)


class Snapshot:
    """a TGR_RAMDUMP (64-byte segment table, then RDRAM) or a raw 8 MB RDRAM (ram.bin)"""

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.native = len(d) != 0x800000             # a port's dump: the game's globals little-endian
        if len(d) == 0x800000:
            self.seg, self.ram = (0,) * 16, d
        else:
            self.seg, self.ram = struct.unpack_from('<16I', d, 0), d[64:]

    def u32(self, a):
        return struct.unpack_from('>I', self.ram, a & 0x7FFFFF)[0]

    def phys(self, a):
        return (self.seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF if a >> 24 < 16 else a & 0x7FFFFF


class Rdp:
    def __init__(self, snap):
        self.s = snap
        self.tmem = bytearray(4096)
        self.tile = [dict(fmt=0, siz=0, line=0, tmem=0, pal=0, cmt=0, maskt=0, shiftt=0, cms=0, masks=0, shifts=0,
                          uls=0, ult=0, lrs=0, lrt=0) for _ in range(8)]
        self.timg = self.timg_siz = self.timg_w = 0
        self.tex_on, self.tex_tile, self.ss, self.ts = 0, 0, 0xFFFF, 0xFFFF
        self.omh = 0
        self.cache = {}
        # the tiles the drawers set once and leave (modeldraw.c, cardraw.c): 7 the load tile,
        # 6 the palette's load tile (TMEM 0x1F0), 5 at 0x100
        self.tile[7].update(siz=2, tmem=0)
        self.tile[6].update(tmem=0x1F0)
        self.tile[5].update(tmem=0x100)

    def mem(self, a, n):
        a &= 0x7FFFFF
        return self.s.ram[a:a + n]

    def put64(self, addr, src, odd):
        for k in range(8):
            self.tmem[((addr * 8) + (k ^ (4 if odd else 0))) & 0xFFF] = src[k] if k < len(src) else 0

    def load_block(self, t, uls, ult, lrs, dxt):
        siz = self.timg_siz
        bpp = 0 if siz == 0 else 4 << siz
        texels = lrs - uls + 1
        nbytes = (texels + 1) // 2 if siz == 0 else texels * (bpp // 8)
        words = (nbytes + 7) // 8
        src = (self.timg & 0x7FFFFF) + ((ult * self.timg_w + uls) * (bpp // 8 if bpp else 1) // (1 if bpp else 2))
        base = self.tile[t]['tmem']
        for i in range(words):
            self.put64(base + i, self.mem(src + i * 8, 8), ((dxt * i) >> 11) & 1)

    def load_tile(self, t, uls, ult, lrs, lrt):
        siz = self.timg_siz
        bps = 0 if siz == 0 else (1 << siz) // 2
        x0, y0, x1, y1 = uls >> 2, ult >> 2, lrs >> 2, lrt >> 2
        line = self.tile[t]['line']
        for y in range(y0, y1 + 1):
            rowbytes = (x1 - x0 + 2) // 2 if siz == 0 else (x1 - x0 + 1) * bps
            src = (self.timg & 0x7FFFFF) + ((y * self.timg_w + x0) // 2 if siz == 0 else (y * self.timg_w + x0) * bps)
            for w in range((rowbytes + 7) // 8):
                self.put64(self.tile[t]['tmem'] + (y - y0) * line + w, self.mem(src + w * 8, 8), (y - y0) & 1)

    def load_tlut(self, t, lrs):
        n = (lrs >> 2) + 1
        base = self.tile[t]['tmem']
        for i in range(n):
            c = self.mem((self.timg & 0x7FFFFF) + i * 2, 2)
            for k in range(4):
                self.tmem[(base * 8 + i * 8 + k * 2) & 0xFFF] = c[0]
                self.tmem[(base * 8 + i * 8 + k * 2 + 1) & 0xFFF] = c[1]

    def texel(self, t, x, y):
        tl = self.tile[t]
        fmt, siz, line, base = tl['fmt'], tl['siz'], tl['line'] * 8, tl['tmem'] * 8
        tm = self.tmem
        swap = 4 if y & 1 else 0
        if siz == 3:
            swap = 8 if y & 1 else 0
        if siz == 0:
            v = tm[(base + y * line + x // 2 ^ swap) & 0xFFF]
            v = v & 0xF if x & 1 else v >> 4
        elif siz == 1:
            v = tm[(base + y * line + x ^ swap) & 0xFFF]
        elif siz == 2:
            a = (base + y * line + x * 2) ^ swap
            v = tm[a & 0xFFF] << 8 | tm[(a + 1) & 0xFFF]
        else:
            a = (base + y * line + x * 2) ^ swap
            return (tm[a & 0xFFF], tm[(a + 1) & 0xFFF], tm[(a + 0x800) & 0xFFF], tm[(a + 0x801) & 0xFFF])
        if fmt == 2 or (fmt == 0 and siz < 2):
            idx = (tl['pal'] << 4) | v if siz == 0 else v
            c = tm[(0x800 + idx * 8) & 0xFFF] << 8 | tm[(0x800 + idx * 8 + 1) & 0xFFF]
            if (self.omh >> 14 & 3) == 3:
                return (c >> 8, c >> 8, c >> 8, c & 0xFF)
            return ((c >> 11 & 31) * 255 // 31, (c >> 6 & 31) * 255 // 31, (c >> 1 & 31) * 255 // 31, 255 if c & 1 else 0)
        k = fmt << 4 | siz
        if k == 0x02:
            return ((v >> 11 & 31) * 255 // 31, (v >> 6 & 31) * 255 // 31, (v >> 1 & 31) * 255 // 31, 255 if v & 1 else 0)
        if k == 0x30:
            i = (v >> 1) * 255 // 7
            return (i, i, i, 255 if v & 1 else 0)
        if k == 0x31:
            return ((v >> 4) * 17,) * 3 + ((v & 15) * 17,)
        if k == 0x32:
            return (v >> 8,) * 3 + (v & 0xFF,)
        if k == 0x40:
            return (v * 17,) * 4
        return (v,) * 4

    def dims(self, t):
        tl = self.tile[t]
        w = (((tl['lrs'] - tl['uls']) & 0xFFF) >> 2) + 1
        h = (((tl['lrt'] - tl['ult']) & 0xFFF) >> 2) + 1
        if tl['masks'] and (1 << tl['masks']) < w:
            w = 1 << tl['masks']
        if tl['maskt'] and (1 << tl['maskt']) < h:
            h = 1 << tl['maskt']
        return max(1, min(1024, w)), max(1, min(1024, h))

    def texture(self, t):
        """-> (key, w, h, rgba): the tile's image, decoded once per TMEM content"""
        tl = self.tile[t]
        key = hash((tuple(sorted(tl.items())), self.omh & (3 << 14), bytes(self.tmem)))
        if key not in self.cache:
            w, h = self.dims(t)
            px = bytearray()
            for y in range(h):
                for x in range(w):
                    px += bytes(self.texel(t, x, y))
            self.cache[key] = (w, h, bytes(px))
        w, h, px = self.cache[key]
        return key, w, h, px

    def mapping(self, t):
        tl = self.tile[t]
        s0 = ((tl['uls'] - 4096) if tl['uls'] > tl['lrs'] else tl['uls']) / 4.0
        t0 = ((tl['ult'] - 4096) if tl['ult'] > tl['lrt'] else tl['ult']) / 4.0
        sscale = 1.0 / (1 << tl['shifts']) if tl['shifts'] <= 10 else float(1 << (16 - tl['shifts']))
        tscale = 1.0 / (1 << tl['shiftt']) if tl['shiftt'] <= 10 else float(1 << (16 - tl['shiftt']))
        return (s0, t0, sscale, tscale,
                int((tl['cms'] & 2) != 0 or tl['masks'] == 0), int((tl['cmt'] & 2) != 0 or tl['maskt'] == 0),
                int(tl['cms'] & 1 != 0), int(tl['cmt'] & 1 != 0),
                1 << tl['masks'] if tl['masks'] else 0, 1 << tl['maskt'] if tl['maskt'] else 0,
                (((tl['lrs'] - tl['uls']) & 0xFFF) >> 2) + 1, (((tl['lrt'] - tl['ult']) & 0xFFF) >> 2) + 1)


def lights(snap):
    """BrTrackDrawSetup's lights: the sun (D_8028AB58) from D_8031B338 normalised to 120
    (the light record's signed bytes), the ambient (D_8028AB5C), and the fixed greys an
    object flagged 0x400 takes for its ambient (D_8031B360); game globals are native
    (little-endian) in a port's snapshot, big-endian in the console's"""
    import math
    le = snap.native
    w = (lambda a: struct.unpack_from('<I' if le else '>I', snap.ram, a & 0x7FFFFF)[0])
    f = (lambda a: struct.unpack_from('<f' if le else '>f', snap.ram, a & 0x7FFFFF)[0])
    v = [f(0x8031B338 + 4 * k) for k in range(3)]
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    d = [int(c / n * 120.0) for c in v]
    dn = math.sqrt(sum(c * c for c in d)) or 1.0
    col = lambda x: (x >> 24 & 255, x >> 16 & 255, x >> 8 & 255)
    return ([c / dn for c in d], col(w(0x8028AB58)), col(w(0x8028AB5C)),
            [col(w(0x8031B360 + 4 * k)) for k in range(4)])


def build(snapshot):
    import math
    snap = Snapshot(snapshot)
    rdp = Rdp(snap)
    ldir, sun, amb, greys = lights(snap)
    objs, n = snap.u32(HDR + 0x60), snap.u32(HDR + 0x64)
    keep, tex = [], {}
    for i in range(n):
        o = (objs & 0x7FFFFF) + i * 0x54
        m = struct.unpack_from('>16f', snap.ram, o)
        dl = snap.u32(o + 0x44)
        flags = struct.unpack_from('>H', snap.ram, o + 0x4C)[0]
        if not dl:
            continue
        vbuf, stack, pc, guard, k = [None] * 32, [], snap.phys(dl), 0, 0
        rdp.tex_on, rdp.tex_tile, rdp.ss, rdp.ts = 1, 0, 0xFFFF, 0xFFFF      # BrTrackDraw's G_TEXTURE
        geom = 0xA0005 | (0 if flags & 4 else 0x2000)                       # lit; flag 4: both sides
        o_sun, o_amb = ((0, 0, 0), greys[flags & 3]) if flags & 0x400 else (sun, amb)
        pts_all = []
        tris = []
        while guard < 40000:
            guard += 1
            w0, w1 = struct.unpack_from('>II', snap.ram, pc)
            pc += 8
            op = w0 >> 24
            if op == 0x04:                                   # G_VTX
                cnt, v0, a = (w0 >> 10) & 0x3F, ((w0 >> 16) & 0xFF) // 2, snap.phys(w1)
                for j in range(cnt):
                    if v0 + j < 32:
                        x, y, z, _, s, t = struct.unpack_from('>3hH2h', snap.ram, a + j * 16)
                        rgba = snap.ram[a + j * 16 + 12:a + j * 16 + 16]
                        if flags & 0x2000:
                            sc = m[0]
                            p = (x * sc + m[12], y * sc + m[13], z * sc + m[14])
                        else:
                            p = (x * m[0] + y * m[4] + z * m[8] + m[12], x * m[1] + y * m[5] + z * m[9] + m[13],
                                 x * m[2] + y * m[6] + z * m[10] + m[14])
                        if geom & 0x20000:           # lit: the ambient and the sun on the vertex's normal
                            nx, ny, nz = struct.unpack_from('3b', rgba, 0)
                            if flags & 0x2000:
                                nw = (nx, ny, nz)
                            else:
                                nw = (nx * m[0] + ny * m[4] + nz * m[8], nx * m[1] + ny * m[5] + nz * m[9],
                                      nx * m[2] + ny * m[6] + nz * m[10])
                            ln = math.sqrt(sum(q * q for q in nw)) or 1.0
                            dot = max(0.0, sum(nw[q] / ln * ldir[q] for q in range(3)))
                            col = tuple(min(255.0, o_amb[q] + o_sun[q] * dot) / 255.0 for q in range(3)) + (rgba[3] / 255.0,)
                        else:
                            col = tuple(c / 255.0 for c in rgba)
                        vbuf[v0 + j] = (a + j * 16, p, col,
                                        (s * rdp.ss / 65536.0 / 32.0, t * rdp.ts / 65536.0 / 32.0))
            elif op in (0xBF, 0xB1):                         # G_TRI1, G_TRI2
                idx = [(((w1 >> 16) & 0xFF) // 2, ((w1 >> 8) & 0xFF) // 2, (w1 & 0xFF) // 2)]
                if op == 0xB1:
                    idx = [(((w0 >> 16) & 0xFF) // 2, ((w0 >> 8) & 0xFF) // 2, (w0 & 0xFF) // 2)] + idx
                for a, b, c in idx:
                    vs = [vbuf[j] if j < 32 else None for j in (a, b, c)]
                    if None in vs:
                        continue
                    textured = rdp.tex_on != 0
                    key, tile = 0, None
                    if textured:
                        key, w, h, px = rdp.texture(rdp.tex_tile)
                        tex[key] = (w, h, px)
                        tile = rdp.mapping(rdp.tex_tile)
                    tris.append(([v[1] for v in vs], textured, key, [c for v in vs for c in v[2]],
                                 [q for v in vs for q in v[3]], tile, (i, k, flags | (0 if geom & 0x3000 else 0x10000))))
                    k += 1
            elif op == 0x06:                                 # G_DL
                if ((w0 >> 16) & 0xFF) == 0:
                    stack.append(pc)
                pc = snap.phys(w1)
            elif op == 0xB8:                                 # G_ENDDL
                if not stack:
                    break
                pc = stack.pop()
            elif op == 0xB6:                                 # G_CLEARGEOMETRYMODE
                geom &= ~w1
            elif op == 0xB7:                                 # G_SETGEOMETRYMODE
                geom |= w1
            elif op == 0xBB:                                 # G_TEXTURE
                rdp.tex_on = w0 & 0xFF
                rdp.tex_tile = (w0 >> 8) & 7
                rdp.ss, rdp.ts = w1 >> 16, w1 & 0xFFFF
            elif op == 0xBA:                                 # G_SETOTHERMODE_H
                sh, ln = (w0 >> 8) & 0xFF, w0 & 0xFF
                mask = (0xFFFFFFFF if ln >= 32 else (1 << ln) - 1) << sh
                rdp.omh = (rdp.omh & ~mask) | w1
            elif op == 0xEF:                                 # G_RDPSETOTHERMODE
                rdp.omh = w0 & 0xFFFFFF
            elif op == 0xFD:                                 # G_SETTIMG
                rdp.timg, rdp.timg_siz, rdp.timg_w = snap.phys(w1), (w0 >> 19) & 3, (w0 & 0xFFF) + 1
            elif op == 0xF5:                                 # G_SETTILE
                t = rdp.tile[(w1 >> 24) & 7]
                t.update(fmt=(w0 >> 21) & 7, siz=(w0 >> 19) & 3, line=(w0 >> 9) & 0x1FF, tmem=w0 & 0x1FF,
                         pal=(w1 >> 20) & 15, cmt=(w1 >> 18) & 3, maskt=(w1 >> 14) & 15, shiftt=(w1 >> 10) & 15,
                         cms=(w1 >> 8) & 3, masks=(w1 >> 4) & 15, shifts=w1 & 15)
            elif op == 0xF2:                                 # G_SETTILESIZE
                t = rdp.tile[(w1 >> 24) & 7]
                t.update(uls=(w0 >> 12) & 0xFFF, ult=w0 & 0xFFF, lrs=(w1 >> 12) & 0xFFF, lrt=w1 & 0xFFF)
            elif op == 0xF3:                                 # G_LOADBLOCK
                rdp.load_block((w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF)
            elif op == 0xF4:                                 # G_LOADTILE
                rdp.load_tile((w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF)
            elif op == 0xF0:                                 # G_LOADTLUT
                rdp.load_tlut((w1 >> 24) & 7, (w1 >> 12) & 0xFFF)
        if not tris:
            continue
        xs = [p[0] for t in tris for p in t[0]]
        ys = [p[1] for t in tris for p in t[0]]
        centre = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2)
        for pts, textured, key, col, st, tile, src in tris:
            keep.append((pts, int(textured), key, col, st, 0.0, 2048.0, set(), centre, tile, src))
    return keep, tex


if __name__ == '__main__':
    import sys
    keep, tex = build(sys.argv[1])
    print('%d triangles (%d textured), %d textures, %d objects' % (
        len(keep), sum(1 for k in keep if k[1]), len(tex), len(set(k[10][0] for k in keep))))
