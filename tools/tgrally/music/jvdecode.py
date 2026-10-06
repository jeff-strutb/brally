import numpy as np,sys
def dsc_data8(w):
    return ((w&0x02)<<6)|((w&0x08)<<3)|((w&0x40)>>1)|((w&0x80)>>3)|((w&0x20)>>2)|((w&0x10)>>2)|((w&0x01)<<1)|((w&0x04)>>2)
def dsc_addr8(a):
    return ((a&0x1)<<1)|((a&0x2)<<3)|((a&0x4)>>2)|((a&0x8)>>1)|((a&0x10)>>1)|((a&0x20)<<10)|((a&0x40)<<4)|((a&0x80)<<10)|((a&0x100)<<6)|((a&0x200)>>4)|((a&0x400)>>3)|((a&0x800)<<1)|((a&0x1000)<<4)|((a&0x2000)>>7)|((a&0x4000)<<4)|((a&0x8000)>>4)|((a&0x10000)>>3)|((a&0x20000)>>8)|((a&0x40000)>>10)|(a&0xFFF80000)
def descramble(buf):
    b=np.frombuffer(buf,np.uint8)
    lut=np.array([dsc_data8(i) for i in range(256)],np.uint8)
    i=np.arange(len(b),dtype=np.int64)
    a=dsc_addr8(i)
    out=np.zeros_like(b); out[a]=lut[b]; return out
def decode_stream(d):
    """DPCM over the whole ROM: sample = cumsum(int8(data) << shift), shift nibble from table at ((addr & 0xFFFFF)>>5)|(addr & 0xF00000)."""
    n=len(d); a=np.arange(n,dtype=np.int64)
    sb=d[((a&0xFFFFF)>>5)|(a&0xF00000)]
    sh=np.where(a&0x10, sb>>4, sb&0xF).astype(np.int64)
    v=d.view(np.int8).astype(np.int64)<<sh
    return np.cumsum(v)
if __name__=='__main__':
    for f in sys.argv[1:]:
        o=descramble(open(f,'rb').read()); o.tofile(f+'.dsc')
        print(f, bytes(o[:0x40]), bytes(o[0x20:0x3a]))
