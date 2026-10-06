import struct, os, collections, hashlib, math
exec(open('trkcensus.py').read().split('out = {}')[0])
seen = collections.defaultdict(list)
geo = {}
for fn in sorted(os.listdir(D)):
    if not fn.endswith('.trk'): continue
    b = open(D+fn,'rb').read()
    ia = be32(b,0x60); ci = be32(b,0x64)
    for i in range(ci):
        r = off(ia)+i*0x54; dl = be32(b,r+0x44)
        if not dl: continue
        tris, tex, cmds, ok = walk(b, off(dl))
        tris = [t for t in tris if all(t)]
        h = hashlib.md5()
        for t in tris:
            h.update(repr([v[:5] for v in t]).encode())
        for a,f,s in tex:
            o = off(a); h.update(b[o:o+256] if o else b'')
        k = h.hexdigest()[:10]
        m = struct.unpack_from('>16f', b, r)
        seen[k].append((fn, i, m[12], m[13], m[14]))
        geo[k] = (len(tris),)
print('unique geometries', len(seen))
rep = [(k,v) for k,v in seen.items() if len(v)>1]
print('repeated', len(rep))
for k,v in sorted(rep, key=lambda kv:-len(kv[1]))[:40]:
    print(k, len(v), geo[k][0], collections.Counter(x[0] for x in v).most_common())
