import json, sys, numpy as np
from PIL import Image, ImageDraw
from trkparse import parse
D='build/brally/wasm32/app/extract/disc/tracks/'
cat=json.load(open('catalog.json'))
for T in cat:
    texs,_=parse(D+T['id']+'.trk')
    items=[(n,i) for n,i in enumerate(T['items']) if i['cat'] in ('card','cutstruct')]
    C=128; COLS=12
    S=Image.new('RGB',(COLS*C*2,((len(items)+COLS-1)//COLS)*(C+14)),(25,25,25)); d=ImageDraw.Draw(S)
    for k,(n,i) in enumerate(items):
        x,y=(k%COLS)*C*2,(k//COLS)*(C+14)
        import base64,io
        th=Image.open(io.BytesIO(base64.b64decode(i['thumb']))).convert('RGB').resize((C,C)); S.paste(th,(x,y))
        im=texs[i['tex']]['img']
        if im is not None:
            t=Image.fromarray(im,'RGBA'); s=C/max(t.size); t=t.resize((max(1,int(t.size[0]*s)),max(1,int(t.size[1]*s))),Image.NEAREST)
            bg=Image.new('RGBA',t.size,(255,0,255,255)); bg.alpha_composite(t); S.paste(bg.convert('RGB'),(x+C,y))
        d.text((x+2,y+C),'%d %s x%d'%(n,i['tex'][:8],i['n']),fill=(255,255,0))
    S.save('cards_%s.png'%T['id'])
