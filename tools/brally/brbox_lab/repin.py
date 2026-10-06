"""Re-point the hand rows for (va, symbol) at the symbol's CURRENT relocation
offsets in the sweep object, in the working tree and the index."""
import sys, re, subprocess
sys.path.insert(0, 'tools')
import reloc_fill
va, fname, obj, sym, value = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
d, secs, syms, relocs = reloc_fill.parse(obj)
fn = next(s for s in syms if reloc_fill.func_symbol_matches(s['name'], fname) and secs[s['sec']]['name'].startswith('.text'))
offs = [rva - fn['val'] for rva, si, rt in relocs[fn['sec']] if next(s for s in syms if s['idx'] == si)['name'] == sym]
p = 'config/brally/reloc_overrides.csv'
def fix(text):
    nl = '\r\n' if '\r\n' in text else '\n'
    keep = [l for l in text.split(nl) if not (l.upper().startswith(va.upper() + ',') and ',' + value.upper() + ',' in l.upper() and sym in l)]
    while keep and keep[-1] == '':
        keep.pop()
    for o in offs:
        keep.append('%s,0x%X,%s,%s -- the .data velocity scale (1.0); the audit pairs it to the 0.2 constant (0x10077b40) -- live oracle 2026-09-23' % (va, o, value, sym))
    return nl.join(keep) + nl
raw = open(p, 'rb').read().decode('latin1')
open(p, 'wb').write(fix(raw).encode('latin1'))
idx = subprocess.run(['git', 'show', ':' + p], capture_output=True).stdout.decode('latin1')
blob = subprocess.run(['git', 'hash-object', '-w', '--stdin'], input=fix(idx).encode('latin1'), capture_output=True).stdout.decode().strip()
subprocess.run(['git', 'update-index', '--cacheinfo', '100644,%s,%s' % (blob, p)], check=True)
print('pinned', [hex(o) for o in offs])
