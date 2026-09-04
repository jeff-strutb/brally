#!/usr/bin/env python3
"""Move a whole CONNECTED GROUP of matched functions into its module at once.

tools/refile.py moves one function at a time, and on the real tree that stalls
almost immediately: a slice's functions call each other and share file-local
statics, so pulling one out either leaves it referencing something it can no
longer see or strips a static its siblings still need. Measured on the backlog,
single moves cleared a handful; the rest reported "shares a file-local static"
or "references sibling definition".

That is not a defect to work around -- it is the shape of the code. MSVC 5
emitted one translation unit contiguously, so a group of functions bound by
shared statics and mutual calls IS an original .c file, and the whole group is
the smallest thing that can move. This tool moves that unit.

A group qualifies only when ALL of these hold:

  * every member has the same recorded module in config/filing.csv
  * every file-local static the group touches is used ONLY by the group --
    a static also read by a function staying behind means the group is not
    self-contained and is skipped, not forced
  * the destination is an existing module file, or a responsibility-named new
    one (never a technique name -- see refile.TECHNIQUE_STEMS)

Then: both files are swept, every moved row must still be `match`, and no
function left behind may regress. Otherwise both files are restored.

    python3 tools/refile_group.py --dry-run
    python3 tools/refile_group.py --limit 5
    python3 tools/refile_group.py --slice src/core/slice2_24.c
"""
import collections
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import autofile                                          # noqa: E402
import refile                                            # noqa: E402
from filing import FILING, is_slice                      # noqa: E402


