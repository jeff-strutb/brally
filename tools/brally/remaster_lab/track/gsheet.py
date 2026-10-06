import json,base64,io
from PIL import Image,ImageDraw
cat=json.load(open('catalog.json'))
for T in cat:
    it=T['items']; C=120; COLS=15
    S=Image.new('RGB',(COLS*C,((len(it)+COLS-1)//COLS)*(C+12)),(15,15,15)); d=ImageDraw.Draw(S)
    for n,x in enumerate(it):
        im=Image.open(io.BytesIO(base64.b64decode(x['thumb']))).convert('RGB')
        X,Y=(n%COLS)*C,(n//COLS)*(C+12); S.paste(im,(X,Y))
        d.text((X+2,Y+C),'%d %s x%d'%(n,x['cat'][:4],x['n']),fill=(255,255,0))
    S.save('g_%s.png'%T['id'])
