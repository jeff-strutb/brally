#!/usr/bin/env python3
"""Ask the SOLVED corpus which C produced a given original instruction pattern.

WHY THIS EXISTS.  Every stuck function in this project fails the same way:
the original keeps a value in a register where we spill it, or homes it to a
slot where we forward it.  That reads like an allocator whim, but VC5's
allocator is deterministic -- identical source gives identical output -- so
the difference is always IN THE SOURCE.  We just do not know which construct
produces which allocation.

And we own ~860 worked examples of exactly that.  Every byte-exact function
is a PROOF that "this C produced these bytes", spills, slot layout and
scheduling included.  Until now that corpus was write-only: it was counted,
never queried.  CLAUDE.md and docs/VC5-IDIOMS.md both say the right thing --
"before writing 'the compiler will not do X', find a site in the same binary
doing it" -- and everybody did it by hand, one site at a time.  This makes it
a query.

It turns the central question from "invent a spelling", which is measured
dead (permuter 0/95, refine batch 0/258), into "look up who already solved
this".  And it COMPOUNDS: every new match anywhere makes it better at the
functions that are stuck.

    .venv/bin/python tools/corpus.py build
    .venv/bin/python tools/corpus.py find --from 0x1000A110 --at 0x2f0 --len 8
    .venv/bin/python tools/corpus.py find --pattern 'mov byte ptr [esp+S], B; mov R, dword ptr [esp+S]; and R, 0xff'
    .venv/bin/python tools/corpus.py show --va 0x10012345 --at 0x40 --len 8

‼ THE INDEX IS OVER **ORIGINAL** BYTES, NOT OURS.  A corpus member is
byte-exact by definition, so an offset in the original is the same offset in
our object -- which is what lets `show` map a hit back to real source lines
through the compiler's own /FAcs listing.  Never index recompiled bytes: a
diffing function would poison the corpus with spellings that are WRONG.

Normalisation is msetdiff's `norm`, so a pattern copied out of msetdiff or
divergence.py output means the same thing here: registers, esp
displacements, reloc'd/absolute operands and branch targets are all blind.
Small immediates are KEPT -- `and R,0xff` and `and R,0xf` are different
questions.
"""
import argparse
import collections
import csv
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
import msetdiff  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
INDEX = os.path.join(ROOT, 'build', 'match', 'corpus_index.json')
WINE = os.path.join(ROOT, 'tools', 'wine.sh')
MSVC_DIR = os.environ.get('BR_MSVC', os.path.join(ROOT, 'tools', 'msvc5'))
if not os.path.isabs(MSVC_DIR):
    MSVC_DIR = os.path.join(ROOT, MSVC_DIR)

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True


# ---------------------------------------------------------------- corpus ---
def reports():
    """Every report the sweep writes.  The C++ and EXE lanes carry byte-exact
    functions too and they are just as good as evidence; skipping them threw
    away a third of the corpus in the first cut of this tool."""
    for name in ('report.csv', 'report_cpp.csv', 'report_exe.csv'):
        p = os.path.join(ROOT, 'build', 'match', name)
        if os.path.exists(p):
            yield p


def matched_rows():
    """(va, name, file) for every function the sweep calls byte-exact."""
    seen = {}
    for p in reports():
        with open(p, newline='') as f:
            for r in csv.DictReader(f):
                # The EXE report carries an extra leading column, so address
                # the fields by name and tolerate the ones that are absent.
                va = (r.get('va') or '').strip()
                st = (r.get('status') or '').strip().lower()
                if st != 'match' or not va.startswith('0x'):
                    continue
                seen[va.lower()] = (va, (r.get('name') or '').strip(),
                                    (r.get('file') or '').strip())
    return list(seen.values())


def orig_path(va):
    for cand in ('%s.bin' % va.upper().replace('0X', '0x'),
                 '%s.bin' % va.lower(),
                 '0x%08X.bin' % int(va, 16),
                 '0x%08x.bin' % int(va, 16)):
        p = os.path.join(ORIG_DIR, cand)
        if os.path.exists(p):
            return p
    return None


def tokenise(data):
    """Normalised token stream for a blob of ORIGINAL (linked) bytes.

    Returns (tokens, offsets) so a hit can be reported at a byte offset the
    other tools agree with.  Trailing alignment padding is dropped, exactly
    as msetdiff does -- otherwise every function ends with the same `int3`
    run and short patterns match everywhere."""
    ins = list(md.disasm(bytes(data), 0))
    while ins and ins[-1].mnemonic in ('nop', 'int3'):
        ins.pop()
    toks, offs = [], []
    for i in ins:
        # Original bytes are already linked, so there is no relocation list;
        # norm's own absolute-address heuristic is what applies here.
        toks.append(msetdiff.norm(i, False))
        offs.append(i.address)
    return toks, offs


