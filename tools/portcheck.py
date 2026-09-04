#!/usr/bin/env python3
"""Compile each source in the PORT configuration and report symbols that did
not travel with the code.

WHAT THIS CATCHES, AND WHY THE SWEEP CANNOT
-------------------------------------------
`tools/match_sweep.py` only ever compiles with /DBR_MATCHING_BUILD.  A file
with an `#ifdef BR_MATCHING_BUILD ... #else ... #endif` pair therefore has its
PORT arm compiled by nothing the matching pipeline runs.  So a port arm that
calls a function whose declaration did not travel with it -- a forward
`extern` that lived in a slice's local cross-slice block rather than in any
header, which is the shape refiling drops most easily -- compiles to a C89
IMPLICIT DECLARATION (warning C4013, not an error) and leaves an UNDEFINED
EXTERNAL in the object.  That is a link failure, and the sweep reports the
file as a clean match either way.  This tool compiles the other configuration
and reads the symbol table, which is the only way to see it.

Found exactly that in `src/core/net/br_sessiondesc.c` during the 2026-09-03
refiling job: BrSub1003CDA0's port arm calls BrExt_1003CDA0, whose only
declaration was a local extern in slice6_74.c.  The matching sweep read 1/1
before and after the move.

THIS IS NOT A PORT BUILD.  It compiles ONE configuration of individual
translation units with the staged MSVC 5.0 and inspects their symbol tables.
It does not link, it does not run the macOS/Metal port's own toolchain, and a
clean result here is not a statement that the port builds -- the port build
was already out of sync before this tool existed (docs/MEMORY.md,
[[port-build-drift]]).  Compile-and-symbols for one configuration, nothing
more.

WHAT COUNTS AS A FINDING, AND WHAT DOES NOT
-------------------------------------------
A finding is a C4013 or a compile error -- NOT "this object has undefined
externals".  Every ordinary cross-translation-unit call is an undefined
external: `src/core/settings/br_ctrlcfg.c` correctly references
`@BrCtrlCfgInit@4`, `_g_BrCtrlCfg` and `_g_BrCtrlDefaults`, all defined in
slice3_42.c.  Listing those as problems would bury the real one, and on a
newly created file -- which has no baseline to subtract -- it would bury it
completely.

C4013 is the detector.  In C89 a call to an undeclared function is a WARNING,
so a declaration that did not travel produces C4013 and nothing else; the tool
then reports whether that same name is also undefined in the object, which is
what turns "warning" into "this will not link".  A dropped extern for a
VARIABLE is a hard error instead (C2065), which the compile-error bucket
catches.

What this canNOT see: a symbol that is properly DECLARED but whose definition
did not travel.  That is indistinguishable from a legitimate cross-TU
reference in a single object and needs a real link.  Said plainly rather than
implied.

WHY THE SYMBOL TABLE AND NOT A BYTE SEARCH
------------------------------------------
Searching the .obj bytes for a symbol NAME gives false positives: a file that
DEFINES the symbol also carries the name in its symbol table and its string
table, so the search reports a hit for exactly the file that proves there is
no problem.  slice3_42.c defines `static int BrCtrlProfileIndex(...)` and a
byte search flags it -- that false positive is real, it was hit while building
this tool.  An undefined external is a specific symbol RECORD: SectionNumber
== 0 (IMAGE_SYM_UNDEFINED), StorageClass == 2 (IMAGE_SYM_CLASS_EXTERNAL) and
Value == 0.  Anything else -- a definition, a static, a section or file
record -- is not.

VALIDATED WITH A NEGATIVE CONTROL
---------------------------------
Not trusted until it was shown to fail on a known-bad input.  With the private
copy of BrCtrlProfileIndex removed from src/core/settings/br_ctrlcfg.c the
tool reports BOTH a C4013 for that name AND `_BrCtrlProfileIndex` as an
undefined external; restoring the copy clears both.  The matching sweep reads
3/3 in both states.

BASELINE
--------
Most findings in this tree are pre-existing and would drown a real one.  VC5
predates C99, so every `snprintf` call is a C4013 here; several slices raise
C2373 from the `#define X X_cdecl` prototype-hiding idiom, which only applies
under BR_MATCHING_BUILD.  `--baseline <git-ref>` compiles the SAME PATH at
that ref and subtracts its findings by SYMBOL NAME (never by line number,
which moves whenever code is added or removed).  Only what is new is reported
as a finding.  A file that does not exist at the ref has no baseline: its
findings are all reported as new and the file is marked NO-BASELINE, which is
the normal case for a destination file a refiling job created.

That case is the common one after a refile -- every destination file is new,
so nothing subtracts and the inherited noise comes back.  `--map NEW=OLD`
says "baseline this new file against that old path", which for a refile is
the slice the code came from:

    --baseline main --map src/core/net/br_dplay.c=src/core/slice2_13.c

subtracts what slice2_13.c already reported on main, leaving only what the
move itself introduced.  Repeatable.  `--ignore NAME` drops one symbol
everywhere, for a name that is noise tree-wide: `snprintf` is the standing
example, since VC5 predates C99 and every call to it is a C4013 here and
always was.

USAGE
    python3 tools/portcheck.py                          # every .c under src/core
    python3 tools/portcheck.py src/core/net/br_dplay.c
    python3 tools/portcheck.py --baseline main src/core/slice6_74.c
    python3 tools/portcheck.py --baseline main          # whole tree vs main
    python3 tools/portcheck.py --baseline main --ignore snprintf \
            --map src/core/net/br_dplay.c=src/core/slice2_13.c

EXIT CODES
    0  no findings (or none new, when --baseline is given)
    1  findings: a C4013 or a compile error. An undefined external is never a
       finding by itself -- see WHAT COUNTS AS A FINDING above.
    2  the tool could not run -- no compiler, bad git ref, bad --map, timeout

Honours BR_MSVC exactly as match_sweep.py does; the two share the staged
toolchain.
"""
import argparse
import concurrent.futures as cf
import os
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match_sweep as ms  # noqa: E402  -- ROOT, WINE, CL, MSVC_DIR

