#!/usr/bin/env python3
"""Assemble every in-scope image from our compiled code plus the original's
bytes, diff each against the original, and EMIT the result as a runnable file.

This is the test the per-function checks cannot perform.  match_sweep.py asks
"do these bytes equal the original's bytes at this address", one function at a
time, and a function that answers yes can still be wrong about WHERE it goes:
two functions can claim the same address, a claimed range can overrun into its
neighbour, a size can be wrong.  Those are not hypothetical -- a stale row set
double-claiming 38 addresses is what the report was quietly doing until it was
caught by hand.

So this does not check functions.  It checks the map: every claim is laid into
one image, collisions are refused rather than resolved, and the result is
compared with the original.  Undecompiled functions are not stubbed or skipped;
the original's own bytes stand in for them, which is the standard matching-decomp
arrangement -- coverage decides how much of the image is OURS, not whether the
image can be built at all.

A clean run means: everything we claim to have reproduced, assembled at the
addresses we claim, reproduces the original image exactly.

**VALIDATION IS THE PRIMARY PURPOSE, AND IT IS A HARD GATE.**  The tool exits
non-zero on ANY differing byte, any overlapping claim, any address claimed by
two names, any claim that lands outside .text, and any claimed match this tool
could not build and place.  A binary that failed its diff is still written --
so it can be inspected -- but it is written to `<name>.FAILED` and the run
exits 1.  Never read "it produced files" as "the claims hold"; read the exit
code.

**THE GATE BUILDS WHAT IT GRADES.**  Both lanes compile their own objects,
cached against the source AND the headers, and the sweep's object is reused
only when it can be PROVEN current.  This is not tidiness: the DLL lane used
to read whatever `.obj` happened to be lying in `build/match/obj_<opt>/` with
no freshness test, so an object left over from an older version of a file
placed bytes the current tree would not produce while the run printed "0
differing bytes / every claim holds".  A missing object was caught; a stale
one was not.  That was the one way this tool could lie, and it is closed.

**THREE OUTCOMES, NOT TWO.**  Exit 0 = every claim holds.  Exit 1 splits into
two findings that call for opposite actions and must never be conflated:

    FAILED        a claim is WRONG -- differing bytes, an overlap, two names
                  at one address, a claim outside .text, or a symbol the
                  object does not contain (a stale report row, a rename, a
                  bad VA).  This is a decomp defect.  Chase it.
    INCONCLUSIVE  a claimed function would not COMPILE, so it was never
                  graded.  Every claim that did build holds.  This says
                  nothing about the claims -- fix the source, or wait for
                  whoever is mid-edit, and re-run.

Exit 2 = the tree was edited while the run was grading it.  The verdict then
describes no single state of the tree in either direction; re-run when the
tree is still and record nothing from the racing run.

**What "working binary" means here, exactly.**  Because unclaimed bytes come
from the original, a GREEN run emits a file that is byte-identical to the
original -- it runs because the original runs, not because our C is proven to
execute correctly.  What the emitted file proves is placement: drop it in and
the game behaves, and every byte our tree claims to own is in the right place
with the right value.  The moment a claim is wrong, the diff catches it here
rather than in a crash on the user's machine.

The four in-scope binaries (config/binaries.csv, and the scope table in
CLAUDE.md) are all built:

    BRGlide.dll   the game        C lane (report.csv) + C++ lane (report_cpp.csv)
    BRally.exe    the launcher    report_exe.csv
    SetVideo.exe  display config  report_exe.csv
    BossRally.exe the intro shim  report_exe.csv

BRD3D.dll and Boot.exe are OUT of scope (rule 0 / the scope table) and are not
built.  A drop-in set still needs BRD3D.dll beside BRGlide.dll if the launcher
is pointed at Direct3D; copy the original for that -- do not build it here.

Usage:
    python3 tools/image_build.py                     # all four -> build/image/
    python3 tools/image_build.py --only BRGlide.dll
    python3 tools/image_build.py --out-dir /tmp/drop
    python3 tools/image_build.py --no-write          # gate only, emit nothing
    python3 tools/image_build.py --recompile         # ignore every cached obj
    python3 tools/image_build.py --out build/x.dll   # legacy: DLL to one path
"""
import csv
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import REL_DIR32, REL_REL32                      # noqa: E402
from reloc_fill import parse, load_maps, resolve               # noqa: E402
from reloc_learn import live_objs                              # noqa: E402
from pe_patch import read_pe_text_info                         # noqa: E402
from match_sweep import PREAMBLES                              # noqa: E402

