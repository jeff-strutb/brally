import sys, struct, numpy as np, collections
sys.argv=['x']; sys.path.insert(0,'ports/brally-wasm/tools')
import remaster_env_place as P
DUMP='build/brally/remaster/lab/trkdump/'
tm=P.texmat()
def fnv(px):
    h=2166136261
    for x in px: h=((h^int(x))*16777619)&0xFFFFFFFF
    return h
def unswap(raw,stride,h):
    raw=bytearray(raw)
    for y in range(1,h,2):
        r0=y*stride
        for q in range(r0,r0+stride-7,8): raw[q:q+8]=raw[q+4:q+8]+raw[q:q+4]
    return bytes(raw)
def c5551(v):
    return ((v>>11&31)*255//31)|(((v>>6&31)*255//31)<<8)|(((v>>1&31)*255//31)<<16)|((v&1)*255<<24)
def walk_tluts(b, o):
    """per tri command: (texaddr, tf, tlut addr) """
    out=[]; timg=tex=tlut=0; fmt=siz=w=h=pal=0; start=o
    while o+8<=len(b):
        w0,w1=struct.unpack_from('>II',b,o); op=w0>>24
        if op==0xFD: timg=w1
        elif op==0xF3: tex=timg
        elif op==0xF0: tlut=timg
        elif op==0xF5 and (w1>>24&7)==0: fmt,siz,pal=w0>>21&7,w0>>19&3,w1>>20&15
        elif op==0xF2 and (w1>>24&7)==0:
            w=(w1>>12&0xFFF)//4-(w0>>12&0xFFF)//4+1; h=(w1&0xFFF)//4-(w0&0xFFF)//4+1
        elif op in (0xBF,0xB1): out.append((tex,(fmt,siz,w,h),tlut,pal))
        elif op==0xB8: break
        o+=8
    return out
def run(tr, sig, mode):
    f=open(P.TRACKS+f'/{tr}.trk','rb').read(); d=open(DUMP+f'trk_{sig}.bin','rb').read()[4:]
    ia,ci=P.be32(f,0x60),P.be32(f,0x64); seen={}
    for i in range(ci):
        r=P.off(ia)+i*0x54; dl=P.be32(f,r+0x44)
        if not dl: continue
        for tex,tf,tlut,pal in walk_tluts(f,P.off(dl)):
            fmt,siz,w,h=tf
            if fmt!=2 or not (0<w<=256 and 0<h<=256) or (tex,tf,tlut,pal) in seen: continue
            o=P.off(tex); to=P.off(tlut) if tlut else None
            if o is None or to is None: continue
            bpp=4 if siz==0 else 8; stride=max(w*bpp//8,8)
            raw=unswap(f[o:o+stride*h],stride,h)
            n=256 if siz==1 else 16
            ents=struct.unpack_from(('>' if mode=='be' else '<')+f'{n*(1 if siz==1 else 16)}H',d,to)
            idx=[]
            for y in range(h):
                row=raw[y*stride:(y+1)*stride]
                for x in range(w):
                    if bpp==4: v=(row[x//2]>>4) if x%2==0 else row[x//2]&15; v+=pal*16 if siz==0 else 0
                    else: v=row[x]
                    idx.append(v)
            px=[c5551(ents[i%len(ents)]) for i in idx]
            seen[(tex,tf,tlut,pal)]=tm.get(fnv(px))
    v=list(seen.values()); return len(v), sum(1 for x in v if x), collections.Counter(v)
if __name__=='__main__':
    for mode in ('be','le'):
        print(mode, run('amazon','6301_6104_744',mode)[:2])
