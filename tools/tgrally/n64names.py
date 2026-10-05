"""Replace address names (func_80XXXXXX, D_80XXXXXX) with real names in src/tgrally.

    .venv/bin/python tools/tgrally/n64names.py            # every file
    .venv/bin/python tools/tgrally/n64names.py FILE...

Once a function or variable is named in config/tgrally/symbols_tgr.csv, every
source that still calls it by address is rewritten to use the name, and any
declaration that becomes a duplicate is dropped.  Names never change code:
the build resolves both spellings to the same address.  The files touched are
rebuilt and the rewrite is undone if anything stops being exact.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))
import n64build as B  # noqa: E402


def rename_text(src, byva):
    def sub(m):
        va = int(m.group(2), 16)
        return byva.get(va, m.group(0))
    out = re.sub(r'\b(func|D)_([0-9A-F]{8})\b', sub, src)
    # drop repeated declaration lines inside the declaration block
    DB, DE = '/* -- declarations -- */', '/* -- end declarations -- */'
    if DB in out and DE in out:
        pre, _, rest = out.partition(DB)
        have, _, post = rest.partition(DE)
        seen, keep = set(), []
        for l in have.split('\n'):
            key = l.strip()
            if not key:
                continue
            m = re.search(r'\b(\w+)\s*(\(|;|\[)', key)
            ident = m.group(1) if m else key
            if ident in seen:
                continue
            seen.add(ident)
            keep.append(l)
        out = pre + DB + '\n' + '\n'.join(keep) + '\n' + DE + post
    return out


def status(files):
    p = subprocess.run([sys.executable, os.path.join(ROOT, 'tools/tgrally/n64build.py')] + files,
                       cwd=ROOT, capture_output=True, text=True)
    return [l.split()[:2] for l in p.stdout.split('\n') if re.match(r'^[0-9A-F]{8} ', l)]


def main():
    syms = B.load_symbols()
    byva = {}
    for name, va in syms.items():
        byva.setdefault(va, name)
    files = [os.path.abspath(f) for f in sys.argv[1:]] or B.all_sources()
    for f in files:
        old = open(f).read()
        new = rename_text(old, byva)
        if new == old:
            continue
        before = status([f])
        open(f, 'w').write(new)
        after = status([f])
        if sorted(before) != sorted(after):
            open(f, 'w').write(old)
            status([f])
            print('REVERTED', os.path.relpath(f, ROOT))
        else:
            print('renamed ', os.path.relpath(f, ROOT))


if __name__ == '__main__':
    main()
