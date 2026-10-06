import sys, csv, re, struct, os
sys.path.insert(0,'tools')
import t3, reloc_fill, t3b_env as ENV
from relocmap import load_maps
rows = t3.report_rows()
fnmap, glmap = load_maps()
shared = {}
for r in csv.reader(open('config/brally/globals_shared.csv')):
    try: shared[int(r[0],16)] = int(r[1],16)
    except Exception: pass
def xl(a):
    # a D3D-space address -> its Glide twin (exact, else nearest base <= a)
    if a in shared: return shared[a]
    return a
over = list(csv.DictReader(open('config/brally/reloc_overrides.csv', encoding='latin1')))
byva = {}
for r in over:
    byva.setdefault(int(r['func_va'],16), []).append(r)
out = []
for va, rs in sorted(byva.items()):
    row = rows.get('0x%08x' % va)
    if not row: continue
    obj = t3._find_obj(row)
    if not obj: continue
    name = row['name']
    if row.get('cpp'):
        import cpp_score
        _n, cs, _k = cpp_score.parse_implements_name(os.path.join(ENV.ROOT, row['file']), va)
        name = cs or name
    sym = t3._sym(obj, name)
    if not sym: continue
    try:
        d, secs, syms, relocs = reloc_fill.parse(obj)
    except Exception: continue
    fn = next((s for s in syms if reloc_fill.func_symbol_matches(s['name'], sym) and secs.get(s['sec'],{}).get('name','').startswith('.text')), None)
    if not fn: continue
    rl = {}
    for rva, si, rt in relocs.get(fn['sec'], []):
        off = rva - fn['val']
        ts = next((s for s in syms if s['idx']==si), None)
        addend = struct.unpack_from('<i', d, secs[fn['sec']]['praw'] + rva)[0]
        rl[off] = (ts['name'] if ts else '?', rt, addend)
    af, ag = ENV.augment_maps(obj, sym, 0x10000)
    for r in rs:
        off = int(r['offset'],16); val = int(r['value'],16)
        if off not in rl: continue
        s_, rt, addend = rl[off]
        if rt != reloc_fill.REL_DIR32: continue
        a = reloc_fill.resolve(s_, af, ag)
        if a is None: continue
        want = (xl(a) + addend) & 0xFFFFFFFF
        want2 = xl((a + addend) & 0xFFFFFFFF)
        m = re.match(r'lockstep (\S+)', r['comment'])
        stale = m and m.group(1) not in (s_, s_.lstrip('_')) and not s_.lstrip('_').startswith(m.group(1).lstrip('_'))
        if val not in (want, want2) or stale:
            out.append((va, row['name'], off, s_, val, want, ('STALE ' if stale else '') + r['comment'][:60]))
for o in out: print('0x%08X %-24s +0x%X %-28s row %08X map %08X  %s' % o)
print(len(out), 'disagreeing DIR32 rows')