ORIG_DLL = os.environ.get('BR_REF',
             os.path.join(ROOT, 'orig', 'BRGlide.dll'))
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
OUT_DIR = os.path.join(ROOT, 'build', 'image')

# report_exe.csv's `exe` column -> the shipped filename, which is what the
# game's launcher looks for on disk and therefore what we must emit.
EXE_FILES = {
    'brally': 'BRally.exe',
    'setvideo': 'SetVideo.exe',
    'bossrally': 'BossRally.exe',
}


def compiled_functions(objs, fnmap, glmap, only=None, origdir=ORIG_DIR,
                       learned=True):
    """Yield (va, name, filled_bytes, n_unresolved, n_fromref) per function.

    `only` is a {raw COFF symbol name -> va} map. When given, symbols are
    placed by that map alone and the C undecorator is not consulted -- the
    C++ lane's symbols are MSVC-mangled ('?Name@@YAH...@Z'), which the
    cdecl/stdcall/fastcall undecoration below would mince into a name that
    is in no map.  Its addresses come from report_cpp.csv, not from fnmap.

    `learned=False` cuts off config/globals_learned.csv.  That file is read
    back out of the GLIDE image, so its addresses are 0x10xxxxxx; letting an
    EXE's relocation resolve through it would write a DLL address into an EXE
    on nothing but a shared symbol name.  The EXE lanes pass False and take
    their unresolvable slots from their own reference image instead.
    """
    for path in objs:
        try:
            d, secs, syms, relocs = parse(path)
        except Exception:
            continue
        byidx = {s['idx']: s for s in syms}
        for sy in syms:
            sec = secs.get(sy['sec'])
            if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
                continue
            if only is not None:
                if sy['name'] not in only:
                    continue
                name, va = sy['name'], only[sy['name']]
            else:
                # Undecorate: cdecl '_f', stdcall '_f@12', fastcall '@f@12'.
                name = sy['name'].lstrip('_@').split('@')[0]
                if name not in fnmap:
                    continue
                va = fnmap[name]
            ob = os.path.join(origdir, '0x%08X.bin' % va)
            if not os.path.exists(ob):
                continue
            orig = open(ob, 'rb').read()
            # A few originals carry a 16-byte link-stage preamble (jmp +0x0b
            # over nops) fused into the map entry; the compiler's output is the
            # body, starting at +len(pre).  match_sweep strips+verifies the
            # preamble and matches the body -- mirror that here or the body lands
            # at offset 0 and the whole function reads as differing.  The
            # preamble bytes are link output, laid down verbatim.
            pre = PREAMBLES.get('0x%08x' % va, b'')
            plen = len(pre)
            body_n = len(orig) - plen
            body_orig = orig[plen:]
            start = sec['praw'] + sy['val']
            code = bytearray(d[start:start + body_n])
            if len(code) != body_n:
                continue
            unres = 0
            fromref = 0
            for rva, si, rt in relocs[sy['sec']]:
                off = rva - sy['val']            # body-relative
                if not (0 <= off <= body_n - 4):
                    continue
                t = byidx.get(si)
                tgt = (resolve(t['name'], fnmap, glmap, learned=learned)
                       if t else None)
                if tgt is None:
                    # No name-level address (typically a per-file STATIC, whose
                    # name is not unique across objects). The function is a
                    # masked-match claim, so the reference image's own dword is
                    # the correct slot value -- the same original-bytes
                    # arrangement the rest of the image build stands on.
                    # Counted separately: these slots are taken from the
                    # reference, not derived from a surveyed name.
                    code[off:off + 4] = body_orig[off:off + 4]
                    fromref += 1
                    continue
                addend = struct.unpack_from('<i', code, off)[0]
                if rt == REL_DIR32:
                    val = tgt + addend
                elif rt == REL_REL32:
                    # The body sits at va+plen in the image, so a pc-relative
                    # site resolves against its post-preamble address.
                    val = tgt + addend - (va + plen + off + 4)
                else:
                    code[off:off + 4] = body_orig[off:off + 4]
                    fromref += 1
                    continue
                struct.pack_into('<I', code, off, val & 0xFFFFFFFF)
            yield va, name, pre + bytes(code), unres, fromref


