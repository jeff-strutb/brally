"""Per-texture census of every .trk: decode each texture, attribute every
triangle to the texture bound when it was drawn, and measure how it is used."""
import struct, os, collections, json, math, hashlib
import numpy as np
from PIL import Image

BASE = 0x80025C00
D = 'build/brally/wasm32/app/extract/disc/tracks/'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tex')
os.makedirs(OUT, exist_ok=True)

def be32(b, o): return struct.unpack_from('>I', b, o)[0]
def off(a): return a - BASE if a >= 0x80000000 else None

def c5551(v):
    r = (v >> 11) & 31; g = (v >> 6) & 31; bl = (v >> 1) & 31; a = v & 1
    return (r * 255 // 31, g * 255 // 31, bl * 255 // 31, 255 * a)

def decode(b, st):
    fmt, siz, w, h, addr, line = st['fmt'], st['siz'], st['w'], st['h'], st['addr'], st['line']
    o = off(addr)
    if o is None or w <= 0 or h <= 0 or w > 512 or h > 512: return None
    img = np.zeros((h, w, 4), np.uint8)
    # row stride: line is in 64-bit words
    bpp = [4, 8, 16, 32][siz]
    stride = line * 8 if line else (w * bpp + 7) // 8
    stride = max(stride, (w * bpp + 7) // 8)
    tl = st['tlut']
    raw = bytearray(b[o:o + stride * h])
    if len(raw) < stride * h: return None
    # LOADBLOCK with dxt 0: the RAM image carries the TMEM odd-row swap
    # (32-bit halves of every 64-bit word exchanged on odd rows).
    for y in range(1, h, 2):
        r0 = y * stride
        for q in range(r0, r0 + stride - 7, 8):
            raw[q:q + 8] = raw[q + 4:q + 8] + raw[q:q + 4]
    b = bytes(raw); o = 0
    for y in range(h):
        row = o + y * stride
        for x in range(w):
            # N64 TMEM swaps odd rows' 32-bit words; loaded via LOADBLOCK with dxt the
            # source in RAM is linear, so read linearly.
            if bpp == 4:
                if row + x // 2 >= len(b): return None
                byte = b[row + x // 2]; v = (byte >> 4) if x % 2 == 0 else byte & 15
            elif bpp == 8:
                if row + x >= len(b): return None
                v = b[row + x]
            elif bpp == 16:
                if row + 2 * x + 2 > len(b): return None
                v = struct.unpack_from('>H', b, row + 2 * x)[0]
            else:
                if row + 4 * x + 4 > len(b): return None
                img[y, x] = tuple(b[row + 4 * x:row + 4 * x + 4]); continue
            if fmt == 0:  # RGBA
                img[y, x] = c5551(v)
            elif fmt == 2:  # CI
                # palettes are 0xFFFF in the shipped file (filled at run time):
                # show the index as grey so the silhouette is readable.
                g = v * 17 if bpp == 4 else v
                img[y, x] = (g, g, g, 255)
            elif fmt == 3:  # IA
                if bpp == 4: i = (v >> 1) * 255 // 7; a = 255 * (v & 1)
                elif bpp == 8: i = (v >> 4) * 17; a = (v & 15) * 17
                else: i = v >> 8; a = v & 255
                img[y, x] = (i, i, i, a)
            else:  # I
                i = v * 17 if bpp == 4 else v
                img[y, x] = (i, i, i, i)
    return img

def normal(p):
    a, b_, c = [np.array(v, float) for v in p]
    n = np.cross(b_ - a, c - a); l = np.linalg.norm(n)
    return (n / l if l else n), l / 2

tracks = {}
for fn in sorted(os.listdir(D)):
    if not fn.endswith('.trk'): continue
    name = fn[:-4]
    b = open(D + fn, 'rb').read()
    ia = be32(b, 0x60); ci = be32(b, 0x64); root = be32(b, 0x50)
    lists = [(-1, root, np.eye(4))]
    for i in range(ci):
        r = off(ia) + i * 0x54
        m = np.array(struct.unpack_from('>16f', b, r)).reshape(4, 4)
        dl = be32(b, r + 0x44)
        if dl: lists.append((i, dl, m))
    texs = {}
    tlut = [0] * 256
    for inst, dl, M in lists:
        o = off(dl)
        vbuf = [None] * 64
        st = dict(fmt=0, siz=2, w=0, h=0, addr=0, line=0, pal=0, tlut=tlut)
        timg = 0; tlut_tmem = {}; key = None; rmode = 0; combine = 0
        while o + 8 <= len(b):
            w0, w1 = be32(b, o), be32(b, o + 4); op = w0 >> 24
            if op == 0xFD: timg = w1
            elif op == 0xF5:
                tile = (w1 >> 24) & 7
                if tile in (6, 7): tlut_tmem[tile] = w0 & 0x1FF
                elif tile == 0:
                    st['fmt'] = (w0 >> 21) & 7; st['siz'] = (w0 >> 19) & 3
                    st['line'] = (w0 >> 9) & 0x1FF; st['pal'] = (w1 >> 20) & 15
            elif op == 0xF0:
                cnt = ((w1 >> 14) & 0x3FF) + 1; lt = (w1 >> 24) & 7
                base = (tlut_tmem[lt] - 0x100) if lt in tlut_tmem else 256 - cnt
                t = off(timg)
                for k in range(cnt):
                    if 0 <= base + k < 256 and t is not None and t + 2 * k + 2 <= len(b):
                        tlut[base + k] = struct.unpack_from('>H', b, t + 2 * k)[0]
            elif op == 0xF3: st['addr'] = timg
            elif op == 0xF2:
                tile = (w1 >> 24) & 7
                if tile == 0:
                    st['w'] = ((w1 >> 12) & 0xFFF) // 4 - ((w0 >> 12) & 0xFFF) // 4 + 1
                    st['h'] = (w1 & 0xFFF) // 4 - (w0 & 0xFFF) // 4 + 1
                    key = None
            elif op == 0xB9: rmode = w1
            elif op == 0xFC: combine = (w0, w1)
            elif op == 0x04:
                n = (w0 >> 10) & 0x3F; v0 = ((w0 >> 16) & 0xFF) // 2; vo = off(w1)
                if vo is not None:
                    for k in range(n):
                        x, y, z = struct.unpack_from('>hhh', b, vo + 16 * k)
                        p = np.array([x, y, z, 1.0]) @ M
                        if v0 + k < 64: vbuf[v0 + k] = p[:3]
            elif op in (0xBF, 0xB1):
                if key is None:
                    tlh = hashlib.md5(struct.pack('>256H', *tlut)).hexdigest()[:6] if st['fmt'] == 2 else ''
                    key = '%08x_%d%d_%dx%d_p%d%s' % (st['addr'], st['fmt'], st['siz'], st['w'], st['h'], st['pal'], tlh)
                    if key not in texs:
                        img = decode(b, st)
                        png = None
                        if img is not None:
                            png = '%s_%s.png' % (name, key)
                            Image.fromarray(img, 'RGBA').save(os.path.join(OUT, png))
                        texs[key] = dict(key=key, png=png, w=st['w'], h=st['h'],
                                         fmt=['RGBA', 'YUV', 'CI', 'IA', 'I'][st['fmt']] + str([4, 8, 16, 32][st['siz']]),
                                         alpha=(float((img[:, :, 3] < 128).mean()) if img is not None else None),
                                         tris=0, area=0.0, up=0.0, down=0.0, vert=0.0, insts=set(), rmodes=set(),
                                         pts=[])
                T = texs[key]
                ws = [w0, w1] if op == 0xB1 else [w1]
                for w in ws:
                    idx = [(w >> 16 & 0xFF) // 2, (w >> 8 & 0xFF) // 2, (w & 0xFF) // 2]
                    p = [vbuf[i] for i in idx]
                    if any(q is None for q in p): continue
                    nrm, ar = normal(p)
                    T['tris'] += 1; T['area'] += ar; T['insts'].add(inst); T['rmodes'].add(rmode)
                    # which axis is up? decided below per track; store normal-weighted
                    T.setdefault('nsum', np.zeros(3)); T['nsum'] += np.abs(nrm) * ar
                    if len(T['pts']) < 4000: T['pts'].extend([q.tolist() for q in p])
            elif op == 0xB8: break
            o += 8
    tracks[name] = texs
    print(name, len(texs), 'textures')

# write JSON (sets -> counts)
js = {}
for name, texs in tracks.items():
    js[name] = []
    for k, T in texs.items():
        pts = np.array(T['pts']) if T['pts'] else np.zeros((1, 3))
        js[name].append(dict(key=k, png=T['png'], w=T['w'], h=T['h'], fmt=T['fmt'], alpha=T['alpha'],
                             tris=T['tris'], area=round(T['area'], 1), ninst=len(T['insts']),
                             nabs=(T.get('nsum', np.zeros(3)) / max(T['area'], 1e-9)).round(3).tolist(),
                             rmodes=sorted('%08x' % r for r in T['rmodes']),
                             bbmin=pts.min(0).round(0).tolist(), bbmax=pts.max(0).round(0).tolist()))
json.dump(js, open(os.path.join(os.path.dirname(OUT), 'texcat.json'), 'w'), indent=1)
