#!/usr/bin/env python3
"""Move a matched function out of its address batch into its recorded module.

The counterpart to tools/filing.py: filing.py decides WHERE a function belongs
and records it; this moves the code there and proves the move did not break the
match.  Nothing here decides anything -- a function with no module in
config/filing.csv is skipped, never guessed at.

Each move is: extract the definition (and only the declarations it actually
uses) from the slice, append it to the module file, delete it from the slice,
then sweep BOTH files.  It is kept only if the function still matches in its
new home AND no sibling in the slice regressed; otherwise both files are
restored.  That is the same authority autofile.py uses -- the single-file sweep,
never a reading of the diff.

Deliberately one function per commit.  A batch move that breaks something
leaves no way to tell which move did it, and rule 7 wants every verified state
committed as it is reached.

    python3 tools/refile.py --dry-run       # what would move, and where
    python3 tools/refile.py --limit 5       # move five, verifying each
    python3 tools/refile.py --va 0x10012340 # one function
    python3 tools/refile.py --module menus  # only that module's backlog
"""
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import autofile                                              # noqa: E402
from filing import FILING, is_slice, load_report             # noqa: E402

ANCHOR = '#endif /* BR_MATCHING_BUILD */'


def neighbour_file(va, filed):
    """The module FILE of the nearest already-filed function on each side,
    when both agree.

    This is the destination rule that matters, and it is preferred over the
    name below.  MSVC 5 emits a translation unit contiguously, so a function
    bracketed by two filed ones from the same file came from that same original
    .c -- which is the thing we are trying to reconstruct.  Going by name
    instead would scatter one original TU across several new files and, worse,
    invent files named for a TECHNIQUE (br_nop.c, br_thunk.c, br_ret0.c) out of
    the little stubs, which src/core/README.md rules out: folders and files are
    named for a responsibility, never for how something is done.
    """
    import bisect
    ks = sorted(filed)
    i = bisect.bisect_left(ks, va)
    lo = filed.get(ks[i - 1]) if i > 0 else None
    hi = filed.get(ks[i]) if i < len(ks) else None
    return lo if (lo and lo == hi) else None


# Stems that name a TECHNIQUE or a shape, not a responsibility. src/core/
# README.md rules those out as file names, and for these the name carries no
# information anyway: a 5-byte BrNop_1002AB8F has no responsibility to file it
# under. Minting br_nop.c would satisfy the gate while making the architecture
# worse, so the mover refuses and leaves them for a person to place -- most
# belong in whatever module file their original TU neighbours end up in, which
# is knowable only once those neighbours are filed.
TECHNIQUE_STEMS = {'br_nop', 'br_ret0', 'br_ret1', 'br_ret', 'br_thunk',
                   'br_stub', 'br_misc', 'br_wrap', 'br_sub', 'br_ext',
                   'br_dispatch', 'br_fn', 'br_get', 'br_set'}


def dest_file(module, name):
    """Module file for `name`: an existing file whose basename matches the
    function's Br<Word> prefix, else a new br_<word>.c in that module."""
    m = re.match(r'Br([A-Za-z][a-z0-9]*)', name or '')
    stem = 'br_' + (m.group(1).lower() if m else 'misc')
    folder = os.path.join(ROOT, 'src', 'core', module)
    if os.path.isdir(folder):
        for f in sorted(os.listdir(folder)):
            if f == stem + '.c':
                return 'src/core/%s/%s' % (module, f)
        # A longer existing name that starts with the same stem (br_ui -> the
        # ui family) keeps related code together instead of minting br_ui.c.
        for f in sorted(os.listdir(folder)):
            if f.endswith('.c') and f[:-2].startswith(stem):
                return 'src/core/%s/%s' % (module, f)
    return 'src/core/%s/%s.c' % (module, stem)


