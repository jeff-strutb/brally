"""For every function with rows: keep its hand rows (comment not starting
'lockstep'), replace its lockstep rows with a fresh lockstep_rows run.
Applied to the working tree and the index (other sessions' edits stay)."""
import subprocess, collections
path = 'config/brally/reloc_overrides.csv'
def funcs(text):
    s = set()
    for l in text.splitlines():
        p = l.split(',')
        if len(p) > 3 and p[0].startswith('0x'):
            s.add(p[0].upper())
    return s
tree = open(path, 'rb').read().decode('latin1')
fresh = {}
for va in sorted(funcs(tree)):
    out = subprocess.run(['.venv/bin/python', 'tools/brally/lockstep_rows.py', va],
                         capture_output=True, text=True).stdout
    if 'REFUSED' in out or not out.strip():
        print(va, 'refused: kept as is'); continue
    hand = {int(l.split(',')[1], 0) for l in tree.splitlines()
            if l.upper().startswith(va + ',') and len(l.split(',')) > 3 and ',lockstep ' not in l}
    fresh[va] = [l for l in out.splitlines() if l.upper().startswith(va + ',')
                 and int(l.split(',')[1], 0) not in hand]
def fix(text):
    lines = text.splitlines(keepends=True)
    out, done = [], set()
    for l in lines:
        va = l.split(',')[0].upper()
        if va in fresh and len(l.split(',')) > 3 and ',lockstep ' in l:
            if va not in done:
                out += [r + '\n' for r in fresh[va]]
                done.add(va)
            continue
        out.append(l)
    for va in fresh:
        if va not in done:
            out += [r + '\n' for r in fresh[va]]
    return ''.join(out)
t = fix(tree)
open(path, 'wb').write(t.encode('latin1'))
idx = subprocess.run(['git', 'show', ':' + path], capture_output=True).stdout.decode('latin1')
blob = subprocess.run(['git', 'hash-object', '-w', '--stdin'], input=fix(idx).encode('latin1'),
                      capture_output=True).stdout.decode().strip()
subprocess.run(['git', 'update-index', '--cacheinfo', '100644,%s,%s' % (blob, path)], check=True)
print('regenerated', len(fresh), 'functions')