def build_index(verbose=True):
    rows = matched_rows()
    out, skipped = [], 0
    for va, name, src in rows:
        p = orig_path(va)
        if not p:
            skipped += 1
            continue
        with open(p, 'rb') as f:
            toks, offs = tokenise(f.read())
        if not toks:
            skipped += 1
            continue
        out.append({'va': va, 'name': name, 'file': src,
                    'toks': toks, 'offs': offs})
    os.makedirs(os.path.dirname(INDEX), exist_ok=True)
    with open(INDEX, 'w') as f:
        json.dump({'fns': out}, f)
    if verbose:
        ni = sum(len(e['toks']) for e in out)
        print('corpus: %d byte-exact functions, %d instructions indexed'
              % (len(out), ni))
        if skipped:
            print('        %d skipped (no extracted original bytes)' % skipped)
        print('written: %s' % os.path.relpath(INDEX, ROOT))
    return out


FN_NAME = {}


def load_index():
    if not os.path.exists(INDEX):
        sys.exit('no index -- run: .venv/bin/python tools/corpus.py build')
    with open(INDEX) as f:
        fns = json.load(f)['fns']
    FN_NAME.update({e['va'].lower(): e['name'] for e in fns})
    return fns


# ----------------------------------------------------------------- query ---
def pattern_from_function(va, at, length):
    """Pull a normalised pattern straight out of a function's ORIGINAL bytes.

    This is the real workflow: divergence.py hands you an offset, and you ask
    the corpus who else has these bytes.  The function does NOT have to be
    byte-exact -- it is the ORIGINAL side being read, which is true whatever
    our recompile does."""
    p = orig_path(va)
    if not p:
        sys.exit('no extracted original bytes for %s' % va)
    with open(p, 'rb') as f:
        toks, offs = tokenise(f.read())
    start = 0
    if at is not None:
        cands = [k for k, o in enumerate(offs) if o >= at]
        if not cands:
            sys.exit('offset 0x%x is past the end of %s' % (at, va))
        start = cands[0]
    return toks[start:start + length]


def parse_pattern(text):
    return [t.strip() for t in text.split(';') if t.strip()]


def search(index, pat, exclude=None, max_hits=40):
    """Every corpus position whose token run equals `pat`."""
    n = len(pat)
    hits = []
    for e in index:
        if exclude and e['va'].lower() == exclude.lower():
            continue
        toks = e['toks']
        for k in range(len(toks) - n + 1):
            if toks[k:k + n] == pat:
                hits.append((e, k))
                if len(hits) >= max_hits:
                    return hits
    return hits


def search_longest(index, pat, exclude=None, min_len=3, max_hits=40):
    """Longest prefix of `pat` that any corpus member contains.

    A full pattern usually misses -- that is why the function is stuck. What
    is useful is the longest run that DOES appear, because that is the
    boundary between construct the corpus can explain and the part that is
    genuinely new."""
    for n in range(len(pat), min_len - 1, -1):
        hits = search(index, pat[:n], exclude, max_hits)
        if hits:
            return pat[:n], hits
    return None, []


# ---------------------------------------------------------------- source ---
def cod_lines(src, va, at, length):
    """Compile `src` with /FAcs and return the source lines around `at`.

    The listing interleaves the C with the assembly it produced, so this is
    the compiler's own answer to "which statement emitted these bytes" -- not
    an inference.  Only valid for a byte-exact function, where the original's
    offsets are ours.
    """
    import shutil
    import tempfile
    tmp = tempfile.mkdtemp(prefix='corpus_cod_')
    try:
        return _cod_lines(tmp, src, va, at, length)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _cod_lines(tmp, src, va, at, length):
    rel_src = os.path.relpath(os.path.join(ROOT, src), ROOT)
    rel_tmp = os.path.relpath(tmp, ROOT)
    cmd = ['sh', WINE, os.path.join(MSVC_DIR, 'bin', 'cl.exe'), '/nologo',
           '/O2', '/W3', '/I', 'include', '/I', 'tools/msvc5-compat',
           '/I', os.path.join(os.path.relpath(MSVC_DIR, ROOT), 'include'),
           '/DBR_MATCHING_BUILD', '/c', '/FAcs',
           '/Fa' + rel_tmp + os.sep, '/Fo' + rel_tmp + os.sep, rel_src]
    try:
        subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=180)
    except Exception as exc:
        return ['(listing failed: %s)' % exc]
    cod = None
    for fn in os.listdir(tmp):
        if fn.lower().endswith('.cod'):
            cod = os.path.join(tmp, fn)
    if not cod:
        return ['(no .cod listing produced)']
    with open(cod, errors='replace') as f:
        lines = f.read().split('\n')
    # ‼ OFFSETS RESTART AT 0 FOR EVERY FUNCTION (each is its own COMDAT), so
    # the scan MUST be scoped to this function's PROC..ENDP block.  Without
    # that, every hit in a multi-function file resolves against whichever
    # function happens to sit at that offset -- which reads as plausible
    # source and is pure fiction.
    nm = FN_NAME.get(va.lower(), '')
    if nm:
        pat = re.compile(r'^[_@]?%s(@\d+)?\s+PROC' % re.escape(nm))
        end = re.compile(r'^[_@]?%s(@\d+)?\s+ENDP' % re.escape(nm))
        lo = hi = None
        for k, ln in enumerate(lines):
            if lo is None and pat.match(ln):
                lo = k
            elif lo is not None and end.match(ln):
                hi = k
                break
        if lo is None:
            return ['(%s not found in the listing -- wrong TU?)' % nm]
        lines = lines[lo:hi or len(lines)]
    want_lo, want_hi = at, at + max(length * 4, 16)
    out, cur = [], None
    for ln in lines:
        m = re.match(r'^;\s*(\d+)\s*:(.*)$', ln)
        if m:
            cur = (int(m.group(1)), m.group(2).rstrip())
            continue
        m = re.match(r'^\s*([0-9a-fA-F]{5,8})\s', ln)
        if m and cur:
            off = int(m.group(1), 16)
            if want_lo <= off <= want_hi:
                if not out or out[-1][0] != cur[0]:
                    out.append(cur)
    return ['%5d | %s' % (n, t) for n, t in out] or \
           ['(no source line covers that offset in the listing)']


