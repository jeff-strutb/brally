import sys, numpy as np
from PIL import Image, ImageDraw
from trkparse import parse
from render import render
D='build/brally/wasm32/app/extract/disc/tracks/'
tr=sys.argv[1]; start=int(sys.argv[2]) if len(sys.argv)>2 else 0; cnt=int(sys.argv[3]) if len(sys.argv)>3 else 200
texs,insts=parse(D+tr+'.trk')
C=112; COLS=16
sel=insts[start:start+cnt]
S=Image.new('RGB',(COLS*C,((len(sel)+COLS-1)//COLS)*(C+12)),(20,20,20)); d=ImageDraw.Draw(S)
for n,I in enumerate(sel):
    sc=np.linalg.norm(I['M'][0,:3]) if I['i']>=0 else 1
    tris=[(P*sc,UV,k) for P,UV,k,rm in I['tris']]
    im=render(tris,texs,C-2)
    x,y=(n%COLS)*C,(n//COLS)*(C+12)
    S.paste(Image.fromarray(im),(x+1,y+1)); d.text((x+2,y+C-1),'%d t%d'%(I['i'],len(I['tris'])),fill=(255,255,0))
S.save('inst_%s_%d.png'%(tr,start))
print(len(insts))
