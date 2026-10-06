import sys, glob, os
from PIL import Image
d=sys.argv[1]; out=sys.argv[2]; names=sys.argv[3].split(",") ; suf=sys.argv[4] if len(sys.argv)>4 else "rem"
ims=[Image.open(f"{d}/{n}_{suf}.ppm").convert("RGB") for n in names]
w=900; ims=[i.resize((w,int(i.size[1]*w/i.size[0]))) for i in ims]
cols=2; rows=(len(ims)+1)//2; h=ims[0].size[1]
S=Image.new("RGB",(w*cols+8,(h+8)*rows),(0,0,0))
for k,i in enumerate(ims): S.paste(i,((k%cols)*(w+8),(k//cols)*(h+8)))
S.save(out,quality=88)
