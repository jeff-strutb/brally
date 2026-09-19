#!/usr/bin/env python3
"""Contract-valid (T3+) build gate for BRGlide.dll -- the Milestone-1 companion
to tools/image_build.py.

image_build.py proves the BYTE-EXACT (T4) subset: everything the tree claims to
have reproduced, assembled at the addresses it claims, is byte-identical to the
original.  Its bar is byte-identity, and the file it emits is a drop-in.

This proves the wider CONTRACT-VALID corpus: every T4 function PLUS every
T3-certified function (complete and verified, NOT byte-exact -- CLAUDE.md rule
12, decided by tools/t3.py --qualify) COMPILES under MSVC 5.0 and places at its
claimed address.  That is the Milestone-1 claim in the README, made checkable:
"we could compile from these and port from them."

**WHY THIS IS NOT A BYTE GATE.**  A T3 function is certified precisely because
its bytes differ from the original -- and it may differ in length.  So the image
this emits is deliberately NOT byte-identical to the original and is NOT a
drop-in: it is the binary with our best contract-valid code compiled in for
every certified function.  Each function is still laid into its original-size
slot (the compiled body is placed truncated to the original's length, exactly as
the byte gate places a match), so the image stays a valid fixed-layout PE that
differs from the original ONLY inside T3 function bodies.  Emitted as
`BRGlide.T3.dll` -- a name nobody can mistake for the byte-exact drop-in.

**PASS / FAIL, and they are not the byte gate's.**  Exit 0 (PASS) means:

    * every T3 and T4 function COMPILED                  (nothing unbuildable)
    * every claimed symbol is present in its object      (nothing unplaced)
    * the T4 backbone is still byte-exact at every T4 address   (no regression)
    * no overlap, no two names at one address, nothing outside .text (geometry)

The residue -- how many bytes the placed T3 functions differ from the original
-- is REPORTED, never a failure: that difference is the definition of T3.  A
T4-span byte that differs, by contrast, IS a failure: it means a match
regressed, and that is the same defect the byte gate exists to catch.

Three outcomes, mirroring image_build.py:

    FAILED        a T4 span differs (a match regressed), an overlap, two names
                  at one address, a claim outside .text, or a claimed symbol
                  absent from its object.  A real defect -- chase it.
    INCONCLUSIVE  a claimed (T3 or T4) function would not COMPILE, so it was
                  never graded.  Fix the source (or wait for whoever is
                  mid-edit) and re-run.
    OK            everything above holds; the only bytes differing from the
                  original are inside T3 bodies, and that is expected.

Exit 2 = the tree was edited while the run was grading it (same meaning and
handling as the byte gate); record nothing from a raced run.

Scope: BRGlide.dll only -- that is where T3 lives.  The three in-scope EXEs are
fully byte-exact (no T3), so "T3 or better" is just "T4" for them; run
image_build.py for those.

Usage:
    python3 tools/image_build_t3.py                # gate + emit BRGlide.T3.dll
    python3 tools/image_build_t3.py --no-write     # gate only, emit nothing
    python3 tools/image_build_t3.py --recompile    # ignore every cached obj
    python3 tools/image_build_t3.py --t3-only      # skip the T4 rebuild; grade
                                                   #   the T3 additions alone
    python3 tools/image_build_t3.py --out-dir DIR
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import image_build as ib                                        # noqa: E402
from reloc_fill import load_maps                                # noqa: E402
from t3 import certified                                        # noqa: E402

REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')


def collect_t3(recompile=False, progress=None):
    """{va: (name, code, unres, fromref, 'T3')} for every certified T3 function,
    plus the rows we could not build (`unbuildable`) or place (`unplaced`).

    A T3 function is a DIFF row in report.csv whose VA carries a validated `@t3`
    tag (t3.certified()[va]['ok']).  It is compiled at the same opt its diff was
    scored under -- the object tools/t3.py --qualify certified -- and laid down
    exactly as image_build places a match: its first orig_size bytes, with every
    relocation resolved.  Mangled C++ tags ('?...') are skipped; the C++ EH lane
    is a separate image concern and carries no report.csv row here.
    """
    fnmap, glmap = load_maps()
    cert = {va for va, i in certified().items()
            if not va.startswith('?') and i['ok']}

    # A T3 body is NOT byte-identical to the original outside its reloc slots
    # -- its certified residue is a reschedule -- so image_build's fallback of
    # copying the reference dword at the same offset writes garbage addresses
    # (BrTimeUpdate 0x1006E360 shipped `mov [0x11], ecx` and page-faulted on
    # real hardware).  ref_fill=False makes an unnameable slot BLOCK the
    # function; the addresses come from augment_maps -- the SAME per-object
    # resolution the A5 oracle certified each function under (address-bearing
    # names, `/* 0x<VA> */` declaration comments, string constants, imports),
    # so a function places with exactly the addresses its equivalence proof
    # used, or it does not place.
    from t3b_env import address_in_name, augment_maps, const_slot_values
    from t3b_env import image as ref_image
    from reloc_fill import resolve as rf_resolve
    from reloc_fill import _undecorate as rf_undecorate
    from reloc_pair import (pair_function, audit_function, jump_table_slots,
                            content_anchor_syms, import_thunk_syms,
                            _our_sites)

    def _site_symbols(obj, name, va, size):
        st, _ = _our_sites(obj, name, size)
        return [((va, s[0]), s[1]) for s in (st or [])]

    recovered = {}          # global-name pairing results, cross-function

    rows = {}
    for r in csv.DictReader(open(REPORT)):
        if r.get('status') == 'diff' and r.get('va') \
                and r['va'].lower() in cert:
            rows[int(r['va'], 16)] = r

    # Same basename-collision guard the byte gate uses: two source files sharing
    # a basename cannot share the sweep's object cache, so build those ourselves.
    ambiguous = ib._ambiguous_basenames(rows)

    best, unplaced, unbuildable = {}, [], []
    want = {}
    for va, r in rows.items():
        if not r.get('opt'):
            unbuildable.append((va, r['name'],
                                '%s: diff row carries no opt' % r['file']))
            continue
        want.setdefault((r['file'], r['opt']), []).append((va, r['name']))

    # ---------------------------------------------------- provenance -----
    # A resolution is EVIDENCE when its address derives from this binary
    # itself: a matched function's VA, the validated learned map, a Ghidra
    # DAT_/FUN_ name (coined from this image), the import table, a string /
    # constant / thunk / jump-table identity, or a pairing against the
    # original body.  It is HEARSAY when it derives from a hand-coined name
    # or a hand row (g_<HEX>, BrSubXXXXXXXX suffixes, globals CSV rows,
    # `/* 0x<VA> */` comments): those have carried stale D3D-space addresses
    # that pairing keeps exposing, and one shipped a mapped-but-wrong WRITE
    # into a placed frame-loop function.  Hearsay places only where the
    # pairing has CONFIRMED or CORRECTED it -- in this function or any other
    # -- and a function left with unconfirmed hearsay BLOCKS.
    from t3b_env import (_ADDR_IN_NAME, _find_cstr, _declared_va,
                         _declared_data_va)
    from relocmap import load_learned, normalize, REL_DIR32
    img = ref_image()
    learned = load_learned()
    imports = img.imports()

    def _trusted(sym, anchors):
        s = sym.lstrip('_')
        u = rf_undecorate(sym)
        for c in (s, u):
            if c in fnmap:
                return fnmap[c]
            if c in learned:
                return learned[c]
        m = _ADDR_IN_NAME.match(s) or _ADDR_IN_NAME.match(u)
        if m:
            a = int(m.group(1), 16)
            if img.mapped(a) or img.is_bss(a):
                if s[:3].upper() in ('FUN', 'SUB') \
                        and not (img.text_lo <= a < img.text_hi):
                    return None
                return a
        if sym.startswith('??_C'):
            return _find_cstr(img, sym)
        a = anchors.get(sym)
        if a is not None:
            return a
        # An IAT slot address answers only the INDIRECT form (`call
        # [__imp__x]`); a direct call to the plain name goes through the
        # jmp-thunk, which is the anchors' job above -- resolving the plain
        # name to the slot put an IAT address in a rel32 (caught by the
        # audit on FUN_10028200).
        if sym.lstrip('_').startswith('imp_') and sym in imports:
            return imports[sym]
        return None

    def _hearsay(sym):
        s = sym.lstrip('_')
        u = rf_undecorate(sym)
        for c in (s, u, normalize(s)):
            if c in glmap:
                return glmap[c]
        a = address_in_name(sym) or address_in_name(u)
        if a is not None:
            return a                       # suffix / g_<HEX> convention form
        for c in (s, u):
            if c in _declared_va():
                return _declared_va()[c]
            if c in _declared_data_va():
                return _declared_data_va()[c]
        return None

    # -------------------------------------------------- phase A: derive --
    # Compile every object, gather identity anchors, run the pairing and the
    # audit, and pool the evidence: which hearsay symbols the original's own
    # bytes CONFIRM, and which they CORRECT (globally, so one function's
    # proof serves every function naming the same global).
    records = []
    g_conf, g_corr, g_bad = {}, {}, set()
    for i, ((rel_src, tag), wanted) in enumerate(sorted(want.items())):
        if progress:
            progress(i + 1, len(want), rel_src)
        obj, err, _how = ib._compile_dll_obj(rel_src, tag, recompile, ambiguous)
        if obj is None:
            for va, name in wanted:
                unbuildable.append((va, name, '%s: %s' % (rel_src, err)))
            continue
        anchors = {}
        idsites = {}
        for va, name in wanted:
            size = int(rows[va]['orig_size'])
            pre = ib.PREAMBLES.get('0x%08x' % va, b'')
            idsites.update(const_slot_values(obj, name, va, size))
            idsites.update(jump_table_slots(obj, name, va, size,
                                            plen=len(pre)))
            anchors.update(content_anchor_syms(obj, name, size, img))
            anchors.update(import_thunk_syms(obj, name, size, img))

        def _known(sym, anchors=anchors):
            v = _trusted(sym, anchors)
            return v if v is not None else _hearsay(sym)

        fns = []
        for va, name in wanted:
            ob = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
            if not os.path.exists(ob):
                continue
            size = int(rows[va]['orig_size'])
            pre = ib.PREAMBLES.get('0x%08x' % va, b'')
            orig_body = open(ob, 'rb').read()[len(pre):]
            paired, _ref, assigned = pair_function(
                obj, name, va, size, orig_body, img, _known, plen=len(pre))
            fixes, rep, confirmed = audit_function(
                obj, name, va, size, orig_body, img, _known, plen=len(pre))
            for osym, oimg, omap in rep:
                print('  MAP OVERRIDE %s %s: image says %s, map said %s'
                      % (name, osym,
                         hex(oimg) if oimg is not None else '?', hex(omap)))
            fns.append((va, name, size, len(pre), paired, assigned,
                        fixes, rep, confirmed))
            for sym, addr in assigned.items():
                rkey = (obj, sym) if '$' in sym else sym
                prev = recovered.setdefault(rkey, (addr, name))
                if prev[0] != addr:
                    if prev[0] is not None:
                        print('  PAIR CONFLICT %s: %#x (%s) vs %#x (%s) -- '
                              'both refused'
                              % (sym, prev[0], prev[1], addr, name))
                    recovered[rkey] = (None, name)
            for sym, addr in confirmed.items():
                ckey = (obj, sym) if '$' in sym else sym
                g_conf.setdefault(ckey, addr)
            for osym, oimg, _omap in rep:
                if oimg is None:
                    continue
                ckey = (obj, osym) if '$' in osym else osym
                prev = g_corr.setdefault(ckey, oimg)
                if prev != oimg:
                    g_bad.add(ckey)
                    print('  CORRECTION CONFLICT %s: %#x vs %#x -- refused'
                          % (osym, prev, oimg))
        records.append((obj, rel_src, wanted, anchors, idsites, fns))

    # -------------------------------------------------- phase B: place ---
    for obj, rel_src, wanted, anchors, idsites, fns in records:
        byname = {n: va for va, n in wanted}
        got = set()
        sites = dict(idsites)
        for va, name, size, plen, paired, assigned, fixes, rep, conf in fns:
            site_syms = dict(_site_symbols(obj, name, va, size))
            st, _b = _our_sites(obj, name, size)
            for off, sym, rt, addend, _key in (st or []):
                k = (va, off)
                if k in sites:
                    continue                     # identity slot
                ckey = (obj, sym) if '$' in sym else sym
                a = _trusted(sym, anchors)
                if a is None:
                    if ckey in g_bad:
                        continue                 # conflicted -> block
                    a = g_corr.get(ckey)
                    if a is None and _hearsay(sym) is not None \
                            and g_conf.get(ckey) == _hearsay(sym):
                        a = _hearsay(sym)
                if a is None:
                    continue                     # pairing below, or block
                if rt == REL_DIR32:
                    sites[k] = (a + addend) & 0xFFFFFFFF
                else:
                    sites[k] = (a + addend
                                - (va + plen + off + 4)) & 0xFFFFFFFF
            # forced per-site evidence beats every name-derived value
            for k, v in paired.items():
                if site_syms.get(k) is not None:
                    rkey = ((obj, site_syms[k]) if '$' in site_syms[k]
                            else site_syms[k])
                    if recovered.get(rkey, (0,))[0] is None:
                        continue                 # conflicted pairing
                sites[k] = v
            sites.update(fixes)
        for va, name, code, unres, fromref in ib.compiled_functions(
                [obj], fnmap, {}, pad_short=True, ref_fill=False,
                extra_sites=sites):
            if byname.get(name) == va:
                best[va] = (name, code, unres, fromref, 'T3')
                got.add(va)
        for va, name in wanted:
            if va not in got:
                unplaced.append((va, name, rel_src + ': symbol not in obj'))
    return best, unplaced, unbuildable


def assemble_contract(orig_path, t4_best, t3_best, names_at, unplaced,
                      unbuildable):
    """Lay the T4 backbone and the T3 additions into the original image; report;
    return (image_bytes, verdict).

    The one difference from image_build.assemble is the diff classification: a
    byte differing inside a T3 span is the certified residue (reported), a byte
    differing anywhere else is a regression of a byte-exact claim (fatal).
    """
    t3_vas = set(t3_best)
    best = dict(t4_best)
    best.update(t3_best)                       # T3 never shares a VA with T4
    usable = {va: v for va, v in best.items() if v[2] == 0}
    blocked = len(best) - len(usable)

    counts = {}
    for v in best.values():
        counts[v[4]] = counts.get(v[4], 0) + 1
    breakdown = ' + '.join('%d %s' % (n, k) for k, n in sorted(counts.items()))

    # Geometry checks -- identical to the byte gate: per-function diffing cannot
    # see an overlap or two functions claiming one address.
    spans = sorted((va, va + len(c), n)
                   for va, (n, c, _u, _f, _l) in usable.items())
    overlaps = [(a, b) for a, b in zip(spans, spans[1:]) if a[1] > b[0]]
    conflicting = [(va, sorted(ns)) for va, ns in sorted(names_at.items())
                   if len(ns) > 1 and va in usable]

    print('=' * 68)
    print('BRGlide.dll  (contract-valid: T3 + T4)')
    print('=' * 68)
    print(f"functions compiled and addressed : {len(best)}  ({breakdown})")
    print(f"  every relocation resolved      : {len(usable)}")
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

    base, secs = ib.read_pe_text_info(orig_path)
    img = bytearray(open(orig_path, 'rb').read())
    text = [s for s in secs if s[0].startswith('.text')][0]
    _, trva, tvsize, traw, _ = text

    outside = []
    regressions = []            # a T4 span that differs -- FATAL
    t3_diffs = []               # a T3 span that differs -- expected residue
    t3_residue = 0
    placed = bytes_placed = 0
    for va, (name, code, _u, _f, lane) in sorted(usable.items()):
        off = va - base - trva
        if off < 0 or off + len(code) > tvsize:
            outside.append((va, name))
            continue
        fo = traw + off
        if img[fo:fo + len(code)] != code:
            nd = sum(1 for i in range(len(code)) if img[fo + i] != code[i])
            if va in t3_vas:
                t3_diffs.append((va, name, nd, len(code)))
                t3_residue += nd
            else:
                regressions.append((va, name, nd, len(code)))
        img[fo:fo + len(code)] = code
        placed += 1
        bytes_placed += len(code)

    n_t3_placed = sum(1 for va in usable if va in t3_vas)
    print(f"\nplaced into the image            : {placed} functions, "
          f"{bytes_placed:,} bytes ({100*bytes_placed/tvsize:.2f}% of .text)")
    print(f"  landed outside .text           : {len(outside)}")
    for va, name in outside[:10]:
        print(f"    {va:#010x} {name}")
    print(f"\nT3 (contract-valid) functions placed : {n_t3_placed}")
    print(f"  T3 residue vs original bytes       : {t3_residue:,} bytes "
          f"across {len(t3_diffs)} function(s)  (EXPECTED -- T3 is not "
          f"byte-exact)")
    for va, name, nd, sz in sorted(t3_diffs, key=lambda x: -x[2])[:10]:
        print(f"    {va:#x} {name}: {nd}/{sz} bytes")
    print(f"\nT4 (byte-exact) spans that REGRESSED : {len(regressions)}")
    for va, name, nd, sz in sorted(regressions, key=lambda x: -x[2])[:10]:
        print(f"    {va:#x} {name}: {nd}/{sz} bytes  <-- a match no longer holds")

    # A wrong claim is a decomp defect; a tree that will not compile says nothing
    # about the claims.  Both exit non-zero, but they call for opposite actions,
    # so name which.  T3 residue is in NEITHER bucket.
    claims_bad = bool(regressions or overlaps or conflicting or outside
                      or unplaced or blocked)
    if claims_bad:
        verdict = 'claims'
        print("\n  -> FAILED: the contract-valid claims do not hold at image "
              "level.")
        if regressions:
            print("     A byte-exact (T4) function regressed -- this is the "
                  "byte gate's own failure, surfaced here.")
    elif unbuildable:
        verdict = 'build'
        print(f"\n  -> INCONCLUSIVE: {len(unbuildable)} claimed function(s) "
              "could not be compiled, so they were never graded.")
        print("     Everything that DID build holds. Fix the source (or wait "
              "for whoever is mid-edit) and re-run.")
    else:
        verdict = 'ok'
        print("\n  -> every contract-valid claim holds: the T4 backbone is "
              "byte-exact and every T3 function compiles and places.")
        print("     The image differs from the original ONLY inside T3 bodies "
              f"({t3_residue:,} bytes). That is Milestone 1.")
    return bytes(img), verdict


def main():
    argv = sys.argv[1:]

    def opt(flag, default=None):
        return argv[argv.index(flag) + 1] if flag in argv else default

    out_dir = opt('--out-dir', ib.OUT_DIR)
    no_write = '--no-write' in argv
    recompile = '--recompile' in argv
    t3_only = '--t3-only' in argv

    before = ib._source_stamp()

    if t3_only:
        t4_best, names_at, t4_unplaced, t4_unbuildable = {}, {}, [], []
        print('  --t3-only: the T4 backbone is not rebuilt; regressions in it '
              'are NOT checked this run.')
    else:
        def dprog(n, total, f):
            sys.stderr.write('\r  T4 backbone %d/%d %-40s'
                             % (n, total, os.path.basename(f)))
            sys.stderr.flush()
        t4_best, names_at, t4_unplaced, t4_unbuildable = ib.collect_dll(
            recompile, dprog)
        sys.stderr.write('\r' + ' ' * 70 + '\r')

    def tprog(n, total, f):
        sys.stderr.write('\r  T3 functions %d/%d %-40s'
                         % (n, total, os.path.basename(f)))
        sys.stderr.flush()
    t3_best, t3_unplaced, t3_unbuildable = collect_t3(recompile, tprog)
    sys.stderr.write('\r' + ' ' * 70 + '\r')

    for va, (name, *_rest) in t3_best.items():
        names_at.setdefault(va, set()).add(name)

    img, verdict = assemble_contract(
        ib.ORIG_DLL, t4_best, t3_best, names_at,
        t4_unplaced + t3_unplaced, t4_unbuildable + t3_unbuildable)

    dest = os.path.join(out_dir, 'BRGlide.T3.dll')
    ib.emit(img, verdict == 'ok', dest, no_write)

    raced = ib._raced(before, ib._source_stamp())
    print()
    print('=' * 68)
    WORD = {'ok': 'OK', 'claims': 'FAILED', 'build': 'INCONCLUSIVE (build)'}
    print('  BRGlide.dll (T3+)   %s' % WORD[verdict])
    if raced:
        print('\n‼ THE TREE CHANGED WHILE THIS RUN WAS GRADING IT '
              '(%d file(s)):' % len(raced))
        for p in raced[:8]:
            print('    %s' % ib._show(p))
        if len(raced) > 8:
            print('    ... and %d more' % (len(raced) - 8))
        print('  This verdict describes no single state of the tree. Re-run '
              'when the tree is still.')
        sys.exit(2)
    if verdict == 'claims':
        print('\nCONTRACT-VALID GATE FAILED')
        sys.exit(1)
    if verdict == 'build':
        print('\nCONTRACT-VALID GATE INCONCLUSIVE -- claimed functions that '
              'would not compile were never graded.')
        sys.exit(1)
    print('\nCONTRACT-VALID (T3+) GATE PASSED')


if __name__ == '__main__':
    main()
