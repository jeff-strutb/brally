import struct, os, collections
exec(open('trkcensus.py').read().split('out = {}')[0])
fmts = collections.Counter(); ops=collections.Counter()
for fn in sorted(os.listdir(D)):
    if not fn.endswith('.trk'): continue
    b = open(D+fn,'rb').read()
    ia = be32(b,0x60); ci = be32(b,0x64); texs=set()
    for i in range(ci):
        r = off(ia)+i*0x54; dl = be32(b,r+0x44)
        if not dl: continue
        o = off(dl)
        while o+8<=len(b):
            w0,w1=be32(b,o),be32(b,o+4); op=w0>>24; ops[op]+=1
            if op==0xFD: fmts[(FMT[(w0>>21)&7] if (w0>>21)&7<5 else '?',SIZ[(w0>>19)&3],(w0&0xFFF)+1)]+=1; texs.add(w1)
            if op==0xB8: break
            o+=8
    print(fn, 'distinct SETTIMG', len(texs))
print(fmts.most_common(20)); print({hex(k):v for k,v in ops.items()})
