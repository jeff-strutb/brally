"""alldec.py VA full.c : read-only; every p1 decision in order (web, sym, type, save, reg, decision, live span)."""
import os, sys
os.environ['TGR_TRACE_CC'] = os.path.abspath('build/tgrally/ext/instr4/out/cc')
sys.path.insert(0, 'tools/tgrally')
import n64alloc as A
g = A.Grader(int(sys.argv[1], 16), sys.argv[2])
nd, log, st = g.grade({'CDX_LOG': '1', 'CDX_DETAIL_WEB': 'all'})
recs = []; cur = None; dets = {}
for l in log.splitlines():
    if '[CDX]' not in l: continue
    x = dict(kv.split('=', 1) for kv in l.split()[2:] if '=' in kv)
    if 'p1dec' in l: cur = {'dec': x}; recs.append(cur)
    if 'p1live' in l and cur: cur['live'] = x['blocks']
    if 'webdetail' in l: dets[x['web']] = x
print('nd', nd)
for r in recs:
    x = r['dec']; d = dets.get(x['web'], {})
    lv = r.get('live', '').rstrip(',').split(',')
    print('%5s sym %5s type %s raw10 %-10s save %8s %-4s %-6s live %s..%s (%d)' % (x['web'], x['sym'], d.get('type'), d.get('raw10'), x['save'][:7], x['bestreg'], x['decision'], lv[0], lv[-1], len(lv)))
