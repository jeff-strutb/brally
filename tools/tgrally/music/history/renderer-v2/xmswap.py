"""Rebuild an XM with chosen samples replaced by higher-quality sources (16-bit, pitch/loop/level preserved)."""
import struct,numpy as np,math
def rewrite(m, repl):
    """repl: {sample_order_index: dict(pcm16=int array, loop=(ls,ll) or None, semis=float)}"""
    hs=struct.unpack_from('<I',m,60)[0]; npat,nins=struct.unpack_from('<HH',m,70)
    q=60+hs
    for _ in range(npat):
        h,=struct.unpack_from('<I',m,q); ps,=struct.unpack_from('<H',m,q+7); q+=h+ps
    out=bytearray(m[:q]); sidx=0
    for _ in range(nins):
        isz,=struct.unpack_from('<I',m,q); n,=struct.unpack_from('<H',m,q+27)
        out+=m[q:q+isz]
        if not n: q+=isz; continue
        shs,=struct.unpack_from('<I',m,q+29); q+=isz
        hdrs=[]; 
        for j in range(n): hdrs.append(bytearray(m[q:q+shs])); q+=shs
        datas=[]
        for h in hdrs:
            ln=struct.unpack_from('<I',h,0)[0]; datas.append(m[q:q+ln]); q+=ln
        for j,h in enumerate(hdrs):
            k=sidx+j
            if k in repl:
                r=repl[k]; pcm=np.asarray(r['pcm16'],dtype=np.int64)
                ft=struct.unpack_from('<b',h,13)[0]; rel=struct.unpack_from('<b',h,16)[0]
                pitch=rel*128+ft+round(r['semis']*128)
                rel2=int(math.floor((pitch+64)/128)); ft2=pitch-rel2*128
                assert -128<=rel2<=127 and -128<=ft2<=127
                typ=(h[14]&3)|16
                if r['loop'] is None: typ&=~3; ls=ll=0
                else: ls,ll=r['loop']
                struct.pack_into('<III',h,0,len(pcm)*2,ls*2,ll*2)
                struct.pack_into('<b',h,13,ft2); h[14]=typ; struct.pack_into('<b',h,16,rel2)
                delta=np.diff(np.concatenate([[0],pcm])).astype(np.int64)
                datas[j]=((delta+32768)%65536-32768).astype('<i2').tobytes()
        sidx+=n
        for h in hdrs: out+=h
        for d in datas: out+=d
    out+=m[q:]
    return bytes(out)
