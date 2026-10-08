#!/usr/bin/env python3
"""segsync.py -- carry the decomp's rewritten functions into the port, one
function at a time, for files a three-way merge garbles (functions reordered,
arms swapped, blocks moved: moved code merges cleanly without the port's
conversions).

    segsync.py rebuild BASE REL...   rewrite ports/tgrally/src/REL
    segsync.py diff REL NAME...      the port's old NAME (HEAD) against the file's
    segsync.py check BASE REL...     what the port had that the file lacks

A file is cut into segments: each function with the declarations above it,
keyed by the function's name, and the text after the last one.  rebuild
keeps the port's segment for every function the decomp left alone since
BASE (the commit in src/FORKED-FROM), takes the decomp's segment for every
one it changed, with the port's one-for-one line conversions (decomp at
BASE against the port at HEAD) applied and raw gbi word stores wrapped in
tgr_wr32, and keeps segments only the port has after their predecessor.
It prints the segments it took from the decomp: each needs a person
(segsync.py diff), for the conversions that are not one line for one line.

check prints the port's added lines (base -> port) the file no longer has,
and base lines the port changed that the file has verbatim: after a sync
each is either a line the decomp rewrote (and the rewrite was converted)
or a conversion that was lost.
"""
import difflib
import re
import subprocess
import sys

TREES = ('src/tgrally', 'n64/src')
FN = re.compile(r'[^(]*?\b(\w+)\s*\(')
KR = re.compile(r'^[A-Za-z_][^;]*?\b(\w+)\s*\([^;]*$')


def show(commit, path):
    r = subprocess.run(['git', 'show', '%s:%s' % (commit, path)], capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def decomp(commit, rel):
    for t in TREES:
        s = show(commit, '%s/%s' % (t, rel))
        if s is not None:
            return s
    return None


def segs(text):
    """[(name, text)]: each top-level function and the declarations above it"""
    out, cur, name, seen = [], [], None, {}
    for i, line in enumerate(text.split('\n')):
        cur.append(line)
        if line == '{' and name is None:
            sig = []
            for back in range(len(cur) - 2, -1, -1):
                t = cur[back]
                if t.strip() == '' or t.startswith(('/*', ' *', '#')) or t.rstrip().endswith((';', '*/')):
                    break
                sig.insert(0, t)
            m = FN.match(' '.join(sig))
            name = m.group(1) if m else None
            if name is None:                 # K&R: the parameter declarations end in ';'
                for back in range(len(cur) - 2, max(-1, len(cur) - 9), -1):
                    m = KR.match(cur[back])
                    if m:
                        name = m.group(1)
                        break
            if name is None:
                name = '?%d' % i
        if line == '}' and name is not None:
            seen[name] = seen.get(name, 0) + 1
            key = name if seen[name] == 1 else '%s#%d' % (name, seen[name])
            out.append((key, '\n'.join(cur)))
            cur, name = [], None
    out.append(('__tail__', '\n'.join(cur)))
    return out


def line_map(base, port):
    """the port's one-for-one replacements of base lines, stripped"""
    m, amb = {}, set()
    bl, pl = base.split('\n'), port.split('\n')
    for t, i1, i2, j1, j2 in difflib.SequenceMatcher(None, bl, pl, autojunk=False).get_opcodes():
        if t == 'replace' and i2 - i1 == j2 - j1:
            for a, b in zip(bl[i1:i2], pl[j1:j2]):
                k = a.strip()
                if k in m and m[k] != b.strip():
                    amb.add(k)
                m[k] = b.strip()
    return {k: v for k, v in m.items() if k not in amb}


def convert(text, m):
    out = []
    for line in text.split('\n'):
        k = line.strip()
        if k and k in m:
            line = line[:len(line) - len(line.lstrip())] + m[k]
        out.append(line)
    return re.sub(r'(\b\w+)->words\.(w[01]) = ([^;]*);', r'tgr_wr32(&\1->words.\2, \3);', '\n'.join(out))


def rebuild(base_c, rels):
    for rel in rels:
        base, head = decomp(base_c, rel), decomp('HEAD', rel)
        port = show('HEAD', 'ports/tgrally/src/' + rel)
        m = line_map(base, port)
        B, P = dict(segs(base)), dict(segs(port))
        hk, res, took = [], [], []
        for k, s in segs(head):
            hk.append(k)
            if B.get(k) == s and k in P:
                res.append(P[k])
            else:
                res.append(convert(s, m))
                took.append(k + ('' if k in P else ' (new)'))
        pk = [k for k, _ in segs(port)]
        kept = [k for k in pk if k not in hk]
        for k in kept:                       # a port-only segment: after its port predecessor
            i = pk.index(k)
            prev = next((pk[j] for j in range(i - 1, -1, -1) if pk[j] in hk), None)
            at = hk.index(prev) + 1 if prev else 0
            hk.insert(at, k)
            res.insert(at, P[k])
        open('ports/tgrally/src/' + rel, 'w').write('\n'.join(res))
        print('%-24s review: %s%s' % (rel, ' '.join(took) or '-',
                                      ('  kept port-only: ' + ' '.join(kept)) if kept else ''))


def diff(rel, names):
    old = dict(segs(show('HEAD', 'ports/tgrally/src/' + rel)))
    new = dict(segs(open('ports/tgrally/src/' + rel).read()))
    for name in names:
        print('###### %s %s' % (rel, name))
        for l in difflib.unified_diff(old.get(name, '').split('\n'), new.get(name, '').split('\n'),
                                      lineterm='', n=1):
            if l.startswith(('---', '+++')) or (l[:1] in '+-' and l[1:].lstrip().startswith(('*', '/*'))):
                continue
            print(l)


def check(base_c, rels):
    for rel in rels:
        base = decomp(base_c, rel).split('\n')
        port = show('HEAD', 'ports/tgrally/src/' + rel).split('\n')
        cur = open('ports/tgrally/src/' + rel).read().split('\n')
        cs, ps = {l.strip() for l in cur}, {l.strip() for l in port}
        out, removed = [], set()
        skip = lambda k: not k or k in ('{', '}') or k.startswith(('/*', '*', '//'))
        for t, i1, i2, j1, j2 in difflib.SequenceMatcher(None, base, port, autojunk=False).get_opcodes():
            if t in ('insert', 'replace'):
                out += ['  MISSING port:%d: %s' % (j + 1, port[j].strip()) for j in range(j1, j2)
                        if not skip(port[j].strip()) and port[j].strip() not in cs]
            if t in ('delete', 'replace'):
                removed |= {base[i].strip() for i in range(i1, i2)}
        out += ['  UNCONV %d: %s' % (n, l.strip()) for n, l in enumerate(cur, 1)
                if not skip(l.strip()) and l.strip() in removed and l.strip() not in ps]
        print('==== %s (%d)' % (rel, len(out)))
        if out:
            print('\n'.join(out))


def main():
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == 'rebuild':
        rebuild(a[1], a[2:])
    elif len(a) >= 3 and a[0] == 'diff':
        diff(a[1], a[2:])
    elif len(a) >= 3 and a[0] == 'check':
        check(a[1], a[2:])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