def components(tu, rows):
    """Group rows that cannot be separated: mutual references or a shared
    file-local static. Returns [[row, ...], ...]."""
    names = [r['name'] for r in rows if r['name']]
    body = {}
    for n in names:
        sp = autofile.find_function(tu, n)
        body[n] = tu[sp[0]:sp[1]] if sp else ''
    parent = {n: n for n in names}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def uni(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for a in names:
        for b in names:
            if a != b and re.search(r'\b%s\b' % re.escape(b), body.get(a, '')):
                uni(a, b)
    for m in refile.STATIC_DEF.finditer(tu):
        s = m.group(1)
        users = [n for n in names
                 if re.search(r'\b%s\b' % re.escape(s), body.get(n, ''))]
        for n in users[1:]:
            uni(users[0], n)

    out = collections.defaultdict(list)
    for r in rows:
        if r['name']:
            out[find(r['name'])].append(r)
    return list(out.values()), body


def static_block(tu, sym):
    """The full definition text of file-local `sym`, initialiser included."""
    m = re.search(r'^static\s+(?:const\s+)?[\w][\w\s]*?[\*\s]\s*\**%s\s*'
                  r'(?:\[[^\]]*\])?\s*(?:=|;)' % re.escape(sym), tu, re.M)
    if not m:
        return None
    i = m.start()
    j = tu.find('=', m.start())
    if j < 0 or j > tu.find(';', m.start()):
        return tu[i:tu.find(';', i) + 1] + '\n'
    depth = 0
    k = j
    while k < len(tu):
        if tu[k] == '{':
            depth += 1
        elif tu[k] == '}':
            depth -= 1
        elif tu[k] == ';' and depth == 0:
            return tu[i:k + 1] + '\n'
        k += 1
    return None


def move_group(slc, grp, body, dry):
    tu = open(os.path.join(ROOT, slc)).read()
    module = grp[0]['module']
    names = [r['name'] for r in grp]

    dst = refile.dest_file(module, names[0])
    stem = os.path.basename(dst)[:-2]
    base = re.sub(r'[0-9a-f]+$', '', stem).rstrip('_') or stem
    if not os.path.exists(os.path.join(ROOT, dst)) and (
            stem in refile.TECHNIQUE_STEMS or base in refile.TECHNIQUE_STEMS):
        print('   NEEDS A DECISION  %-28s (would mint %s)'
              % (','.join(names)[:28], os.path.basename(dst)))
        return False

    # Statics the group touches, and whether anyone staying behind uses them.
    grp_text = '\n'.join(body[n] for n in names)
    rest = tu
    for n in names:
        rest = rest.replace(body[n], '')
    statics = []
    for m in refile.STATIC_DEF.finditer(tu):
        s = m.group(1)
        if not re.search(r'\b%s\b' % re.escape(s), grp_text):
            continue
        if re.search(r'\b%s\b' % re.escape(s), rest):
            print('   NOT SELF-CONTAINED  %-24s static %s is used by a '
                  'function staying behind' % (','.join(names)[:24], s))
            return False
        statics.append(s)

    # Anything the group calls that is DEFINED in the slice and not moving.
    for n in names:
        for od in set(re.findall(r'\b(\w+)\s*\([^;{)]*\)\s*\n?\s*\{', rest)):
            if od in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                continue
            # A member of the group is not "staying behind". It can appear in
            # `rest` when its body could not be delimited, which would other-
            # wise report a function as calling itself.
            if od in names:
                continue
            if re.search(r'\b%s\b' % re.escape(od), body[n]):
                print('   NOT SELF-CONTAINED  %-24s calls %s, which stays'
                      % (','.join(names)[:24], od))
                return False

    if dry:
        print('   %-46s %d fn%s + %d static%s -> %s'
              % (','.join(names)[:46], len(names), '' if len(names) == 1 else 's',
                 len(statics), '' if len(statics) == 1 else 's', dst))
        return True

    dpath = os.path.join(ROOT, dst)
    spath = os.path.join(ROOT, slc)
    src_before, dst_existed = tu, os.path.exists(dpath)
    dst_before = open(dpath).read() if dst_existed else None
    before_src = refile.status_of(slc)
    if not dst_existed:
        refile.new_module_file(dpath, module, names[0])

    blocks, decls = [], []
    for s in statics:
        b = static_block(tu, s)
        if b is None:
            print('   SKIP %s: could not delimit static %s' % (names[0], s))
            if not dst_existed:
                os.unlink(dpath)
            return False
        blocks.append(b)
    cut = tu
    for s, b in zip(statics, blocks):
        cut = cut.replace(b, '', 1)
    for n in names:
        d, bd, _ = autofile.extract(tu, n)
        if d:
            decls += [x for x in d if x not in decls]
        hdr = refile.tag_block(cut, n, '')
        blk = (hdr or '') + body[n]
        blocks.append(blk)
        cut = refile.strip_tag_block(cut, n, '')
        sp = autofile.find_function(cut, n)
        if sp is None:
            # The definition could not be re-located after earlier members were
            # cut out (nested or macro-wrapped bodies do this). Abandon the
            # whole group rather than write a half-moved file.
            print('   SKIP %s: cannot re-locate %s after cutting its group'
                  % (names[0], n))
            open(spath, 'w').write(src_before)
            if dst_existed:
                open(dpath, 'w').write(dst_before)
            elif os.path.exists(dpath):
                os.unlink(dpath)
            return False
        cut = cut[:sp[0]] + cut[sp[1]:]

    dtu = open(dpath).read()
    if refile.ANCHOR not in dtu:
        print('   SKIP %s: %s has no BR_MATCHING_BUILD anchor' % (names[0], dst))
        if not dst_existed:
            os.unlink(dpath)
        return False
    at = dtu.rindex(refile.ANCHOR)
    add = ('\n'.join(decls + ['']) if decls else '') + '\n'.join(blocks) + '\n\n'
    open(dpath, 'w').write(dtu[:at] + add + dtu[at:])
    open(spath, 'w').write(cut)

    refile.sweep(dst)
    refile.sweep(slc)
    ad, asrc = refile.status_of(dst), refile.status_of(slc)
    moved_ok = all(ad.get(r['glide_va'].lower()) == 'match' for r in grp)
    regress = [k for k, v in before_src.items()
               if v == 'match' and k not in {r['glide_va'].lower() for r in grp}
               and asrc.get(k) != 'match']
    if not (moved_ok and not regress):
        open(spath, 'w').write(src_before)
        if dst_existed:
            open(dpath, 'w').write(dst_before)
        else:
            os.unlink(dpath)
        refile.sweep(slc)
        if dst_existed:
            refile.sweep(dst)
        why = 'a moved function no longer matches' if not moved_ok \
            else '%d sibling(s) regressed' % len(regress)
        print('   FAIL %-40s -> %s (%s, reverted)'
              % (','.join(names)[:40], dst, why))
        return False

    refile.git('add', slc, dst)
    refile.git('commit', '-q', '-m',
               '%s: file %d function%s into %s\n\n'
               'Moved as a GROUP, which is the smallest unit that can move: '
               'these\nfunctions call each other or share a file-local static, '
               'so pulling\none out alone breaks the rest. That grouping is the '
               'original\ntranslation unit -- MSVC 5 emitted one .c '
               'contiguously.\n\n%s\nVerified by sweeping both files: every '
               'moved function still MATCHes and\nno function left behind '
               'regressed.'
               % (os.path.basename(slc), len(names),
                  '' if len(names) == 1 else 's', dst,
                  '\n'.join('  %s %s' % (r['glide_va'], r['name']) for r in grp)))
    print('   MOVED %-44s -> %s' % (','.join(names)[:44], dst))
    return True


def main():
    dry = '--dry-run' in sys.argv
    lim = int(sys.argv[sys.argv.index('--limit') + 1]) if '--limit' in sys.argv else 10**9
    only = sys.argv[sys.argv.index('--slice') + 1] if '--slice' in sys.argv else None

    rows = [r for r in csv.DictReader(open(FILING)) if is_slice(r['file'])]
    byslice = collections.defaultdict(list)
    for r in rows:
        byslice[r['file']].append(r)

    done = 0
    for slc in sorted(byslice):
        if only and slc != only:
            continue
        tu = open(os.path.join(ROOT, slc)).read()
        comps, body = components(tu, byslice[slc])
        for grp in comps:
            if done >= lim:
                break
            mods = {r['module'] for r in grp}
            if len(mods) != 1 or not list(mods)[0]:
                continue
            if move_group(slc, grp, body, dry):
                done += 1
                if not dry:
                    tu = open(os.path.join(ROOT, slc)).read()
                    comps2, body = components(tu, byslice[slc])
    print('\n%s %d group(s)' % ('would move' if dry else 'moved', done))
    if not dry:
        subprocess.run([sys.executable, os.path.join('tools', 'filing.py')],
                       cwd=ROOT)


if __name__ == '__main__':
    main()
