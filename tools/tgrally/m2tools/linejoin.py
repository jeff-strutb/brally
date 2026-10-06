"""linejoin.py base.c outprefix : write variants joining line k with k+1 (k inside the body),
plus variants splitting a line at top-level ' = ' / binary operators"""
import sys,re
base=open(sys.argv[1]).read().split('\n'); pre=sys.argv[2]
n=0
for k in range(2,len(base)-2):
    a,b=base[k],base[k+1]
    if not a.strip() or not b.strip(): continue
    if a.strip().startswith('/*'): continue
    v=base[:k]+[a.rstrip()+' '+b.strip()]+base[k+2:]
    open('%s_j%02d.c'%(pre,k),'w').write('\n'.join(v)); n+=1
# also: one blank line inserted before line k (shifts later numbers)
for k in range(2,len(base)-1):
    v=base[:k]+['']+base[k:]
    open('%s_b%02d.c'%(pre,k),'w').write('\n'.join(v)); n+=1
print(n)