ROOT = ms.ROOT
WORK = os.path.join(ROOT, 'build', 'portcheck')


# ---------------------------------------------------------------- COFF ------

# Symbol record, 18 bytes:  Name[8] Value(u32) SectionNumber(i16) Type(u16)
# StorageClass(u8) NumberOfAuxSymbols(u8).  A name whose first four bytes are
# zero is an offset into the string table that follows the symbol table.
IMAGE_SYM_UNDEFINED     = 0
IMAGE_SYM_CLASS_EXTERNAL = 2


def undefined_externals(obj_path):
    """Names of symbols this object references but does not define.

    NOT a substring search over the file -- see the module docstring for why
    that reports the one file that proves there is no problem.
    """
    d = open(obj_path, 'rb').read()
    if len(d) < 20:
        return []
    _machine, _nsec, _ts, psym, nsym, _oh, _ch = struct.unpack_from('<HHIIIHH', d, 0)
    if psym == 0 or nsym == 0:
        return []
    strtab = psym + nsym * 18
    out, i = [], 0
    while i < nsym:
        off = psym + i * 18
        if off + 18 > len(d):
            break
        name8 = d[off:off + 8]
        value, secnum, _typ, sclass, naux = struct.unpack_from('<IhHBB', d, off + 8)
        if name8[0:4] == b'\x00\x00\x00\x00':
            soff = struct.unpack_from('<I', name8, 4)[0]
            end = d.index(b'\x00', strtab + soff)
            name = d[strtab + soff:end].decode('latin1')
        else:
            name = name8.rstrip(b'\x00').decode('latin1')
        if (secnum == IMAGE_SYM_UNDEFINED
                and sclass == IMAGE_SYM_CLASS_EXTERNAL
                and value == 0):
            out.append(name)
        i += 1 + naux
    return sorted(set(out))


# ------------------------------------------------------------- compile ------