def new_module_file(path, module, name):
    """Create a module file with a banner, per src/core/README.md."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    stem = os.path.basename(path)
    with open(path, 'w') as f:
        f.write('/* %s -- %s.\n'
                ' *\n'
                ' * Filed out of the address batches: these functions were\n'
                ' * matched first and grouped by what they are afterwards.\n'
                ' * Every function carries its original address.\n'
                ' */\n'
                '#ifdef BR_MATCHING_BUILD\n'
                '/* The original is /MD: CRT calls go through the import\n'
                ' * table (FF 15). */\n'
                '#define _CRTIMP __declspec(dllimport)\n'
                '#endif\n'
                '#include <stdint.h>\n'
                '\n'
                '#ifdef BR_MATCHING_BUILD\n'
                '\n'
                '%s\n' % (stem, module, ANCHOR))


def tag_block(tu, name, va):
    """The comment block that belongs to `name` -- its @implements tag, its
    WHAT IT DOES: description and any codegen notes -- as text ending in a
    newline, or None if no tag for this function is above it.

    Anchored on the DEFINITION, not on the first tag matching the VA: a file
    can carry the same address in a `@d3donly` note or in a sibling's residue
    comment, and moving the wrong block would strip a different function's
    documentation.
    """
    span = autofile.find_function(tu, name)
    if span is None:
        return None
    lines = tu[:span[0]].split('\n')
    i = len(lines) - 1
    while i >= 0 and not lines[i].strip():
        i -= 1
    # Skip declaration lines sitting between the tag and the definition, but
    # do NOT take them: they are usually shared with the functions staying
    # behind, and removing them regressed every sibling in the slice. The
    # destination gets its own copies from autofile.extract's harvest.
    while i >= 0 and lines[i].strip().endswith(';') \
            and not lines[i].strip().startswith('#'):
        i -= 1
    while i >= 0 and not lines[i].strip():
        i -= 1
    end = i + 1
    while i >= 0:
        s = lines[i].strip()
        if s.startswith(('*', '/*')) or s.endswith('*/') or not s:
            i -= 1
            continue
        break
    blk = [l for l in lines[i + 1:end]]
    if not any('@implements' in l for l in blk):
        return None
    return '\n'.join(blk) + '\n'


def strip_tag_block(tu, name, va):
    """`tu` with the function's comment block removed, so the tag does not
    stay behind in the source file claiming a function that has left."""
    blk = tag_block(tu, name, va)
    if not blk:
        return tu
    at = tu.rfind(blk, 0, autofile.find_function(tu, name)[0] + 1)
    return tu[:at] + tu[at + len(blk):] if at >= 0 else tu


STATIC_DEF = re.compile(
    r'^static\s+(?:const\s+)?[\w][\w\s]*?[\*\s]\s*\**(\w+)\s*(?:\[[^\]]*\])?\s*(?:=|;)',
    re.M)


def shared_statics(tu, body, name):
    """File-local statics this function reads that OTHER functions here use too.

    A file-scope `static` is private to its translation unit, so a function
    touching one cannot leave the file alone: take the static and the siblings
    break, leave it and the moved function will not compile. This is a property
    of the code, not a defect in the mover -- the correct unit of movement is
    the whole group that shares the static, which is what the ORIGINAL
    translation unit was. Reported so the group can be moved deliberately
    rather than the move failing with a bare compile error.
    """
    out = []
    for m in STATIC_DEF.finditer(tu):
        sym = m.group(1)
        if not re.search(r'\b%s\b' % re.escape(sym), body):
            continue
        # used anywhere outside this function's own text?
        rest = tu.replace(body, '')
        if re.search(r'\b%s\b' % re.escape(sym), rest):
            out.append(sym)
    return out


def git(*a):
    return subprocess.run(['git'] + list(a), cwd=ROOT,
                          capture_output=True, text=True)


def sweep(relfile):
    p = subprocess.run([sys.executable, os.path.join('tools', 'match_sweep.py'),
                        relfile], cwd=ROOT, capture_output=True, text=True,
                       timeout=900)
    return p.returncode == 0


def status_of(relfile):
    out = {}
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    if os.path.exists(rep):
        with open(rep) as f:
            for r in csv.DictReader(f):
                if r['file'] == relfile:
                    out[r['va'].lower()] = r['status']
    return out


def move_one(rec, size, dry, filed):
    va, name, module, src = (rec['glide_va'], rec['name'], rec['module'],
                             rec['file'])
    dst = neighbour_file(int(va, 16), filed) or dest_file(module, name)
    if not dst.startswith('src/core/%s/' % module):
        # A neighbour file in another module would contradict the recorded
        # decision; the recorded module wins.
        dst = dest_file(module, name)
    stem = os.path.basename(dst)[:-2]
    base = re.sub(r'[0-9a-f]+$', '', stem).rstrip('_') or stem
    if not os.path.exists(os.path.join(ROOT, dst)) and (
            stem in TECHNIQUE_STEMS or base in TECHNIQUE_STEMS):
        print('   NEEDS A DECISION %s %-28s (would mint %s)'
              % (va, name, os.path.basename(dst)))
        return False
    if dry:
        print('   %s %-30s %s -> %s' % (va, name, os.path.basename(src), dst))
        return True

    spath, dpath = os.path.join(ROOT, src), os.path.join(ROOT, dst)
    tu = open(spath).read()
    decls, body, _fp = autofile.extract(tu, name)
    if decls is None:
        print('   SKIP %s %s: %s' % (va, name, body))
        return False

    # autofile.extract() starts at the SIGNATURE -- it walks back only as far
    # as the previous ';', '}' or '*/'. That drops the comment block above the
    # function, and with it the @implements tag and the WHAT IT DOES: line.
    # A function moved without its tag arrives invisible: the sweep produces no
    # row for it in its new file, the verify below sees no match, and the move
    # is reverted as a failure when nothing was actually wrong. Carry the block.
    header = tag_block(tu, name, va)
    if header is None:
        print('   SKIP %s %s: no @implements block found above it' % (va, name))
        return False
    sh = shared_statics(tu, body, name)
    if sh:
        print('   SHARES A FILE-LOCAL STATIC %s %-24s (%s) -- move its group'
              % (va, name, ', '.join(sh[:3])))
        return False
    body = header + body

    before_src = status_of(src)
    src_before_text = tu
    dst_existed = os.path.exists(dpath)
    dst_before_text = open(dpath).read() if dst_existed else None
    if not dst_existed:
        new_module_file(dpath, module, name)

    dtu = open(dpath).read()
    if ANCHOR not in dtu:
        print('   SKIP %s %s: %s has no BR_MATCHING_BUILD anchor'
              % (va, name, dst))
        if not dst_existed:
            os.unlink(dpath)
        return False

    block = '\n'.join(decls + ['']) + body + '\n\n'
    at = dtu.rindex(ANCHOR)
    open(dpath, 'w').write(dtu[:at] + block + dtu[at:])
    # Remove from the slice only after the destination is written, so a crash
    # duplicates a function (caught by the sweep) rather than losing it.
    # Remove the body AND its comment block: a tag left behind would claim a
    # function that is no longer in the file, and the next sweep would score
    # the wrong source against that address.
    cut = strip_tag_block(src_before_text, name, va)
    span = autofile.find_function(cut, name)
    open(spath, 'w').write(cut[:span[0]] + cut[span[1]:])

    ok = sweep(dst) and sweep(src)
    after_dst = status_of(dst)
    after_src = status_of(src)
    good = (ok and after_dst.get(va.lower()) == 'match'
            and not [k for k, v in before_src.items()
                     if v == 'match' and k != va.lower()
                     and after_src.get(k) != 'match'])
    if not good:
        open(spath, 'w').write(src_before_text)
        if dst_existed:
            open(dpath, 'w').write(dst_before_text)
        else:
            os.unlink(dpath)
        sweep(src)
        if dst_existed:
            sweep(dst)
        print('   FAIL %s %s -> %s (reverted)' % (va, name, dst))
        return False

    git('add', src, dst)
    git('commit', '-q', '-m',
        '%s %s: file into %s\n\n'
        'Moved out of %s, which is an address batch, not a responsibility.\n'
        'Verified by single-file sweep on both files: still MATCH in its new\n'
        'home and no sibling in the batch regressed.'
        % (va, name, dst, os.path.basename(src)))
    print('   MOVED %s %-30s -> %s' % (va, name, dst))
    return True


def main():
    dry = '--dry-run' in sys.argv
    limit = int(sys.argv[sys.argv.index('--limit') + 1]) if '--limit' in sys.argv else 10**9
    only_va = sys.argv[sys.argv.index('--va') + 1].lower() if '--va' in sys.argv else None
    only_mod = sys.argv[sys.argv.index('--module') + 1] if '--module' in sys.argv else None

    with open(FILING) as f:
        rec = list(csv.DictReader(f))
    size = {int(r['va'], 16): int(r['orig_size'] or 0) for r in load_report()}
    # va -> the module FILE it already lives in, for the neighbour rule
    filed = {int(r['glide_va'], 16): r['file'] for r in rec
             if not is_slice(r['file']) and r['file'].startswith('src/core/')}

    todo = [r for r in rec if r['module'] and is_slice(r['file'])]
    if only_va:
        todo = [r for r in todo if r['glide_va'].lower() == only_va]
    if only_mod:
        todo = [r for r in todo if r['module'] == only_mod]
    # Smallest first: the cheapest moves prove the machinery before a 4 KB
    # function is at stake.
    todo.sort(key=lambda r: size.get(int(r['glide_va'], 16), 0))

    print('assigned but still in an address batch: %d' % len(todo))
    moved = 0
    for r in todo[:limit]:
        if move_one(r, size, dry, filed):
            moved += 1
    if not dry:
        print('\nmoved %d' % moved)
        subprocess.run([sys.executable, os.path.join('tools', 'filing.py')],
                       cwd=ROOT)


if __name__ == '__main__':
    main()
