import numpy as np
DP=[0x02,0x00,0x04,0x05,0x07,0x06,0x03,0x01,0x0a,0x08,0x0c,0x0d,0x0f,0x0e,0x0b,0x09]
AP=[0x03,0x01,0x02,0x00,0x0c,0x06,0x0b,0x04,0x09,0x0f,0x08,0x05,0x07,0x0d,0x10,0x0a,0x0e,0x11,0x12,0x13]
def permute(v,perm):
    r=np.zeros_like(v)
    for bit,src in enumerate(perm): r|=((v>>src)&1)<<bit
    return r
def descramble(raw):
    w=np.frombuffer(raw,'<u2').astype(np.uint32)
    src=np.arange(len(w),dtype=np.uint32)
    out=np.zeros(len(w),np.uint32)
    data=permute(w,DP); tgt=permute(src,AP)
    out[tgt]=data
    out=out.astype('<u2').view(np.uint8).copy()
    out[:0x20]=np.frombuffer(raw[:0x20],np.uint8)   # plaintext header
    return out
def decode_bank(b):
    """FCE decode of one 1 MiB bank as a continuous stream (int64 accumulator)."""
    a=np.arange(len(b),dtype=np.int64)
    packed=b[a>>5]; exp=np.where(a&0x10,packed>>4,packed&15).astype(np.int64)
    delta=b.view(np.int8).astype(np.int64)
    return np.cumsum(delta<<exp)
if __name__=='__main__':
    for i in range(1,5):
        raw=open('jv/jv1080_waverom%d.bin'%i,'rb').read(); d=descramble(raw)
        print(i,bytes(d[:0x20]),bytes(d[0x20:0x28]),bytes(d[0x30:0x3a]))
        d.tofile('jv/w%d.dsc'%i)
