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
    from reloc_fill import _load_overrides
    from reloc_pair import (pair_function, audit_function, jump_table_slots,
                            content_anchor_syms, import_thunk_syms,
                            _our_sites)

    def _site_symbols(obj, name, va, size):
        st, _ = _our_sites(obj, name, size)
        return [((va, s[0]), s[1]) for s in (st or [])]

    recovered = {}          # global-name pairing results, cross-function

    _SLOT_OK = []

    def _slot_ok():
        if not _SLOT_OK:
            p = os.path.join(ROOT, 'config', 't3_slot_ok.csv')
            vas = set()
            if os.path.exists(p):
                for row2 in csv.DictReader(open(p)):
                    try:
                        vas.add(int(row2['va'], 16))
                    except (ValueError, KeyError):
                        pass
            _SLOT_OK.append(vas)
        return _SLOT_OK[0]

    # FORCE-BLOCKED rows: a certified function whose PLACED span the in-image
    # A5 run rejected (a real behavioural divergence, not a modelling gap)
    # must not ship, whatever the contract gates say.  One row per VA in
    # config/t3_blocked.csv with the evidence; delete the row when the lane
    # that caused it is fixed and the placed-image run agrees.
    blocked = {}
    bp = os.path.join(ROOT, 'config', 't3_blocked.csv')
    if os.path.exists(bp):
        for row2 in csv.DictReader(open(bp)):
            try:
                blocked[int(row2['va'], 16)] = row2.get('evidence', '')
            except (ValueError, KeyError):
                pass

    rows = {}
    for r in csv.DictReader(open(REPORT)):
        if r.get('status') == 'diff' and r.get('va') \
                and r['va'].lower() in cert:
            va2 = int(r['va'], 16)
            if va2 in blocked:
                print('  FORCE-BLOCKED 0x%08X %s: %s'
                      % (va2, r['name'], blocked[va2]))
                continue
            rows[va2] = r

    # Same basename-collision guard the byte gate uses: two source files sharing
    # a basename cannot share the sweep's object cache, so build those ourselves.
    ambiguous = ib._ambiguous_basenames(rows)

    best, unplaced, unbuildable = {}, [], []
    # Per-function opt override (config/t3_variant.csv): the sweep records
    # the raw-byte-min variant, which for a handful of functions is LONGER
    # than the slot while another variant fits.  The override changes which
    # compile places -- lockstep rows must be regenerated against it.
    variant = {}
    vp = os.path.join(ROOT, 'config', 't3_variant.csv')
    if os.path.exists(vp):
        for vr in csv.DictReader(open(vp)):
            try:
                variant[int(vr['va'], 16)] = vr['opt']
            except (ValueError, KeyError):
                pass
    want = {}
    for va, r in rows.items():
        opt = variant.get(va) or r.get('opt')
        if not opt:
            unbuildable.append((va, r['name'],
                                '%s: diff row carries no opt' % r['file']))
            continue
        if va in variant:
            r = dict(r)
            r['opt'] = opt
            rows[va] = r
        want.setdefault((r['file'], opt), []).append((va, r['name']))

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
        if sym.lstrip('_').startswith('imp_'):
            if sym in imports:
                return imports[sym]
            base_sym = sym.split('@')[0]     # __imp__mmioGetInfo@12 -> table key
            if base_sym in imports:
                return imports[base_sym]
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
            # forced per-site evidence beats every name-derived value --
            # but NEVER an identity slot (a jump-table / const slot is exact
            # from the object's own symbol table, while pairing and the
            # audit compare against the ORIGINAL's operands, whose layout
            # differs: BrObjDlBuild's switch tables shipped the original's
            # table addresses, +0x194 into OUR body, and the placed image
            # dispatched through garbage while the obj oracle said
            # EQUIVALENT).
            for k, v in paired.items():
                if k in idsites:
                    continue                     # exact; pairing is blind
                if site_syms.get(k) is not None:
                    rkey = ((obj, site_syms[k]) if '$' in site_syms[k]
                            else site_syms[k])
                    if recovered.get(rkey, (0,))[0] is None:
                        continue                 # conflicted pairing
                sites[k] = v
            for k, v in fixes.items():
                if k in idsites:
                    continue                     # exact; audit is blind
                sites[k] = v
            # config/reloc_overrides.csv LAST and WINNING: the sanctioned
            # per-site hand channel.  The pairing and the audit are
            # register-blind -- BrFadeTick's role-swapped slots were paired
            # positionally WRONG and silently overrode the dataflow-derived
            # hand rows, shipping three crossed cells while the oracle path
            # (fill_function, where the CSV wins) said EQUIVALENT.  One
            # precedence order for both consumers: the CSV outranks machine
            # evidence, and every disagreement is printed for review.
            for (ova, ooff), oval in _load_overrides().items():
                if ova != va:
                    continue
                k = (va, ooff)
                if k in sites and sites[k] != oval:
                    print('  NOTE %s off %#x: CSV row %#x overrides '
                          'machine value %#x -- if the CSV row is machine-'
                          'generated, regenerate it'
                          % (name, ooff, oval, sites[k]))
                sites[k] = oval
        # TRUNCATION GUARD.  compiled_functions slices the original-size
        # window out of a longer recompilation, cutting its tail --
        # BrGbiSizeShift lost its final `ret` and fell into the next
        # function.  The splice is sound ONLY when the cut bytes equal what
        # the image already holds at those addresses (shared epilogues often
        # do); anything else blocks the function.
        from reloc_fill import parse as rf_parse, func_symbol_matches
        try:
            od, osecs, osyms, orl = rf_parse(obj)
        except Exception:
            od = None
        for va, name in wanted:
            if od is None:
                break
            fnsym = next((s2 for s2 in osyms
                          if func_symbol_matches(s2['name'], name)
                          and osecs.get(s2['sec'], {}).get('name', '')
                                  .startswith('.text')), None)
            if fnsym is None:
                continue
            sec2 = osecs[fnsym['sec']]
            # bound by the next FUNCTION symbol only: a `$L` case label or
            # `$T` constant is INSIDE this function, and counting it made
            # the guard measure BrCtlInputApply at 231 of its 3300 bytes --
            # a truly over-slot body shipped silently truncated.
            nx = [s2['val'] for s2 in osyms
                  if s2['sec'] == fnsym['sec'] and s2['val'] > fnsym['val']
                  and '$' not in s2['name']]
            end2 = min(nx) if nx else sec2['size']
            bod = od[sec2['praw'] + fnsym['val']:sec2['praw'] + end2]
            code_len = len(bod)
            while code_len and bod[code_len - 1] in (0x90, 0xCC):
                code_len -= 1
            size = int(rows[va]['orig_size'])
            pre = ib.PREAMBLES.get('0x%08x' % va, b'')
            slot = size - len(pre)
            if code_len <= slot:
                continue
            over = bod[slot:code_len]
            keep = bytes(img.byte(va + len(pre) + slot + i) or 0
                         for i in range(len(over)))
            # the overhang carries relocation slots whose values would have
            # been filled; a byte-compare on raw obj bytes is only sound
            # when no reloc lands in the overhang
            reloc_in_over = any(slot <= (rva2 - fnsym['val']) < code_len
                                for rva2, _si2, _rt2 in orl.get(fnsym['sec'],
                                                                []))
            if over == keep and not reloc_in_over:
                print('  TRUNCATION OK %s: %dB overhang is byte-identical '
                      'to the image tail' % (name, len(over)))
                continue
            if va in _slot_ok():
                # the A5 oracle ran the PLACED span -- spliced tail and all
                # -- and returned EQUIVALENT; per the certification standard
                # the behavioural verdict outranks byte conservatism.  The
                # evidence row lives in config/t3_slot_ok.csv.
                print('  TRUNCATION ADMITTED %s: %dB overhang, placed-image '
                      'A5 EQUIVALENT on record' % (name, len(over)))
                continue
            print('  TRUNCATED %s: %dB of code past the slot differ from '
                  'the image -- BLOCKED (no placed-image A5 verdict)'
                  % (name, len(over)))
            byname.pop(name, None)       # never accept its placement
        if os.environ.get('BR_DUMP_SITES'):
            with open(os.environ['BR_DUMP_SITES'], 'a') as f:
                for (sva, soff), sval in sorted(sites.items()):
                    f.write('0x%08X,0x%X,0x%08X\n' % (sva, soff, sval))
        for va, name, code, unres, fromref in ib.compiled_functions(
                [obj], fnmap, {}, pad_short=True, ref_fill=False,
                extra_sites=sites):
            if byname.get(name) == va:
                best[va] = (name, code, unres, fromref, 'T3')
                got.add(va)
        for va, name in wanted:
            if va not in got:
                unplaced.append((va, name, rel_src + ': symbol not in obj'))
    # ------------------------------------------------ C++ lane T3 rows ---
    # Fifteen certified functions live only in report_cpp.csv (their
    # transcriptions are src/core/cpp/<VA>.cpp).  Same placement rules, the
    # lane's own pre-built sweep objects and mangled symbols.
    import cpp_score
    from relocmap import REL_REL32
    tags_cpp = [ib._opt_tag(o) for o in cpp_score.DEFAULT_OPTS]
    repcpp = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    cpp_rows = []
    if os.path.exists(repcpp):
        for r in csv.DictReader(open(repcpp)):
            v = (r.get('va') or '').lower()
            if r.get('status') == 'diff' and v in cert \
                    and int(v, 16) not in rows:
                cpp_rows.append(r)
    for r in cpp_rows:
        va = int(r['va'], 16)
        size = int(r['orig_size'])
        base = os.path.splitext(os.path.basename(r['file']))[0]
        try:
            ti = tags_cpp.index(r['opt'])
        except ValueError:
            unbuildable.append((va, r['name'],
                                'cpp opt %r unknown' % r.get('opt')))
            continue
        obj = os.path.join(cpp_score.OBJ_DIR,
                           '%s_sweep_%08X_%d.obj' % (base, va, ti))
        if not os.path.exists(obj):
            unbuildable.append((va, r['name'], 'cpp sweep obj missing'))
            continue
        _impl, symtag, kind = cpp_score.parse_implements_name(
            os.path.join(ROOT, r['file']), va)
        raw = ib._raw_symbol(obj, symtag, kind)
        if raw is None:
            unplaced.append((va, r['name'], 'cpp raw symbol not found'))
            continue
        pre = ib.PREAMBLES.get('0x%08x' % va, b'')
        ob = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        orig_body = open(ob, 'rb').read()[len(pre):]
        anchors = {}
        anchors.update(content_anchor_syms(obj, raw, size, img))
        anchors.update(import_thunk_syms(obj, raw, size, img))
        sites = dict(const_slot_values(obj, raw, va, size))
        sites.update(jump_table_slots(obj, raw, va, size, plen=len(pre)))

        def _knownc(sym, anchors=anchors):
            v = _trusted(sym, anchors)
            return v if v is not None else _hearsay(sym)
        paired, _refc, assigned = pair_function(
            obj, raw, va, size, orig_body, img, _knownc, plen=len(pre))
        fixes, repc, confirmed = audit_function(
            obj, raw, va, size, orig_body, img, _knownc, plen=len(pre))
        for osym, oimg2, omap in repc:
            print('  MAP OVERRIDE %s %s: image says %s, map said %s'
                  % (r['name'], osym,
                     hex(oimg2) if oimg2 is not None else '?', hex(omap)))
        st, _b = _our_sites(obj, raw, size)
        for off, sym, rt2, addend, _k in (st or []):
            k = (va, off)
            if k in sites:
                continue
            a = _trusted(sym, anchors)
            if a is None:
                # The cpp lane's ?g_<HEX>@@ / sub_<HEX> names are coined
                # FROM THE GLIDE BINARY (the transcriptions live in
                # src/core/cpp/<glide-VA>.cpp), so the embedded address is
                # evidence here, not the C lane's D3D-era hearsay --
                # ?g_0A9360 lands exactly on the pairing-proven cell where
                # the C lane's stale name missed by 0xCB0.
                a = address_in_name(sym)
            if a is None:
                ck = (obj, sym) if '$' in sym else sym
                a = g_corr.get(ck)
                if a is None and _hearsay(sym) is not None \
                        and g_conf.get(ck) == _hearsay(sym):
                    a = _hearsay(sym)
                if a is None and sym in confirmed:
                    a = confirmed[sym]
            if a is None:
                continue
            if rt2 == REL_REL32:
                sites[k] = (a + addend - (va + len(pre) + off + 4)) \
                    & 0xFFFFFFFF
            else:
                sites[k] = (a + addend) & 0xFFFFFFFF
        # identity slots (const / jump-table) are exact from the object's
        # symbol table; pairing and the audit compare against the ORIGINAL's
        # layout and must not clobber them (same guard as the base lane).
        idkeys = set(const_slot_values(obj, raw, va, size))
        idkeys.update(jump_table_slots(obj, raw, va, size, plen=len(pre)))
        for k, v in paired.items():
            if k in idkeys:
                continue
            sites[k] = v
        for k, v in fixes.items():
            if k in idkeys:
                continue
            sites[k] = v
        for (ova, ooff), oval in _load_overrides().items():
            if ova == va:
                sites[(va, ooff)] = oval
        # truncation guard, cpp flavour
        try:
            od2, osecs2, osyms2, orl2 = rf_parse(obj)
            fnsym2 = next((s2 for s2 in osyms2
                           if func_symbol_matches(s2['name'], raw)
                           and osecs2.get(s2['sec'], {}).get('name', '')
                                   .startswith('.text')), None)
        except Exception:
            fnsym2 = None
        if fnsym2 is not None:
            sec3 = osecs2[fnsym2['sec']]
            nx2 = [s2['val'] for s2 in osyms2
                   if s2['sec'] == fnsym2['sec']
                   and s2['val'] > fnsym2['val']
                   and '$' not in s2['name']]   # $-labels live INSIDE the fn
            end3 = min(nx2) if nx2 else sec3['size']
            bod2 = od2[sec3['praw'] + fnsym2['val']:sec3['praw'] + end3]
            clen = len(bod2)
            while clen and bod2[clen - 1] in (0x90, 0xCC):
                clen -= 1
            slot2 = size - len(pre)
            if clen > slot2 and va not in _slot_ok():
                over2 = bod2[slot2:clen]
                keep2 = bytes(img.byte(va + len(pre) + slot2 + i) or 0
                              for i in range(len(over2)))
                if over2 != keep2:
                    print('  TRUNCATED %s (cpp): %dB past the slot differ '
                          '-- BLOCKED' % (r['name'], len(over2)))
                    unplaced.append((va, r['name'], 'cpp overhang'))
                    continue
        got_it = False
        for va2, name2, code, unres2, fromref2 in ib.compiled_functions(
                [obj], fnmap, {}, only={raw: va}, pad_short=True,
                ref_fill=False, extra_sites=sites):
            if va2 == va:
                best[va] = (r['name'], code, unres2, fromref2, 'T3')
                got_it = True
        if not got_it:
            unplaced.append((va, r['name'], 'cpp symbol not placed'))
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
