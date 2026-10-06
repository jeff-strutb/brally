import struct, sys, os, json, collections, math
BASE = 0x80025C00
D = 'build/brally/wasm32/app/extract/disc/tracks/'

def be32(b, o): return struct.unpack_from('>I', b, o)[0]
def bef(b, o): return struct.unpack_from('>f', b, o)[0]

def off(a):
    if a >= 0x80000000: return a - BASE
    return None

def walk(b, o):
    """walk a list; return tris(list of 3 verts), textures, cmds, ok"""
    vbuf = [None]*64
    tris = []; tex = []; cmds = collections.Counter(); tile = None
    ok = False
    for _ in range(200000):
        if o is None or o + 8 > len(b): break
        w0, w1 = be32(b, o), be32(b, o+4); op = w0 >> 24
        cmds[op] += 1
        if op == 0x04:
            n = (w0 >> 10) & 0x3F; v0 = ((w0 >> 16) & 0xFF) // 2
            vo = off(w1)
            if vo is not None:
                for i in range(n):
                    x, y, z, fl, s, t = struct.unpack_from('>hhhhhh', b, vo + 16*i)
                    r, g, bb, a = b[vo+16*i+12:vo+16*i+16]
                    if v0+i < 64: vbuf[v0+i] = (x, y, z, s, t, a)
        elif op == 0xBF:
            idx = [(w1 >> 16 & 0xFF)//2, (w1 >> 8 & 0xFF)//2, (w1 & 0xFF)//2]
            tris.append([vbuf[i] for i in idx])
        elif op == 0xB1:
            for w in (w0, w1):
                idx = [(w >> 16 & 0xFF)//2, (w >> 8 & 0xFF)//2, (w & 0xFF)//2]
                tris.append([vbuf[i] for i in idx])
        elif op == 0xFD:
            fmt = (w0 >> 21) & 7; siz = (w0 >> 19) & 3
            tex.append((w1, fmt, siz))
        elif op == 0xB8:
            ok = True; break
        o += 8
    return tris, tex, cmds, ok

FMT = ['RGBA', 'YUV', 'CI', 'IA', 'I']
SIZ = [4, 8, 16, 32]

def planar(tris):
    pts = [v[:3] for t in tris for v in t if v]
    if len(pts) < 3: return None
    # PCA thickness
    n = len(pts); c = [sum(p[i] for p in pts)/n for i in range(3)]
    cov = [[sum((p[i]-c[i])*(p[j]-c[j]) for p in pts)/n for j in range(3)] for i in range(3)]
    # eigenvalues via characteristic eq (symmetric 3x3)
    a = cov; p1 = a[0][1]**2 + a[0][2]**2 + a[1][2]**2
    q = (a[0][0]+a[1][1]+a[2][2])/3
    p2 = (a[0][0]-q)**2 + (a[1][1]-q)**2 + (a[2][2]-q)**2 + 2*p1
    p = math.sqrt(p2/6) or 1e-9
    B = [[(a[i][j] - (q if i == j else 0))/p for j in range(3)] for i in range(3)]
    detB = (B[0][0]*(B[1][1]*B[2][2]-B[1][2]*B[2][1]) - B[0][1]*(B[1][0]*B[2][2]-B[1][2]*B[2][0]) + B[0][2]*(B[1][0]*B[2][1]-B[1][1]*B[2][0]))
    r = max(-1, min(1, detB/2)); phi = math.acos(r)/3
    e1 = q + 2*p*math.cos(phi); e3 = q + 2*p*math.cos(phi + 2*math.pi/3); e2 = 3*q - e1 - e3
    return math.sqrt(max(e3, 0)), math.sqrt(max(e2, 0)), math.sqrt(max(e1, 0))

def normal(t):
    (a, b, c) = [v[:3] for v in t]
    u = [b[i]-a[i] for i in range(3)]; w = [c[i]-a[i] for i in range(3)]
    n = [u[1]*w[2]-u[2]*w[1], u[2]*w[0]-u[0]*w[2], u[0]*w[1]-u[1]*w[0]]
    l = math.sqrt(sum(x*x for x in n)) or 1
    return [x/l for x in n]

out = {}
for fn in sorted(os.listdir(D)):
    if not fn.endswith('.trk'): continue
    b = open(D+fn, 'rb').read()
    root = be32(b, 0x50); ia = be32(b, 0x60); ci = be32(b, 0x64)
    lists = collections.OrderedDict()
    insts = []
    for i in range(ci):
        r = off(ia) + i*0x54
        m = struct.unpack_from('>16f', b, r)
        dl = be32(b, r+0x44)
        extra = b[r+0x40:r+0x54].hex()
        insts.append((i, dl, m, extra))
        if dl: lists.setdefault(dl, []).append(i)
    meshes = []
    rt = walk(b, off(root)) if root else None
    for dl, ii in lists.items():
        tris, tex, cmds, ok = walk(b, off(dl))
        tris = [t for t in tris if all(t)]
        ev = planar(tris)
        # orientation stats
        ups = [abs(normal(t)[1]) for t in tris]
        alpha_v = sum(1 for t in tris for v in t if v[5] < 255)
        m0 = insts[ii[0]][2]
        sc = math.sqrt(m0[0]**2 + m0[1]**2 + m0[2]**2)
        pos = [(round(insts[k][2][12]), round(insts[k][2][13]), round(insts[k][2][14])) for k in ii]
        pts = [v[:3] for t in tris for v in t]
        bb = [[min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]] if pts else None
        meshes.append(dict(dl=hex(dl), n=len(ii), tris=len(tris), ok=ok,
                           tex=sorted(set((hex(a), FMT[f] if f < 5 else f, SIZ[s]) for a, f, s in tex)),
                           ev=ev, upfrac=sum(1 for u in ups if u > 0.9)/max(1, len(ups)),
                           vertfrac=sum(1 for u in ups if u < 0.2)/max(1, len(ups)),
                           scale=sc, bb=bb, pos=pos[:6], flags=[insts[k][3] for k in ii[:3]]))
    out[fn] = dict(root=hex(root), root_tris=len(rt[0]) if rt else 0, root_tex=len(set(t[0] for t in rt[1])) if rt else 0,
                   ninst=ci, nlists=len(lists), meshes=meshes)
    print(fn, 'inst', ci, 'distinct', len(lists), 'root tris', out[fn]['root_tris'])
json.dump(out, open(os.path.dirname(__file__)+'/census.json', 'w'), indent=1, default=str)
