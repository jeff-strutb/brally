"""tryv.py VA variant.c [variant2.c ...] : each variant file holds a replacement
function body (full definition); spliced into the tree file and graded."""
import os,sys
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T, n64build as B
from concurrent.futures import ThreadPoolExecutor
va=int(sys.argv[1],16); path,name,_=T.source_of(va)
tree=open(path).read(); body=T.function_text(tree,name)
work=os.path.join(os.path.dirname(__file__),'tw'); os.makedirs(work,exist_ok=True)
diff='--diff' in sys.argv
files=[a for a in sys.argv[2:] if not a.startswith('--')]
def run(f):
    nb=open(f).read().strip('\n')
    src=tree.replace(body,nb)
    if f.endswith('.full.c'): src=nb
    return f,S.grade(src,va,name,work)
with ThreadPoolExecutor(12) as ex:
    for f,g in ex.map(run,files): print(os.path.basename(f),g)
