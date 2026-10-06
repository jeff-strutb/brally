"""scoreall.py: grade every N64 T3 row against the tree. prints VA size file name aligned regblind positional"""
import os,sys,subprocess
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T
from concurrent.futures import ThreadPoolExecutor
rows=[l.split() for l in subprocess.run(['.venv/bin/python','tools/tgrally/n64tiers.py','--no-build','--list','T3'],capture_output=True,text=True).stdout.splitlines() if l.strip()]
work=os.path.join(os.path.dirname(__file__),'tw'); os.makedirs(work,exist_ok=True)
def run(r):
    va=int(r[0],16); path,name,_=T.source_of(va)
    try: g=S.grade(open(path).read(),va,name,work)
    except Exception as e: g=('ERR',str(e)[:40])
    return r[0],r[1],path,name,g
with ThreadPoolExecutor(8) as ex:
    for res in ex.map(run,rows): print(*res[:4],res[4]); sys.stdout.flush()
