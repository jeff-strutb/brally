import struct,numpy as np,modsamp,os,glob
def load_st(path):
    d=open(path,'rb').read()
    if d[:4]==b'FORM' and d[8:12]==b'8SVX':
        p=12; rate=8363; body=b''
        while p+8<=len(d):
            cid=d[p:p+4]; ln=struct.unpack('>I',d[p+4:p+8])[0]; c=d[p+8:p+8+ln]
            if cid==b'VHDR': rate=struct.unpack('>H',c[12:14])[0]
            if cid==b'BODY': body=c
            p+=8+ln+(ln&1)
        raw=np.frombuffer(body,dtype=np.int8).astype(np.int16); fmt='8SVX'
    else:
        raw=np.frombuffer(d,dtype=np.int8).astype(np.int16); rate=8363; fmt='RAW'
    return modsamp.mk(os.path.relpath(path,'stxx'),raw,8,c5=rate,fmt=fmt)
def all_st():
    out={}
    for f in sorted(glob.glob('stxx/**/*',recursive=True)):
        if os.path.isfile(f) and os.path.getsize(f)>32:
            try: out[f]=[load_st(f)]
            except Exception: pass
    return out