def cpp_claims():
    """(obj_path, {mangled symbol: va}, name) for every 4/4 C++ match.

    The C++ lane is scored by tools/cpp_sweep.py into report_cpp.csv and never
    reaches report.csv, so before this it was the one body of verified work the
    image gate could not see: 149 functions inside this same DLL whose bytes
    were checked one at a time but whose ADDRESSES were never laid down beside
    the C claims.  Placement and overlap are exactly what this tool exists to
    check, so it has to place them too.

    Only 4/4 rows count -- a 3/4 row's .text may well be right, but 'match' on
    this lane means all four pieces, and the image gate does not get to use a
    looser bar than the sweep that produced the row.
    """
    import cpp_score
    report = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    if not os.path.exists(report):
        return []
    tags = [_opt_tag(o) for o in cpp_score.DEFAULT_OPTS]
    out = []
    for r in csv.DictReader(open(report)):
        if r.get('status') != 'match' or r.get('pieces') != '4/4':
            continue
        va = int(r['va'], 16)
        src = os.path.join(ROOT, r['file'])
        base = os.path.splitext(os.path.basename(r['file']))[0]
        # cpp_sweep tags each obj with the INDEX into DEFAULT_OPTS it was built
        # at; the report records only the opt's short tag, so map back.
        try:
            i = tags.index(r['opt'])
        except ValueError:
            continue
        obj = os.path.join(cpp_score.OBJ_DIR,
                           '%s_sweep_%08X_%d.obj' % (base, va, i))
        if not os.path.exists(obj):
            continue
        _impl, sym, kind = cpp_score.parse_implements_name(src, va)
        raw = _raw_symbol(obj, sym, kind)
        if raw is None:
            continue
        out.append((obj, {raw: va}, r['name'] or sym or raw))
    return out


def _raw_symbol(obj, sym, kind):
    """The RAW COFF symbol cpp_sweep scored, for this obj.

    The @cpp_symbol tag is not always the literal COFF name: a cdecl one is
    written without its leading underscore, and cpp_sweep resolves the rest
    through cpp_score.find_symbol (class name -> ??1Foo@@..., prefer=kind).
    parse_coff_code's keys are a mix of raw and undecorated, so ask it which
    symbol was scored and then map that answer back to the raw name the COFF
    parser here will see.  Placing anything else would place a function the
    sweep never scored.
    """
    import cpp_score
    import match_diff
    try:
        d, secs, syms, _relocs = parse(obj)
    except Exception:
        return None
    raws = [s['name'] for s in syms
            if s['sec'] > 0 and secs.get(s['sec'], {}).get(
                'name', '').startswith('.text')]
    if sym in raws:
        return sym
    try:
        key = cpp_score.find_symbol(cpp_score.parse_coff_code(obj), sym,
                                    prefer=kind or 'dtor')
    except Exception:
        key = None
    if key is None:
        return None
    if key in raws:
        return key
    for n in raws:
        if match_diff.undecorate(n) == key:
            return n
    return None


def _opt_tag(opt):
    import cpp_sweep
    return cpp_sweep._opt_tag(opt)


# ---------------------------------------------------------------- DLL lane

# report.csv's `opt` column is match_sweep's short tag; cl.exe needs the real
# flags. Kept in sync with match_sweep.VARIANTS rather than duplicated -- an
# added variant must not silently fall back to /O2 here and grade a function
# against a build it never matched under.
def _dll_opt_flags(tag):
    import match_sweep
    for t, flags in match_sweep.VARIANTS:
        if t == tag:
            return flags
    return None


_DEPS_MTIME = None


def _deps_mtime():
    """Newest mtime among the headers every TU compiles against.

    An object newer than its .c can still be stale: a header edit reaches
    dozens of files and changes their codegen without touching one .c. Folding
    the headers into the freshness test costs a rebuild only when a header has
    actually moved -- the same rebuild the sweep would pay -- and closes the
    hole where the gate grades yesterday's bytes.
    """
    global _DEPS_MTIME
    if _DEPS_MTIME is None:
        newest = 0.0
        for d in (os.path.join(ROOT, 'include'),
                  os.path.join(ROOT, 'tools', 'msvc5-compat')):
            for dirpath, _dirs, files in os.walk(d):
                for fn in files:
                    if fn.endswith(('.h', '.inc')):
                        try:
                            newest = max(newest, os.path.getmtime(
                                os.path.join(dirpath, fn)))
                        except OSError:
                            pass
        _DEPS_MTIME = newest
    return _DEPS_MTIME


