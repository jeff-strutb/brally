#!/usr/bin/env python3
"""Turn 32-bit byte views of shared records into named fields.

The alias pass left accesses like

    (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */

whose byte offset is the ORIGINAL's.  On 64-bit the record is laid out
again, so the offset is wrong wherever a pointer comes before it.  This
looks the offset up in the record's i386 layout (clang's, from a file that
sees the record) and rewrites the access to the field at that offset:

    (*(int *)&g_brRaceRules.f0C)

An offset inside an array of records indexes the array.  Offsets that do
not land on a field start are left alone and reported.

Usage: bytefix.py [--dry] [FILE...]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT
VIEW = re.compile(r'\(\*\((?P<t>[^()]*?(?:\(\*\)\[\d*\])?[^()]*?)\)\(\(char \*\)&(?P<obj>\w+) \+ 0x(?P<off>[0-9A-Fa-f]+)\)\) /\* BR_LP64_BYTE_VIEW \*/')


def decls():
    """object name -> (type, array dims) from the shared headers."""
    out = {}
    for dp, _, fs in os.walk(os.path.join(ROOT, 'ports/64b/include')):
        for f in fs:
            s = open(os.path.join(dp, f), encoding='latin-1').read()
            for m in re.finditer(r'^extern\s+(?:const\s+)?(?:struct\s+)?(\w+)\s+(\w+)((?:\[\w*\])*)\s*;', s, re.M):
                out[m.group(2)] = (m.group(1), re.findall(r'\[(\w*)\]', m.group(3)), f)
    return out


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        for dp, _, fs in os.walk('ports/64b/src/core'):
            files += [os.path.join(dp, f) for f in fs if f.endswith(('.c', '.cpp'))]
    D = decls()
    hits = []
    for f in files:
        s = open(f, encoding='latin-1').read()
        for m in VIEW.finditer(s):
            hits.append((f, m))
    objs = sorted({m.group('obj') for _, m in hits})
    # one probe TU that sees every record involved
    hdrs = sorted({D[o][2] for o in objs if o in D})
    probe = os.path.join(ROOT, 'build/portable/bytefix_probe.c')
    with open(probe, 'w') as fh:
        for h in hdrs:
            fh.write('#include "%s"\n' % h)
        for o in objs:
            if o in D:
                fh.write('void *bytefix_%s = &%s;\n' % (o, o))
    vm.load_ptr_typedefs(probe, [])
    recs, sizes = vm.layouts(probe, [])
    done, left = 0, []
    for f in files:
        s = open(f, encoding='latin-1').read()

        def fix(m):
            nonlocal done
            obj, off, t = m.group('obj'), int(m.group('off'), 16), m.group('t').strip()
            if obj not in D:
                left.append('%s: %s+0x%X (no declaration)' % (f, obj, off))
                return m.group(0)
            rt, dims, _ = D[obj]
            if rt not in recs and dims and rt in vm.PTR_TYPEDEFS and off % 4 == 0:
                done += 1             # a table of pointers: 4-byte slots in the original
                return '(*(%s)&%s[%d])' % (t, obj, off // 4)
            if rt not in recs:
                left.append('%s: %s+0x%X (no layout for %s)' % (f, obj, off, rt))
                return m.group(0)
            base = obj
            if dims:
                esz = sizes.get(rt)
                i, off = divmod(off, esz)
                base = '%s[%d]' % (obj, i)
            # the field the view's type asks for: a pointer to t
            am = re.match(r'^(.*?)\s*\(\*\)\[(\d+)\]$', t)
            if am:
                es = vm.tsize(am.group(1), sizes)
                want = es * int(am.group(2)) if es else None
            elif t.endswith('*'):
                want = vm.tsize(re.sub(r'\s*\*$', '', t), sizes)
            else:
                want = None
            r = vm.resolve(recs, sizes, rt, off, want)
            if r is None or r[2]:
                left.append('%s: %s+0x%X (not a field start)' % (f, obj, off))
                return m.group(0)
            done += 1
            return '(*(%s)&%s.%s)' % (t, base, r[0])
        n = VIEW.sub(fix, s)
        if n != s and not dry:
            open(f, 'w', encoding='latin-1').write(n)
    print('byte views rewritten %d, left %d' % (done, len(left)))
    for l in left:
        print('  ' + l)


if __name__ == '__main__':
    main()
