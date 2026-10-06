"""intf.py VA full.c WEB [REG...] : read-only. Webs whose live blocks overlap WEB, in p1 decision order up to WEB,
with save/colour; REG filters to neighbours holding those colours. Uses build/tgrally/ext/instr4 (live-block records)."""
import os, sys
os.environ['TGR_TRACE_CC'] = os.path.abspath('build/tgrally/ext/instr4/out/cc')
sys.path.insert(0, 'tools/tgrally')
import n64alloc as A
va = int(sys.argv[1], 16); full = sys.argv[2]; target = sys.argv[3]; regs = set(sys.argv[4:])
g = A.Grader(va, full)
nd, log, st = g.grade({'CDX_LOG': '1', 'CDX_DETAIL_WEB': 'all'})
det, seq, live = {}, [], {}
last = None
for l in log.splitlines():
    if '[CDX]' not in l: continue
    x = dict(kv.split('=', 1) for kv in l.split()[2:] if '=' in kv)
    if 'webdetail' in l: det[x['web']] = x
    if 'p1dec' in l: seq.append(x); last = x
    if 'p1live' in l and last is not None:
        live[id(last)] = set(int(b) for b in x['blocks'].split(',') if b)
tdecs = [x for x in seq if x['web'] == target]
if not tdecs: sys.exit('no web %s' % target)
t = tdecs[-1]; tb = live[id(t)]
print('nd', nd, 'target', target, 'blocks', sorted(tb), 'reg', t['bestreg'], t['decision'], 'save', t['save'][:7], 'forb', t['forbidden0'])
for x in seq:
    if x is t: break
    if x['decision'] != 'color': continue
    if regs and x['bestreg'] not in regs: continue
    ov = live[id(x)] & tb
    if ov:
        d = det.get(x['web'], {})
        print('  %5s type %s tbl %5s raw10 %-10s save %8s nocs %3s %-4s overlap %s' % (x['web'], d.get('type'), d.get('table'), d.get('raw10'), x['save'][:7], x['nocs'], x['bestreg'], sorted(ov)[:8]))