def _fresh(obj, src):
    """Is `obj` newer than both its source and the newest header?"""
    try:
        t = os.path.getmtime(obj)
    except OSError:
        return False
    return t >= os.path.getmtime(src) and t >= _deps_mtime()


def _compile_dll_obj(rel_src, tag, recompile=False):
    """The object for one DLL TU at the opt its match was scored under.

    (obj_path, err, source) -- source is 'sweep' when match_sweep's own object
    was still fresh, 'gate' when this tool had to build it.

    ‼ THIS USED TO READ THE SWEEP'S OBJECT WITH NO FRESHNESS TEST AT ALL, and
    that is the one way a hard gate can lie: an object left over from an older
    version of the file places bytes the current tree would not produce, while
    the run prints "0 differing bytes / every claim holds". Missing objects
    were caught (they surfaced as unplaced rows); STALE ones were not. The
    gate now builds what it grades, exactly as the EXE lane already did, and
    only reuses the sweep's object when it can prove it current.
    """
    src = os.path.join(ROOT, rel_src)
    if not os.path.exists(src):
        return None, 'source missing', None
    flags = _dll_opt_flags(tag)
    if flags is None:
        return None, 'unknown opt tag %r' % tag, None
    base = os.path.splitext(os.path.basename(src))[0]
    swept = os.path.join(ROOT, 'build', 'match', 'obj_' + tag, base + '.obj')
    if not recompile and _fresh(swept, src):
        return swept, None, 'sweep'
    own_tag = 'img_dll_' + tag
    own = os.path.join(ROOT, 'build', 'match', 'obj_' + own_tag, base + '.obj')
    if not recompile and _fresh(own, src):
        return own, None, 'gate'
    import match_sweep
    got, errs = match_sweep.compile_variant(src, own_tag, flags)
    if got is None:
        return None, '; '.join(errs) or 'compile failed', None
    return got, None, 'gate'