# ------------------------------------------------------------------ main ---
def cmd_build(a):
    build_index()


def _resolve_pattern(a):
    if a.pattern:
        return parse_pattern(a.pattern)
    if a.frm:
        at = int(a.at, 0) if a.at else None
        return pattern_from_function(a.frm, at, a.len)
    sys.exit('need --pattern or --from')


def cmd_find(a):
    index = load_index()
    pat = _resolve_pattern(a)
    print('pattern (%d instructions):' % len(pat))
    for t in pat:
        print('    %s' % t)
    print()
    used, hits = (pat, search(index, pat, a.frm, a.max_hits)) if a.exact \
        else search_longest(index, pat, a.frm, a.min_len, a.max_hits)
    if not hits:
        print('NO corpus member contains any run of %d+ of these instructions.'
              % a.min_len)
        print('That is itself the finding: the construct is not proven '
              'anywhere in the solved tree, so there is no spelling to copy.')
        return
    if len(used) < len(pat):
        print('‼ the FULL pattern is not in the corpus; longest run that is: '
              '%d of %d instructions' % (len(used), len(pat)))
        print('  the corpus explains:')
        for t in used:
            print('      %s' % t)
        print('  it does NOT explain, and this is where the real question is:')
        for t in pat[len(used):]:
            print('      %s' % t)
        print()
    print('%d hit(s) in byte-exact functions:' % len(hits))
    for e, k in hits:
        print('  %-12s %-28s +0x%-5x  %s'
              % (e['va'], e['name'][:28], e['offs'][k], e['file']))
    if a.source:
        print()
        for e, k in hits[:a.source]:
            print('--- %s %s  (+0x%x)  %s'
                  % (e['va'], e['name'], e['offs'][k], e['file']))
            for ln in cod_lines(e['file'], e['va'], e['offs'][k], len(used)):
                print('    ' + ln)
            print()


def cmd_show(a):
    """Print the C that produced a given offset of a byte-exact function."""
    index = load_index()
    hit = [e for e in index if e['va'].lower() == a.va.lower()]
    if not hit:
        sys.exit('%s is not a byte-exact corpus member' % a.va)
    e = hit[0]
    at = int(a.at, 0)
    print('%s %s  %s' % (e['va'], e['name'], e['file']))
    for ln in cod_lines(e['file'], e['va'], at, a.len):
        print('    ' + ln)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)

    sub.add_parser('build', help='(re)build the index from report*.csv')

    f = sub.add_parser('find', help='who else emits this instruction pattern')
    f.add_argument('--pattern', help='"tok; tok; tok" in msetdiff normal form')
    f.add_argument('--from', dest='frm', metavar='VA',
                   help='take the pattern from this function\'s ORIGINAL bytes')
    f.add_argument('--at', help='byte offset within --from (e.g. 0x2f0)')
    f.add_argument('--len', type=int, default=8, help='instructions to take')
    f.add_argument('--min-len', dest='min_len', type=int, default=3)
    f.add_argument('--max-hits', dest='max_hits', type=int, default=40)
    f.add_argument('--exact', action='store_true',
                   help='do not fall back to the longest matching prefix')
    f.add_argument('--source', type=int, nargs='?', const=3, default=0,
                   metavar='N', help='also print the C for the first N hits')

    s = sub.add_parser('show', help='print the C behind an offset')
    s.add_argument('--va', required=True)
    s.add_argument('--at', required=True)
    s.add_argument('--len', type=int, default=8)

    a = ap.parse_args()
    {'build': cmd_build, 'find': cmd_find, 'show': cmd_show}[a.cmd](a)


if __name__ == '__main__':
    main()
