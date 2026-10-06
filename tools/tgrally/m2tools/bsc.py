"""bsc.py VA draft.c [draft2.c ...] [--hunks N] : blind structural score.

Registers, sp offsets and branch targets masked; prologue/epilogue saves of
callee-saved registers dropped.  Prints the count of non-equal ops (larger
side of each hunk) and, with --hunks N, the first N hunks with ROM addresses."""
import difflib, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

args = sys.argv[1:]
nh = 0
regions = None
if '--regions' in args:
    i = args.index('--regions'); regions = [tuple(x.split(':')) for x in args[i + 1].split(',')]; del args[i:i + 2]
if '--hunks' in args:
    i = args.index('--hunks'); nh = int(args[i + 1]); del args[i:i + 2]
va, files = args[0], args[1:]
DV = os.environ.get("DVTOOL", os.path.join(os.path.dirname(__file__), "dv.py"))
SAVE = re.compile(r'^(sw|lw|sdc1|ldc1)\s+\$(s\d|fp|ra|f2\d|f3\d), 0x[0-9a-f]+\(\$sp\)')


def norm(x):
    x = re.sub(r'-?0x[0-9a-f]+\(\$sp\)', 'S', x)
    x = re.sub(r'(addiu\s+\$\w+, \$sp, )-?0x[0-9a-f]+', r'\1K', x)
    x = re.sub(r'\$(sp)', 'SP', x)
    x = re.sub(r'\$f\d+', 'F', x)
    x = re.sub(r'^(lui\s+\S+, )0x[0-9a-f]+$|^(lui\s+\S+, )0$', lambda m: (m.group(1) or m.group(2)) + 'H', x)
    x = re.sub(r'-?0x[0-9a-f]+\((\$(?!sp)\w+)\)|\b\d+\((\$(?!sp)\w+)\)', lambda m: 'O(' + (m.group(1) or m.group(2)) + ')', x)
    x = re.sub(r'(?<![\w)])\((\$(?!sp)\w+)\)', r'O(\1)', x)
    x = re.sub(r'\$[a-z][a-z0-9]', 'R', x)
    x = re.sub(r'(\bb\w*\s.*)0x[0-9a-f]+$', r'\1L', x)
    x = re.sub(r'^(addiu\s+SP, SP, )\S+', r'\1N', x)
    return ' '.join(x.split())


def listing(f):
    out = subprocess.run([sys.executable, DV, va, f], capture_output=True, text=True).stdout.splitlines()[1:]
    o, r = [], []
    for l in out:
        if '|' not in l:
            continue
        a, b = l.split('|', 1)
        a = a[12:].strip(); b = b.strip()
        if a and not SAVE.match(a):
            o.append(a)
        if b:
            addr, ins = l[:8], b
            if not SAVE.match(ins.strip()):
                r.append((addr, ins.strip()))
    return o, r


def score(f):
    o, r = listing(f)
    if not o:
        return f, None, []
    sm = difflib.SequenceMatcher(None, [norm(x) for x in o], [norm(y) for _, y in r], autojunk=False)
    n, hunks = 0, []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != 'equal':
            n += max(i2 - i1, j2 - j1)
            hunks.append((tag, o[i1:i2], r[j1:j2]))
    return f, n, hunks


def main():
    rom_len = None
    with ThreadPoolExecutor(8) as ex:
        for f, n, hunks in ex.map(score, files):
            print(f'{os.path.basename(f)} blind={n}')
            if regions:
                cnt = {}
                for tag, a, b in hunks:
                    pos = b[0][0] if b and b[0][0] else None
                    key = '?'
                    if pos:
                        for name, lo in regions:
                            if int(pos, 16) >= int(lo, 16):
                                key = name
                    cnt[key] = cnt.get(key, 0) + max(len(a), len(b))
                print('  regions:', cnt)
            for tag, a, b in hunks[:nh]:
                at = b[0][0] if b else '?'
                print(f'--- {tag} @{at}')
                for k in range(max(len(a), len(b))):
                    x = a[k] if k < len(a) else ''
                    y = (b[k][0] + ' ' + b[k][1]) if k < len(b) else ''
                    print(f'  {x:40s} | {y}')


main()