def collect_dll(recompile=False, progress=None):
    """{va: (name, code, unres, fromref, lane)} plus the bookkeeping the gate
    needs: the names claiming each address, and the rows we FAILED to build."""
    fnmap, glmap = load_maps()

    # Only place what the tree actually CLAIMS to have reproduced. fnmap holds
    # every tagged function including the ones still diffing; laying those in
    # would measure the size of the undone work, not the truth of the claims.
    claimed = {}
    names_at = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match',
                                              'report.csv'))):
        if r.get('va') and r.get('status') == 'match':
            va = int(r['va'], 16)
            claimed[va] = r
            names_at.setdefault(va, set()).add(r['name'])

    # Take the build the SCORER matched -- its optimisation level, from its
    # source file -- not whichever build happens to resolve most relocations.
    # Every file is compiled at both /O2 and /Od and a function matches under
    # one of them; picking the other yields different code at the same address
    # and reads as a failed claim when nothing is wrong with the claim.
    want = {}
    for va, r in claimed.items():
        if not r.get('opt'):
            continue
        want.setdefault((r['file'], r['opt']), []).append((va, r['name']))

    best = {}
    unbuildable = []
    reused = built = 0
    for i, ((rel_src, tag), wanted) in enumerate(sorted(want.items())):
        if progress:
            progress(i + 1, len(want), rel_src)
        obj, err, how = _compile_dll_obj(rel_src, tag, recompile)
        if obj is None:
            for va, name in wanted:
                unbuildable.append((va, name, '%s: %s' % (rel_src, err)))
            continue
        reused += how == 'sweep'
        built += how == 'gate'
        byname = {n: va for va, n in wanted}
        for va, name, code, unres, fromref in compiled_functions(
                [obj], fnmap, glmap):
            if byname.get(name) == va:
                best[va] = (name, code, unres, fromref, 'C')

    # The C++ lane, placed beside the C so a cross-lane address collision is
    # visible.  Same reloc filling, same collision refusal, same diff.
    for obj, symmap, dispname in cpp_claims():
        for va, _sym, code, unres, fromref in compiled_functions(
                [obj], fnmap, glmap, only=symmap):
            names_at.setdefault(va, set()).add(dispname)
            best[va] = (dispname, code, unres, fromref, 'C++')

    # A claimed match this tool could not build is NOT a pass. Before the gate
    # was strict these dropped out silently, and a run that placed less work
    # than the report claims still printed "0 differing bytes" -- the one way
    # this tool can lie. Named here, and fatal in main().
    #
    # Two different things, kept apart because they call for opposite actions:
    # `unbuildable` means the TREE does not currently compile (fix the source,
    # or wait for whoever is mid-edit); `unplaced` means it compiled and the
    # claimed symbol is NOT IN the object, which is a real bookkeeping defect
    # -- a stale report row, a renamed function, a wrong VA. Reporting both as
    # "the tree's claims do not hold" sent a session chasing a phantom.
    bad = set(va for va, _n, _w in unbuildable)
    unplaced = [(va, r['name'], r['file'] + ': symbol not in obj')
                for va, r in sorted(claimed.items())
                if va not in best and va not in bad]
    cpp_rows = sum(1 for r in csv.DictReader(open(
        os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')))
        if r.get('status') == 'match' and r.get('pieces') == '4/4') \
        if os.path.exists(os.path.join(ROOT, 'build', 'match',
                                       'report_cpp.csv')) else 0
    n_cpp = sum(1 for v in best.values() if v[4] == 'C++')
    for _ in range(cpp_rows - n_cpp):
        unplaced.append((0, '(C++ 4/4 row)', 'report_cpp.csv'))
    if reused or built:
        print('  objects: %d reused from the sweep, %d rebuilt by the gate'
              % (reused, built))
    return best, names_at, unplaced, unbuildable


# ---------------------------------------------------------------- EXE lanes


def _obj_tag(exe, opt):
    """A filesystem-safe obj-dir tag. The opt string carries slashes and
    spaces ('/O2 /Oy- /ML'), and two rows in one EXE can differ only by it --
    the tag has to keep those builds in separate directories or the second
    overwrites the first under the same basename."""
    slug = opt.replace('/', '').replace('-', '').replace(' ', '_')
    return 'img_%s_%s' % (exe, slug)


def _compile_exe_obj(rel_src, opt, exe, recompile=False):
    """Compile one EXE TU at the opt its match was scored under, cached.

    exe_sweep.py scores through ghidra_to_match._score_source, which builds in
    a temp dir and deletes it -- so unlike the DLL lane there is no obj on disk
    to reuse and every EXE function has to be rebuilt here. Cached by mtime so
    a re-run of the gate costs nothing.
    """
    import match_sweep
    src = os.path.join(ROOT, rel_src)
    if not os.path.exists(src):
        return None, 'source missing'
    tag = _obj_tag(exe, opt)
    obj = os.path.join(ROOT, 'build', 'match', 'obj_' + tag,
                       os.path.splitext(os.path.basename(src))[0] + '.obj')
    if not recompile and os.path.exists(obj) and \
            os.path.getmtime(obj) >= os.path.getmtime(src):
        return obj, None
    got, errs = match_sweep.compile_variant(src, tag, opt)
    if got is None:
        return None, '; '.join(errs) or 'compile failed'
    return got, None


def collect_exe(exe, recompile=False, progress=None):
    """The one EXE's claims, built and reloc-filled.

    The function map here is the EXE's OWN matched functions and nothing else.
    load_maps() is Glide-keyed (report.csv + globals_glide.csv), so feeding it
    an EXE relocation would resolve a shared name to a 0x10xxxxxx address
    inside a 0x004xxxxx image. Intra-EXE calls to our own matched functions do
    resolve; everything else -- CRT, imports, statics -- takes the reference
    image's own dword, the same arrangement the DLL lane's unnameable slots
    use.
    """
    report = os.path.join(ROOT, 'build', 'match', 'report_exe.csv')
    rows = [r for r in csv.DictReader(open(report))
            if r.get('exe') == exe and r.get('status') == 'match'
            and r.get('va')]
    # compiled_functions' C path undecorates a COFF symbol by stripping '_@'
    # and everything from the first '@', so it looks up 'matherr', while the
    # report spells the CRT-shaped names with their underscore ('_matherr').
    # Key the map BOTH ways or those two rows read as "symbol not in obj" on a
    # function that built fine.
    fnmap = {}
    for r in rows:
        if not r.get('name'):
            continue
        va = int(r['va'], 16)
        fnmap[r['name']] = va
        fnmap.setdefault(r['name'].lstrip('_@').split('@')[0], va)
    origdir = os.path.join(ROOT, 'build', 'match', 'orig_%s' % exe)

    best, names_at, unplaced, unbuildable = {}, {}, [], []
    for i, r in enumerate(rows):
        va = int(r['va'], 16)
        names_at.setdefault(va, set()).add(r['name'])
        if progress:
            progress(i + 1, len(rows), r['file'])
        obj, err = _compile_exe_obj(r['file'], r['opt'], exe, recompile)
        if obj is None:
            unbuildable.append((va, r['name'], '%s: %s' % (r['file'], err)))
            continue
        hit = False
        for gva, _name, code, unres, fromref in compiled_functions(
                [obj], fnmap, {}, origdir=origdir, learned=False):
            if gva != va:
                continue
            best[va] = (r['name'], code, unres, fromref, 'EXE')
            hit = True
        if not hit:
            unplaced.append((va, r['name'], r['file'] + ': symbol not in obj'))
    return best, names_at, unplaced, unbuildable


# ---------------------------------------------------------------- assembly


def assemble(orig_path, best, names_at, unplaced, label, unbuildable=()):
    """Lay every claim into the original image; print the report; return
    (image_bytes, verdict) where verdict is 'ok', 'claims' or 'build'."""
    usable = {va: v for va, v in best.items() if v[2] == 0}
    blocked = len(best) - len(usable)
    n_fromref_fns = sum(1 for v in usable.values() if v[3])
    n_fromref_slots = sum(v[3] for v in usable.values())

    # How much of each lane is OURS vs taken from the reference image. A slot
    # filled from the reference cannot fail the diff, so it is not evidence --
    # quoting the image's byte total without this split overstates the C++
    # lane, whose mangled symbols resolve through no surveyed map.
    lane = {}
    for _va, (_n, c, _u, fr, ln) in usable.items():
        d = lane.setdefault(ln, [0, 0, 0])
        d[0] += 1
        d[1] += len(c)
        d[2] += fr * 4

    # GLOBAL CHECK 1 -- overlapping claims. Per-function diffing cannot see it.
    spans = sorted((va, va + len(c), n)
                   for va, (n, c, _u, _f, _l) in usable.items())
    overlaps = [(a, b) for a, b in zip(spans, spans[1:]) if a[1] > b[0]]

    # GLOBAL CHECK 2 -- two DIFFERENT functions claiming one address. The same
    # function appearing twice is not a conflict: every source file is built at
    # both /O2 and /Od, so each function legitimately has two builds. Only
    # distinct names at one address mean a tagging error.
    conflicting = [(va, sorted(ns)) for va, ns in sorted(names_at.items())
                   if len(ns) > 1 and va in usable]

    counts = {}
    for v in best.values():
        counts[v[4]] = counts.get(v[4], 0) + 1
    breakdown = ' + '.join('%d %s' % (n, k) for k, n in sorted(counts.items()))

    print('=' * 68)
    print(label)
    print('=' * 68)
    print(f"functions compiled and addressed : {len(best)}"
          + (f"  ({breakdown})" if len(counts) > 1 else ""))
    print(f"  every relocation resolved      : {len(usable)}")
    print(f"    of those, {n_fromref_fns} functions fill {n_fromref_slots} "
          f"static/local slots from the reference image")
    print(f"  blocked on an unknown address  : {blocked}")
    print(f"  claimed but the TREE WON'T BUILD: {len(unbuildable)}")
    for va, name, why in list(unbuildable)[:10]:
        print(f"    {va:#010x} {name}: {why}")
    print(f"  claimed but NOT placed         : {len(unplaced)}")
    for va, name, why in unplaced[:10]:
        print(f"    {va:#010x} {name}: {why}")
    print(f"\noverlapping address claims       : {len(overlaps)}")
    for a, b in overlaps[:10]:
        print(f"    {a[2]} [{a[0]:#x}..{a[1]:#x}) runs into {b[2]} at {b[0]:#x}")
    print(f"two names claiming one address   : {len(conflicting)}")
    for va, ns in conflicting[:10]:
        print(f"    {va:#x}: {' vs '.join(ns)}")

    # Lay every usable function into the original image at its claimed address.
    base, secs = read_pe_text_info(orig_path)
    img = bytearray(open(orig_path, 'rb').read())
    text = [s for s in secs if s[0].startswith('.text')][0]
    _, trva, tvsize, traw, _ = text

    placed = bytes_placed = 0
    outside = []
    diffs = []
    for va, (name, code, _u, _f, _l) in sorted(usable.items()):
        off = va - base - trva
        if off < 0 or off + len(code) > tvsize:
            outside.append((va, name))
            continue
        fo = traw + off
        if img[fo:fo + len(code)] != code:
            nd = sum(1 for i in range(len(code)) if img[fo + i] != code[i])
            diffs.append((va, name, nd, len(code)))
        img[fo:fo + len(code)] = code
        placed += 1
        bytes_placed += len(code)

    orig = open(orig_path, 'rb').read()
    delta = sum(1 for i in range(len(orig)) if orig[i] != img[i])

    print(f"\nplaced into the image            : {placed} functions, "
          f"{bytes_placed:,} bytes ({100*bytes_placed/tvsize:.2f}% of .text)")
    for ln in sorted(lane):
        n, b, rb = lane[ln]
        print(f"    {ln:<4} {n:4} fns  {b:8,} B  "
              f"({rb:,} B of that filled from the reference"
              f", {100*rb/b if b else 0:.1f}%)")
    print(f"  landed outside .text           : {len(outside)}")
    for va, name in outside[:10]:
        print(f"    {va:#010x} {name}")
    print(f"functions differing from original: {len(diffs)}")
    for va, name, nd, sz in sorted(diffs, key=lambda x: -x[2])[:10]:
        print(f"    {va:#x} {name}: {nd}/{sz} bytes")

    print(f"\nASSEMBLED IMAGE vs ORIGINAL: {delta} differing bytes")
    # Two non-passes, and they are NOT the same finding. A claim that is wrong
    # is a decomp defect; a tree that will not compile is a state problem that
    # says nothing about the claims. Both still exit non-zero -- a run that
    # graded less than the report claims must never read as a pass -- but the
    # message has to name which, or it sends the reader after the wrong bug.
    claims_bad = bool(delta or overlaps or conflicting or outside or unplaced
                      or blocked)
    if claims_bad:
        verdict = 'claims'
        print("  -> FAILED: the tree's claims do not hold at image level.")
    elif unbuildable:
        verdict = 'build'
        print(f"  -> INCONCLUSIVE: {len(unbuildable)} claimed function(s) "
              "could not be compiled, so they were never graded.")
        print("     Every claim that DID build holds. Fix the source (or wait "
              "for whoever is mid-edit) and re-run;")
        print("     this is not evidence of a wrong claim.")
    else:
        verdict = 'ok'
        print("  -> every claim holds at image level.")
    return bytes(img), verdict


def _show(path):
    """Repo-relative when it IS in the repo; an --out-dir outside the tree
    otherwise renders as a stack of '../..' that is harder to read than the
    absolute path it came from."""
    rel = os.path.relpath(path, ROOT)
    return path if rel.startswith('..') else rel


def _source_stamp():
    """{path: mtime} for every file a run's verdict depends on.

    Compared before and after the run: if any of them moved, the tree was
    edited WHILE it was being graded and the verdict describes no single state
    of the tree. That is not a hypothetical -- a refiling job rewriting src/
    produced three different failure sets in twenty minutes, each of which read
    as a hard FAIL. A racing run must say so rather than accuse the tree.
    """
    out = {}
    for d in (os.path.join(ROOT, 'src'), os.path.join(ROOT, 'include')):
        for dirpath, _dirs, files in os.walk(d):
            for fn in files:
                if fn.endswith(('.c', '.cpp', '.h', '.inc')):
                    p = os.path.join(dirpath, fn)
                    try:
                        out[p] = os.path.getmtime(p)
                    except OSError:
                        pass
    for name in ('report.csv', 'report_cpp.csv', 'report_exe.csv'):
        p = os.path.join(ROOT, 'build', 'match', name)
        try:
            out[p] = os.path.getmtime(p)
        except OSError:
            pass
    return out


def _raced(before, after):
    """The files that appeared, vanished or changed between two stamps."""
    moved = [p for p, t in after.items() if before.get(p) != t]
    moved += [p for p in before if p not in after]
    return sorted(set(moved))


def emit(img, ok, out_path, no_write):
    """Write the assembled image. A failed one is still written -- for
    inspection -- but under a name nobody can mistake for a drop-in."""
    if no_write:
        return None
    path = out_path if ok else out_path + '.FAILED'
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    open(path, 'wb').write(img)
    for stale in (out_path, out_path + '.FAILED'):
        if stale != path and os.path.exists(stale):
            os.unlink(stale)
    print(f"wrote {_show(path)}  ({len(img):,} bytes)")
    return path


def main():
    argv = sys.argv[1:]

    def opt(flag, default=None):
        return argv[argv.index(flag) + 1] if flag in argv else default

    out_dir = opt('--out-dir', OUT_DIR)
    legacy_out = opt('--out')
    only = opt('--only')
    no_write = '--no-write' in argv
    recompile = '--recompile' in argv

    targets = ['BRGlide.dll'] + list(EXE_FILES.values())
    if only:
        want = only.lower()
        targets = [t for t in targets if t.lower() == want]
        if not targets:
            sys.exit('--only: no in-scope binary named %r (have: %s)'
                     % (only, ', '.join(['BRGlide.dll']
                                        + list(EXE_FILES.values()))))

    before = _source_stamp()
    results = []
    for i, tgt in enumerate(targets):
        if i:
            print()
        if tgt == 'BRGlide.dll':
            def dprog(n, total, f):
                sys.stderr.write('\r  building BRGlide %d/%d %-40s'
                                 % (n, total, os.path.basename(f)))
                sys.stderr.flush()
            best, names_at, unplaced, unbuildable = collect_dll(recompile,
                                                                dprog)
            sys.stderr.write('\r' + ' ' * 70 + '\r')
            orig_path = ORIG_DLL
        else:
            exe = [k for k, v in EXE_FILES.items() if v == tgt][0]

            def prog(n, total, f, _e=exe):
                sys.stderr.write('\r  building %s %d/%d %-40s'
                                 % (_e, n, total, os.path.basename(f)))
                sys.stderr.flush()
            best, names_at, unplaced, unbuildable = collect_exe(
                exe, recompile, prog)
            sys.stderr.write('\r' + ' ' * 70 + '\r')
            orig_path = os.path.join(ROOT, 'orig', tgt)
        img, verdict = assemble(orig_path, best, names_at, unplaced, tgt,
                                unbuildable)
        dest = legacy_out if (legacy_out and tgt == 'BRGlide.dll') \
            else os.path.join(out_dir, tgt)
        emit(img, verdict == 'ok', dest, no_write)
        results.append((tgt, verdict))

    raced = _raced(before, _source_stamp())

    print()
    print('=' * 68)
    WORD = {'ok': 'OK', 'claims': 'FAILED', 'build': 'INCONCLUSIVE (build)'}
    for tgt, verdict in results:
        print('  %-14s %s' % (tgt, WORD[verdict]))
    if raced:
        # Printed before the verdict line so it is read first: a racing run's
        # verdict is about no single state of the tree, in EITHER direction.
        print('\n‼ THE TREE CHANGED WHILE THIS RUN WAS GRADING IT '
              '(%d file(s)):' % len(raced))
        for p in raced[:8]:
            print('    %s' % _show(p))
        if len(raced) > 8:
            print('    ... and %d more' % (len(raced) - 8))
        print('  This verdict describes no single state of the tree. Re-run '
              'when the tree is still;')
        print('  do NOT record either a pass or a failure from this run.')
        sys.exit(2)
    failed = [t for t, v in results if v == 'claims']
    unbuilt = [t for t, v in results if v == 'build']
    if failed:
        print('\nIMAGE GATE FAILED: ' + ', '.join(failed))
        sys.exit(1)
    if unbuilt:
        print('\nIMAGE GATE INCONCLUSIVE: ' + ', '.join(unbuilt)
              + ' -- claimed functions that would not compile were never '
                'graded.')
        print('Every claim that built holds. This is a tree-state problem, '
              'not a wrong claim.')
        sys.exit(1)
    if not no_write and not legacy_out:
        # BRD3D.dll is out of scope (rule 0) but the launcher will load it if
        # the registry says Direct3D, so a drop-in directory that lacks it is
        # not a drop-in directory. Copied verbatim, never built.
        src = os.path.join(ROOT, 'orig', 'BRD3D.dll')
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(out_dir, 'BRD3D.dll'))
            print('  BRD3D.dll      copied verbatim (out of scope, rule 0)')
        print('\ndrop-in set in %s' % _show(out_dir))
    print('IMAGE GATE PASSED')


if __name__ == '__main__':
    main()
