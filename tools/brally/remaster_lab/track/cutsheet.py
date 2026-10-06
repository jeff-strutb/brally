import sys, struct, numpy as np, collections, json
ARGS=sys.argv[1:]; sys.argv=['x']; sys.path.insert(0,'ports/brally-wasm/tools')
import remaster_env_place as P
from cihash import walk_tluts, unswap, DUMP
from PIL import Image, ImageDraw
SIG={'amazon':'6301_6104_744','coast':'6427_6535_673','desert':'9226_7877_804','mine':'7155_6503_597','mountain':'5398_4994_615','race':'2606_2340_386','bonus':'2388_2505_264'}
def rgba5551(v): return ((v>>11&31)*255//31,(v>>6&31)*255//31,(v>>1&31)*255//31,(v&1)*255)
def decode(f,d,tex,tf,tlut,pal):
    fmt,siz,w,h=tf
    if not (0<w<=256 and 0<h<=256): return None
    o=P.off(tex); bpp=[4,8,16,32][siz]; stride=max(w*bpp//8,8)
    if o is None or o+stride*h>len(f): return None
    raw=unswap(f[o:o+stride*h],stride,h); img=np.zeros((h,w,4),np.uint8)
    ents=None
    if fmt==2:
        to=P.off(tlut) if tlut else None
        if to is None: return None
        ents=struct.unpack_from('>256H',d,to)
    for y in range(h):
        row=raw[y*stride:(y+1)*stride]
        for x in range(w):
            if bpp==4: v=(row[x//2]>>4) if x%2==0 else row[x//2]&15
            elif bpp==8: v=row[x]
            elif bpp==16: v=row[2*x]<<8|row[2*x+1]
            else: continue
            if fmt==0: img[y,x]=rgba5551(v)
            elif fmt==2: img[y,x]=rgba5551(ents[(v+pal*16 if bpp==4 else v)&255])
            elif fmt==3:
                if bpp==4: i=(v>>1)*255//7; a=255*(v&1)
                elif bpp==8: i=(v>>4)*17; a=(v&15)*17
                else: i=v>>8; a=v&255
                img[y,x]=(i,i,i,a)
            else: i=v*17 if bpp==4 else v; img[y,x]=(i,i,i,i)
    return img
def textures(tr):
    f=open(P.TRACKS+f'/{tr}.trk','rb').read(); d=open(DUMP+f'trk_{SIG[tr]}.bin','rb').read()[4:]
    ia,ci=P.be32(f,0x60),P.be32(f,0x64); T={}
    for i in range(ci):
        r=P.off(ia)+i*0x54; dl=P.be32(f,r+0x44)
        if not dl: continue
        for tex,tf,tlut,pal in walk_tluts(f,P.off(dl)):
            k=(tex,tf,tlut,pal)
            e=T.setdefault(k,{'n':0,'inst':set()}); e['n']+=1; e['inst'].add(i)
    out=[]
    for k,e in T.items():
        im=decode(f,d,*k)
        if im is None: continue
        if (im[...,3]<128).mean()<0.04: continue        # opaque: not a cut-out
        out.append((k,e,im))
    out.sort(key=lambda t:-len(t[1]['inst']))
    return out
if __name__=='__main__':
    tr=sys.argv[1] if len(sys.argv)>1 else 'amazon'
    for tr in ARGS:
        L=textures(tr); C=120; COLS=12
        S=Image.new('RGB',(COLS*C,((len(L)+COLS-1)//COLS)*(C+14)),(255,0,255)); dr=ImageDraw.Draw(S)
        meta=[]
        for n,(k,e,im) in enumerate(L):
            t=Image.fromarray(im,'RGBA'); s=(C-4)/max(t.size); t=t.resize((max(1,int(t.size[0]*s)),max(1,int(t.size[1]*s))),Image.NEAREST)
            x,y=(n%COLS)*C,(n//COLS)*(C+14); bg=Image.new('RGBA',t.size,(255,0,255,255)); bg.alpha_composite(t)
            S.paste(bg.convert('RGB'),(x+2,y+2)); dr.rectangle((x,y+C,x+C,y+C+13),fill=(0,0,0)); dr.text((x+2,y+C),f'{n} {hex(k[0])[2:]} x{len(e["inst"])}',fill=(255,255,0))
            meta.append(dict(n=n,tex=k[0],tf=k[1],tlut=k[2],pal=k[3],inst=len(e['inst'])))
        S.save(f'cut_{tr}.png'); json.dump(meta,open(f'cut_{tr}.json','w')); print(tr,len(L))
