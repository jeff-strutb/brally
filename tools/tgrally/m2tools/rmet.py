"""rmet.py VA draft.c [draft2.c ...] : per-region masked (blind) diff counts; regions split at ROM jal/branch-target anchors every ~64 ROM instructions."""
import difflib, os, re, subprocess, sys
def load(f):
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'dv.py'), sys.argv[1], f],
                         capture_output=True, text=True).stdout.splitlines()
    ours, rom = [], []
    for l in out[1:]:
        if '|' not in l: continue
        a, b = l.split('|', 1)
        o, r = a[12:].strip(), b.strip()
        if o: ours.append(re.sub(r'\s+', ' ', o))
        if r: rom.append(re.sub(r'\s+', ' ', r))
    return ours, rom
def m1(x):
    x = re.sub(r'^lui \$at, -?0x[0-9a-f]+|^lui \$at, 0$', 'lui $at, K', x)
    x = re.sub(r'-?0x[0-9a-f]+\(\$at\)|\b\d+\(\$at\)', 'K($at)', x)
    x = re.sub(r'(lui \$\w+, |addiu \$\w+, \$\w+, )0$', r'\1K', x)
    x = re.sub(r'^(b\w*|j) (.*?)0x8024[0-9a-f]{4}$', r'\1 \2B', x)
    return re.sub(r'\$(t\d|s\d|v\d|a\d|f\d+|at|ra)', 'R', x)
mask = lambda L: [m1(x) for x in L]
tot = lambda per: sum(per.values())
va = int(sys.argv[1], 16)
for f in sys.argv[2:]:
    ours, rom = load(f)
    sm = difflib.SequenceMatcher(None, mask(ours), mask(rom), autojunk=False)
    per = {}
    for t, i1, i2, j1, j2 in sm.get_opcodes():
        if t == 'equal': continue
        k = j1 // 64
        per[k] = per.get(k, 0) + max(i2 - i1, j2 - j1)
        if os.environ.get('SHOWR') and int(os.environ['SHOWR'], 16) == va + 4 * 64 * k:
            print('--', t, ours[i1:i2], '|', rom[j1:j2])
    print(os.path.basename(f), 'total', tot(per), ' '.join('%X:%d' % (va + 4 * 64 * k, v) for k, v in sorted(per.items())))