def compile_port(rel_src, objdir, tag):
    """Compile rel_src (relative to ROOT) WITHOUT /DBR_MATCHING_BUILD.

    Returns (ok, c4013_names, undefined, errors).  cl.exe reads a leading '/'
    as an option prefix, so every path handed to it is relative to ROOT --
    the same constraint match_sweep.compile_variant documents.
    """
    os.makedirs(objdir, exist_ok=True)
    obj = os.path.join(objdir, tag + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    cmd = ['sh', ms.WINE, ms.CL, '/nologo', '/O2', '/W3',
           '/I', 'include', '/I', 'tools/msvc5-compat',
           '/I', os.path.join(os.path.relpath(ms.MSVC_DIR, ROOT), 'include'),
           '/c', rel_src,
           '/Fo' + os.path.relpath(obj, ROOT).replace('/', '\\')]
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=240)
        out = p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, [], [], ['cl.exe timed out']

    c4013, errors = [], []
    for line in out.splitlines():
        if 'C4013' in line:
            # ...warning C4013: 'name' undefined; assuming extern returning int
            name = line.split("'")[1] if "'" in line else line.strip()
            c4013.append(name)
        elif ': error' in line.lower():
            errors.append(line.strip())
    if not os.path.exists(obj):
        return False, sorted(set(c4013)), [], errors or ['no obj, no diagnostic']
    return True, sorted(set(c4013)), undefined_externals(obj), errors


def findings(ok, c4013, undef, errors):
    """One comparable set per file. Symbol NAMES only -- line numbers move.

    Undefined externals are deliberately NOT findings on their own: an
    ordinary call into another translation unit is one. They appear only as
    corroboration for a C4013 of the same name, which is the difference
    between a warning and a link failure.
    """
    s = set('C4013:' + n for n in c4013)
    if ok is False:
        s.add('COMPILE-ERROR')
    if ok is None:
        s.add('TIMEOUT')
    return s


# ------------------------------------------------------------- baseline -----

def materialise_baseline(ref, rels, mapping):
    """Write each path's content at `ref` under build/portcheck/base/.

    Kept at the same relative path so a quoted #include resolves against the
    includer's own directory the way MSVC does, with include/ still on /I.
    `mapping` redirects a path to a different one at that ref -- for a refile,
    the slice the code came from.

    Returns {rel: rel_of_copy or None if absent at that ref}.
    """
    base = os.path.join(WORK, 'base')
    if os.path.isdir(base):
        shutil.rmtree(base)
    got = {}
    for rel in rels:
        src_at_ref = mapping.get(rel, rel)
        p = subprocess.run(['git', 'show', '%s:%s' % (ref, src_at_ref)],
                           cwd=ROOT, capture_output=True)
        if p.returncode != 0:
            got[rel] = None
            continue
        dst = os.path.join(base, src_at_ref)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(p.stdout)
        got[rel] = os.path.relpath(dst, ROOT)
    return got


# ----------------------------------------------------------------- main -----

def default_sources():
    out = []
    for dirpath, _dirs, files in os.walk(os.path.join(ROOT, 'src', 'core')):
        for f in files:
            if f.endswith('.c'):
                out.append(os.path.relpath(os.path.join(dirpath, f), ROOT))
    return sorted(out)


