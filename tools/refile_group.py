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
from refile import ANCHOR                                # noqa: E402
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


def seed_module_file(path, module, slc, tu):
    """Create a module file carrying the source slice's preamble.

    Everything above the first @implements tag is the compiler's view of that
    translation unit -- the #includes, the #ifdef BR_MATCHING_BUILD block, the
    typedefs and struct definitions the matched bodies were compiled against.
    Copy it verbatim, then open a matching-build section for the functions to
    land in. Unused declarations are harmless; a MISSING one changes codegen or
    breaks the compile, and a changed struct layout silently breaks the match.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    m = re.search(r'/\*[^*]*@implements', tu)
    pre = tu[:m.start()] if m else ''
    # Drop a trailing '#endif' that closed the slice's own matching section --
    # we re-open one below and would otherwise unbalance the file.
    pre = re.sub(r'#endif[^\n]*\n\s*$', '', pre)
    with open(path, 'w') as f:
        f.write('/* %s -- %s.\n'
                ' *\n'
                ' * Filed out of %s. The preamble below is that file\'s,\n'
                ' * copied verbatim: these bodies are byte-exact only under the\n'
                ' * view the compiler had of them -- same includes, same\n'
                ' * typedefs, same struct layouts. Do not trim it without\n'
                ' * re-sweeping.\n'
                ' */\n%s\n#ifdef BR_MATCHING_BUILD\n\n%s\n'
                % (os.path.basename(path), module, os.path.basename(slc),
                   pre, ANCHOR))


def move_group(slc, grp, body, dry, defer=None):
    """Move one group. With `defer` (a {dest: snapshot} dict) the edits are
    written and the VERIFY IS SKIPPED, so a caller can move every group in a
    slice and then sweep each file once instead of once per group. The verify
    itself is unchanged -- only how often it runs."""
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
    if defer is not None and dst not in defer:
        defer[dst] = dst_before          # None means "did not exist"
    if not dst_existed:
        # Seed a NEW module file from the SLICE'S OWN preamble, not a minimal
        # stub. A byte-exact function is only byte-exact under the view the
        # compiler had of it: the same includes, typedefs and struct layouts,
        # and the same optimisation variant. A fresh file with just <stdint.h>
        # gives it none of that, so every move into a new file failed its
        # verify -- which is what stalled this tool, not the filing.
        seed_module_file(dpath, module, slc, tu)

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

    if defer is not None:
        for r in grp:
            r['_dest'] = dst
        return True

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


def move_slice(slc, rows, dry):
    """Move every qualifying group in one slice, then verify ONCE.

    The per-group verify was this tool's entire cost: two full-file compiles
    and diffs, ~25 s, for every group -- and there are hundreds. Deciding where
    a function belongs takes no time at all; PROVING the move did not break the
    byte match is what takes it, so the fix is to prove it once per file
    instead of once per group.

    The check is not weakened. After the writes, every moved function must
    still be `match` in its new file and nothing left behind may regress -- the
    same two conditions, asked once. What batching gives up is ISOLATION: a
    failure does not say which group caused it, so the slice is restored whole
    and retried one group at a time. Slices that are fine, which is most of
    them, pay one sweep per file instead of one per group.
    """
    spath = os.path.join(ROOT, slc)
    tu = open(spath).read()
    comps, body = components(tu, rows)
    plan = [g for g in comps
            if len({r['module'] for r in g}) == 1 and g[0]['module']]
    if not plan:
        return 0
    if dry:
        for g in plan:
            move_group(slc, g, body, True)
        return len(plan)

    before_src = refile.status_of(slc)
    src_before = tu
    snaps, moved = {}, []
    for grp in plan:
        if move_group(slc, grp, body, False, defer=snaps):
            moved += grp
            tu = open(spath).read()
            comps, body = components(tu, rows)
    if not moved:
        return 0

    files = sorted(snaps)
    for f in files:
        refile.sweep(f)
    refile.sweep(slc)

    ok = True
    for r in moved:
        st = refile.status_of(r['_dest'])
        if st.get(r['glide_va'].lower()) != 'match':
            ok = False
            break
    asrc = refile.status_of(slc)
    movedv = {r['glide_va'].lower() for r in moved}
    regress = [k for k, v in before_src.items()
               if v == 'match' and k not in movedv and asrc.get(k) != 'match']

    if ok and not regress:
        refile.git('add', slc, *files)
        refile.git('commit', '-q', '-m',
                   '%s: file %d function%s into %d module file%s\n\n'
                   'Moved as connected GROUPS -- functions that call each other '
                   'or share a\nfile-local static cannot be separated, and that '
                   'grouping is the\noriginal translation unit.\n\n%s\n\n'
                   'Verified after the batch: every moved function still MATCHes '
                   'in its new\nfile and nothing left behind regressed.'
                   % (os.path.basename(slc), len(moved),
                      '' if len(moved) == 1 else 's', len(files),
                      '' if len(files) == 1 else 's',
                      '\n'.join('  %s %-32s -> %s' % (r['glide_va'], r['name'],
                                                      r['_dest']) for r in moved)))
        print('   %-22s %3d fn -> %s' % (os.path.basename(slc), len(moved),
                                         ', '.join(os.path.basename(f) for f in files)))
        return len(moved)

    open(spath, 'w').write(src_before)
    for f, snap in snaps.items():
        fp = os.path.join(ROOT, f)
        if snap is None:
            if os.path.exists(fp):
                os.unlink(fp)
        else:
            open(fp, 'w').write(snap)
    refile.sweep(slc)
    for f in files:
        if os.path.exists(os.path.join(ROOT, f)):
            refile.sweep(f)
    why = 'a moved function no longer matches' if not ok \
        else '%d left behind regressed' % len(regress)
    print('   %-22s BATCH FAILED (%s) -- restored, retrying per group'
          % (os.path.basename(slc), why))
    n = 0
    tu = open(spath).read()
    comps, body = components(tu, rows)
    for grp in comps:
        if len({r['module'] for r in grp}) == 1 and grp[0]['module']:
            if move_group(slc, grp, body, False):
                n += len(grp)
                tu = open(spath).read()
                comps2, body = components(tu, rows)
    return n


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
        if done >= lim:
            break
        done += max(0, move_slice(slc, byslice[slc], dry))
    print('\n%s %d function(s)' % ('would move' if dry else 'moved', done))
    if not dry:
        subprocess.run([sys.executable, os.path.join('tools', 'filing.py')],
                       cwd=ROOT)


if __name__ == '__main__':
    main()
