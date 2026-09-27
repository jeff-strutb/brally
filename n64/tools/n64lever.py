"""Try source-truth levers on the T2 functions already in n64/src.

    .venv/bin/python n64/tools/n64lever.py                 # every T2, every lever
    .venv/bin/python n64/tools/n64lever.py --lever fsuf     # one lever
    .venv/bin/python n64/tools/n64lever.py 0x802238B8       # one function

A lever is a whole-function rewrite that the ROM, not taste, decides -- the
Ghidra draft said one thing and the bytes may say another.  Unlike the
permuter's respellings these can change meaning (`0.0` vs `0.0f`), so a lever
is kept only when the function gets strictly closer to the ROM AND no other
function in the file gets further away (CLAUDE.md rule 6: surroundings decide
codegen).  The file is rebuilt with n64build.py either way, so verify.csv
stays current.

Levers:
  fsuf   float literals get an `f` (Ghidra prints single constants as double)
"""
import argparse
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402
import n64gen as G  # noqa: E402
import n64t3 as T  # noqa: E402

LEVERS = {'fsuf': G.fsuffix}


def grade_file(path):
    """-> {va: ndiff} for every tagged function in path (0 = EXACT)."""
    out = subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), path,
                          '--csv', os.devnull], cwd=ROOT, capture_output=True, text=True).stdout
    res = {}
    for line in out.split('\n'):
        p = line.split()
        if len(p) >= 3 and len(p[0]) == 8 and p[1] in ('EXACT', 'DIFF', 'CCFAIL', 'ERROR'):
            try:
                va = int(p[0], 16)
            except ValueError:
                continue
            res[va] = 0 if p[1] == 'EXACT' else (int(p[2]) if p[2].isdigit() else 10 ** 6)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('vas', nargs='*')
    ap.add_argument('--lever', action='append', choices=sorted(LEVERS))
    a = ap.parse_args()
    levers = a.lever or sorted(LEVERS)
    if a.vas:
        vas = [int(v, 16) for v in a.vas]
    else:
        vas = [int(r['va'], 16) for r in csv.DictReader(open(os.path.join(B.OUT, 'verify.csv')))
               if r['va'] and r['status'] == 'DIFF']
    touched = set()
    for va in vas:
        path, name, _ = T.source_of(va)
        if not path:
            continue
        for lv in levers:
            src = open(path).read()
            body = T.function_text(src, name)
            new = LEVERS[lv](body)
            if new == body:
                continue
            before = grade_file(path)
            open(path, 'w').write(src.replace(body, new))
            after = grade_file(path)
            worse = [v for v in before if after.get(v, 10 ** 6) > before[v]]
            if after.get(va, 10 ** 6) < before.get(va, 10 ** 6) and not worse:
                print('%08X %-28s %-5s %d -> %d%s' % (va, name, lv, before[va], after[va],
                                                   '  EXACT' if after[va] == 0 else ''))
                touched.add(path)
            else:
                open(path, 'w').write(src)
    for path in sorted(touched):
        subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), path, '-q'], cwd=ROOT)


if __name__ == '__main__':
    main()