def main():
    ap = argparse.ArgumentParser(
        description='Compile in the port configuration and report symbols '
                    'that did not travel. Not a port build.')
    ap.add_argument('sources', nargs='*',
                    help='source files (default: every .c under src/core)')
    ap.add_argument('--baseline', metavar='GITREF',
                    help='subtract the findings the same path has at GITREF')
    ap.add_argument('--map', action='append', default=[], metavar='NEW=OLD',
                    help='baseline NEW against OLD at the baseline ref -- for '
                         'a refile, the slice the code came from. Repeatable.')
    ap.add_argument('--ignore', action='append', default=[], metavar='NAME',
                    help='drop this symbol everywhere; for a name that is '
                         'noise tree-wide, e.g. snprintf. Repeatable.')
    ap.add_argument('--jobs', type=int, default=4, help='parallel compiles')
    ap.add_argument('--quiet', action='store_true',
                    help='print only files with findings')
    ap.add_argument('--all-undefined', action='store_true',
                    help='also list every undefined external of a flagged '
                         'file (informational; ordinary cross-TU calls are '
                         'undefined externals and are not findings)')
    args = ap.parse_args()

    if not os.path.exists(ms.CL):
        print('portcheck: no cl.exe at %s -- run setup.sh' % ms.CL)
        return 2

    rels = []
    for s in (args.sources or default_sources()):
        p = os.path.abspath(s)
        if not os.path.exists(p):
            print('portcheck: no such file: %s' % s)
            return 2
        rels.append(os.path.relpath(p, ROOT))

    baseline = {}
    if args.baseline:
        chk = subprocess.run(['git', 'rev-parse', '--verify',
                              args.baseline + '^{commit}'],
                             cwd=ROOT, capture_output=True)
        if chk.returncode != 0:
            print('portcheck: not a git ref: %s' % args.baseline)
            return 2
        mapping = {}
        for m in args.map:
            if '=' not in m:
                print('portcheck: --map wants NEW=OLD, got: %s' % m)
                return 2
            new, old = m.split('=', 1)
            mapping[os.path.relpath(os.path.abspath(new), ROOT)] = \
                os.path.relpath(os.path.abspath(old), ROOT)
        baseline = materialise_baseline(args.baseline, rels, mapping)

    objdir = os.path.join(WORK, 'obj-%d' % os.getpid())

    def one(i_rel):
        i, rel = i_rel
        cur = compile_port(rel, objdir, 'cur%04d' % i)
        base = None
        if args.baseline:
            brel = baseline.get(rel)
            if brel is not None:
                base = compile_port(brel, objdir, 'base%04d' % i)
        return rel, cur, base

    results = []
    with cf.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ex:
        for r in ex.map(one, list(enumerate(rels))):
            results.append(r)

    n_flag = 0
    n_nobase = 0
    for rel, cur, base in sorted(results):
        ok, c4013, undef, errors = cur
        c4013 = [n for n in c4013 if n not in args.ignore]
        new = findings(ok, c4013, undef, errors)
        if base is not None:
            bok, bc, bu, be = base
            new -= findings(bok, [n for n in bc if n not in args.ignore], bu, be)
        tags = []
        if args.baseline and baseline.get(rel) is None:
            tags.append('NO-BASELINE')
            n_nobase += 1
        elif args.baseline and rel in (mapping if args.baseline else {}):
            tags.append('baselined against ' + mapping[rel])
        if not new:
            if not args.quiet:
                print('ok    %-46s %s' % (rel, ' '.join(tags)))
            continue
        n_flag += 1
        print('FLAG  %-46s %s' % (rel, ' '.join(tags)))
        # An undefined external is decorated: _name for cdecl, @name@N for
        # fastcall/thiscall. Match on the bare name inside the decoration.
        def is_undefined(sym):
            return any(u == '_' + sym or u.startswith('@' + sym + '@')
                       or u == sym for u in undef)
        for f in sorted(new):
            if f.startswith('C4013:'):
                sym = f[6:]
                if is_undefined(sym):
                    print('        implicit declaration (C4013): %s'
                          '  -- AND UNDEFINED in the object: will not link'
                          % sym)
                else:
                    print('        implicit declaration (C4013): %s'
                          '  -- not undefined here (defined in this file, or '
                          'the call was folded away)' % sym)
            elif f == 'COMPILE-ERROR':
                for e in errors[:4]:
                    print('        %s' % e)
            else:
                print('        %s' % f)
        if args.all_undefined:
            print('        [all undefined externals, informational: %s]'
                  % (', '.join(undef) or 'none'))

    shutil.rmtree(objdir, ignore_errors=True)
    print('\n%d file(s) checked, %d with findings%s'
          % (len(results), n_flag,
             '' if not args.baseline else ' new since %s' % args.baseline))
    if n_nobase:
        print('%d had no baseline at that ref (new files); everything they '
              'report is listed as new.' % n_nobase)
    print('Compile-and-symbols for ONE configuration. This is not a port build '
          'and does not link.')
    return 1 if n_flag else 0


if __name__ == '__main__':
    sys.exit(main())
