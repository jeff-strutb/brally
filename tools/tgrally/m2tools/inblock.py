"""inblock.py VA full.c BLOCK [type] : read-only; every p1 decision (in order) whose live blocks include BLOCK."""
import os, sys
os.environ['TGR_TRACE_CC'] = os.path.abspath('build/tgrally/ext/instr4/out/cc')
sys.path.insert(0, 'tools/tgrally')
import n64alloc as A
g = A.Grader(int(sys.argv[1], 16), sys.argv[2])
blk = sys.argv[3]; ty = sys.argv[4] if len(sys.argv) > 4 else None
nd, log, st = g.grade({'CDX_LOG': '1', 'CDX_DETAIL_WEB': 'all'})
recs = []; cur = None
for l in log.splitlines():
    if '[CDX]' not in l: continue
    x = dict(kv.split('=', 1) for kv in l.split()[2:] if '=' in kv)
    if 'p1dec' in l: cur = {'dec': x}; recs.append(cur)
    if 'p1live' in l and cur: cur['live'] = x['blocks']
    if 'webdetail' in l and cur and cur['dec']['web'] == x['web']: cur['det'] = x
print('nd', nd)
for r in recs:
    x = r['dec']; d = r.get('det', {})
    if blk in r.get('live', '').split(',') and (ty is None or d.get('type') == ty):
        lv = r.get('live', '').rstrip(',').split(',')
        print('%5s sym %5s type %s tbl %5s raw10 %-10s save %8s nocs %3s %-4s %-6s net %7s live %s..%s (%d)' % (x['web'], x['sym'], d.get('type'), d.get('table'), d.get('raw10'), x['save'][:7], x['nocs'], x['bestreg'], x['decision'], x['totalsave'][:6], lv[0], lv[-1], len(lv)))
