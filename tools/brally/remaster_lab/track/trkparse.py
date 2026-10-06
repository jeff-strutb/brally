"""Parse a retail .trk: decoded textures + every instance's triangles (local
space, with UVs, texture key and render mode)."""
import struct, hashlib
import numpy as np

BASE = 0x80025C00
FMTN = ['RGBA', 'YUV', 'CI', 'IA', 'I']

def be32(b, o): return struct.unpack_from('>I', b, o)[0]
def off(a): return a - BASE if a >= 0x80000000 else None

def c5551(v):
    return ((v >> 11 & 31) * 255 // 31, (v >> 6 & 31) * 255 // 31, (v >> 1 & 31) * 255 // 31, 255 * (v & 1))

def decode(b, st):
    fmt, siz, w, h, addr, line = st['fmt'], st['siz'], st['w'], st['h'], st['addr'], st['line']
    o = off(addr)
    if o is None or not (0 < w <= 512 and 0 < h <= 512): return None
    bpp = [4, 8, 16, 32][siz]
    stride = max(line * 8, (w * bpp + 7) // 8)
    raw = bytearray(b[o:o + stride * h])
    if len(raw) < stride * h: return None
    for y in range(1, h, 2):
        r0 = y * stride
        for q in range(r0, r0 + stride - 7, 8):
            raw[q:q + 8] = raw[q + 4:q + 8] + raw[q:q + 4]
    img = np.zeros((h, w, 4), np.uint8)
    for y in range(h):
        row = y * stride
        for x in range(w):
            if bpp == 4:
                byte = raw[row + x // 2]; v = (byte >> 4) if x % 2 == 0 else byte & 15
            elif bpp == 8: v = raw[row + x]
            elif bpp == 16: v = raw[row + 2 * x] << 8 | raw[row + 2 * x + 1]
            else: img[y, x] = tuple(raw[row + 4 * x:row + 4 * x + 4]); continue
            if fmt == 0: img[y, x] = c5551(v)
            elif fmt == 2:
                # the shipped TLUTs are all 0xFFFF (filled at run time): grey by index,
                # index 0 treated as the cut-out colour.
                g = v * 17 if bpp == 4 else v
                img[y, x] = (g, g, g, 0 if v == 0 else 255)
            elif fmt == 3:
                if bpp == 4: i = (v >> 1) * 255 // 7; a = 255 * (v & 1)
                elif bpp == 8: i = (v >> 4) * 17; a = (v & 15) * 17
                else: i = v >> 8; a = v & 255
                img[y, x] = (i, i, i, a)
            else:
                i = v * 17 if bpp == 4 else v
                img[y, x] = (i, i, i, i)
    return img

def parse(path):
    b = open(path, 'rb').read()
    ia = be32(b, 0x60); ci = be32(b, 0x64); root = be32(b, 0x50)
    recs = [(-1, root, np.eye(4))]
    for i in range(ci):
        r = off(ia) + i * 0x54
        M = np.array(struct.unpack_from('>16f', b, r)).reshape(4, 4)
        dl = be32(b, r + 0x44)
        if dl: recs.append((i, dl, M))
    texs = {}; insts = []
    for inst, dl, M in recs:
        o = off(dl); vbuf = [None] * 64
        st = dict(fmt=0, siz=2, w=0, h=0, addr=0, line=0, pal=0, uls=0, ult=0)
        timg = 0; key = None; rmode = 0; tris = []
        while o + 8 <= len(b):
            w0, w1 = be32(b, o), be32(b, o + 4); op = w0 >> 24
            if op == 0xFD: timg = w1
            elif op == 0xF5 and (w1 >> 24 & 7) == 0:
                st['fmt'] = w0 >> 21 & 7; st['siz'] = w0 >> 19 & 3; st['line'] = w0 >> 9 & 0x1FF
                st['pal'] = w1 >> 20 & 15; key = None
            elif op == 0xF3: st['addr'] = timg; key = None
            elif op == 0xF2 and (w1 >> 24 & 7) == 0:
                st['uls'] = (w0 >> 12 & 0xFFF) / 4; st['ult'] = (w0 & 0xFFF) / 4
                st['w'] = (w1 >> 12 & 0xFFF) // 4 - (w0 >> 12 & 0xFFF) // 4 + 1
                st['h'] = (w1 & 0xFFF) // 4 - (w0 & 0xFFF) // 4 + 1
                key = None
            elif op == 0xB9: rmode = w1
            elif op == 0x04:
                n = w0 >> 10 & 0x3F; v0 = (w0 >> 16 & 0xFF) // 2; vo = off(w1)
                if vo is not None:
                    for k in range(n):
                        x, y, z, _, s, t = struct.unpack_from('>hhhhhh', b, vo + 16 * k)
                        if v0 + k < 64: vbuf[v0 + k] = (x, y, z, s / 32.0, t / 32.0)
            elif op in (0xBF, 0xB1):
                if key is None:
                    key = '%08x_%s%d_%dx%d' % (st['addr'], FMTN[st['fmt']] if st['fmt'] < 5 else '?',
                                              [4, 8, 16, 32][st['siz']], st['w'], st['h'])
                    if key not in texs:
                        texs[key] = dict(img=decode(b, st), w=st['w'], h=st['h'])
                for w in ([w0, w1] if op == 0xB1 else [w1]):
                    idx = [(w >> 16 & 0xFF) // 2, (w >> 8 & 0xFF) // 2, (w & 0xFF) // 2]
                    p = [vbuf[i] for i in idx]
                    if any(q is None for q in p): continue
                    P = np.array([q[:3] for q in p], float)
                    UV = np.array([[(q[3] - st['uls']) / max(st['w'], 1), (q[4] - st['ult']) / max(st['h'], 1)] for q in p])
                    tris.append((P, UV, key, rmode))
            elif op == 0xB8: break
            o += 8
        insts.append(dict(i=inst, M=M, tris=tris))
    return texs, insts
