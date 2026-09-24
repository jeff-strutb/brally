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
import concurrent.futures
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import image_build as ib                                        # noqa: E402

STALE_ROWS = []      # (va, name, offset, value): CSV rows at no relocation
from reloc_fill import load_maps                                # noqa: E402
from t3 import certified                                        # noqa: E402

REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ANNEX_MANIFEST = os.path.join(ib.OUT_DIR, 't3_annex.csv')


def _annex_rva(orig_path):
    """RVA where the annex section starts: first section-aligned address past
    every existing section.  Computed identically by the collector (to assign
    body addresses) and the assembler (to write the section header)."""
    import struct as _s
    d = open(orig_path, 'rb').read()
    pe = _s.unpack_from('<I', d, 0x3c)[0]
    nsec = _s.unpack_from('<H', d, pe + 6)[0]
    optsz = _s.unpack_from('<H', d, pe + 20)[0]
    salign = _s.unpack_from('<I', d, pe + 24 + 32)[0]
    end = 0
    o = pe + 24 + optsz
    for _ in range(nsec):
        vsz, va = _s.unpack_from('<II', d, o + 8)
        end = max(end, va + vsz)
        o += 40
    return (end + salign - 1) // salign * salign


def collect_t3(recompile=False, progress=None, jobs=None):
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
    certinfo = certified()
    cert = {va for va, i in certinfo.items()
            if not va.startswith('?') and i['ok']}

    def _a5_proven(va):
        """True when this cert's @t3-measure records an A5 EQUIVALENT (or
        EQUIV-MODULO-FP) verdict -- the proof RAN, with augment_maps'
        addresses, so placing through the same reader is placing with
        exactly the addresses the equivalence proof used."""
        i = certinfo.get('0x%08x' % va)
        m = i.get('measure') if i else None
        return bool(m) and str(m[-1]).upper().startswith('EQUIV')

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
    from reloc_fill import parse as rf_parse, func_symbol_matches
    from reloc_pair import (pair_function, audit_function, jump_table_slots,
                            content_anchor_syms, import_thunk_syms,
                            _our_sites)

    def _site_symbols(obj, name, va, size):
        st, _ = _our_sites(obj, name, size)
        return [((va, s[0]), s[1]) for s in (st or [])]

    recovered = {}          # global-name pairing results, cross-function

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
            # The cert names the transcription that was qualified; a report
            # row from a DIFFERENT file is a twin graded at this VA by the
            # d3d<->glide alias (e.g. 0x10062B80: the C twin is a cdecl free
            # function, the certified body a `ret 0x10` thiscall -- placing
            # the twin unbalances the stack at every reference call site).
            # Skipping it here lets the cpp-lane pass below place the
            # certified body.
            cfile = certinfo[r['va'].lower()].get('file')
            if cfile and cfile != r['file']:
                print('  TWIN-DEFERRED 0x%08X %s: report row is %s, cert '
                      'lives in %s' % (va2, r['name'], r['file'], cfile))
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
    # config/t3_variant_c.csv: the C-row compile pin tools/t3.py grades and
    # the live oracle runs (a TU's real options, e.g. /O2 /Op for tu_022).
    # Placing any other compile ships code nobody verified: 0x10021C70's
    # plain /O2 object dropped /Op's rounding points and drew a 1-ulp colour
    # difference at frame 345 while its /Op object was EQUIVALENT
    # (2026-09-24).  t3_variant.csv still wins where both name a VA.
    vpc = os.path.join(ROOT, 'config', 't3_variant_c.csv')
    if os.path.exists(vpc):
        for vr in csv.DictReader(open(vpc)):
            try:
                variant.setdefault(int(vr['va'], 16), vr['opt'])
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

    # Compile every T3 TU up front, in parallel -- same rationale as the
    # backbone in image_build.collect_dll: the wine/MSVC compile is the cost
    # and each (file, opt) writes its own directory-keyed object, so the
    # workers share no output.  The pairing/audit that follows reads those
    # objects and MUTATES shared evidence (g_conf/g_corr/recovered) in claim
    # order, so it stays serial -- only the compile fans out.
    if jobs is None:
        jobs = int(os.environ.get('BR_JOBS') or 0) or min(8, (os.cpu_count() or 4))
    _items = sorted(want.items())
    _compiled = {}
    if jobs <= 1 or len(_items) <= 1:
        for i, ((rel_src, tag), _w) in enumerate(_items):
            if progress:
                progress(i + 1, len(_items), rel_src)
            _compiled[(rel_src, tag)] = ib._compile_dll_obj(
                rel_src, tag, recompile, ambiguous)
    else:
        _done = 0
        with concurrent.futures.ProcessPoolExecutor(max_workers=jobs) as ex:
            futs = {ex.submit(ib._compile_dll_obj, rel_src, tag, recompile,
                              ambiguous): (rel_src, tag)
                    for (rel_src, tag), _w in _items}
            for fut in concurrent.futures.as_completed(futs):
                _compiled[futs[fut]] = fut.result()
                _done += 1
                if progress:
                    progress(_done, len(_items), futs[fut][0])

    for i, ((rel_src, tag), wanted) in enumerate(_items):
        obj, err, _how = _compiled[(rel_src, tag)]
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

    # ------------------------------------------------ annex placement ----
    # A certified body that compiles LONGER than the original's byte-slot
    # cannot be laid at its own VA without truncating real code (the crash
    # class the placed-image sweep exposed).  Certification is not the
    # question -- the slot is.  So the WHOLE body is placed in a fresh
    # executable section appended to the image (the "annex") and the
    # original VA gets a 5-byte `jmp annex` thunk; callers and function
    # pointers keep targeting the original VA.  Every site value is
    # computed for the annex address, and the placed-image oracle then
    # verifies the shipped thunk+body like any other span.
    import struct as _st
    from relocmap import REL_REL32 as _REL32
    annex_base_va = 0x10000000 + _annex_rva(ib.ORIG_DLL)
    annex_cursor = [annex_base_va]
    annex = []                    # (annex_va, va, name, body_bytes)

    # -- BR_TRACE: comprehensive execution trace --------------------------
    # With BR_TRACE=1 every annexable transcribed function is routed through a
    # stub `push VA; call BrDiagTrace; jmp body` that appends its VA to
    # brally.log.  Any crash's last log line is then the function it died in --
    # no guessing, no re-instrumentation.  The sink is __stdcall so the stub is
    # stack-transparent; fclose flushes each line so a crash keeps the trace
    # (which makes a full-trace run SLOW -- the price of comprehensiveness).
    from reloc_fill import parse as _rfp, func_symbol_matches as _fsm
    _trace_on = os.environ.get('BR_TRACE') == '1'
    # BR_TRACE_ONLY=<comma VAs>: stub ONLY these functions.  The crash filter
    # installs from any traced line, so listing one early, non-hot function
    # arms the filter at startup while leaving every hot per-texel/per-pixel
    # loop UNtraced -- the game then runs at normal speed and a fault yields a
    # single clean `C ...` line (no fopen-per-texel death).  Empty = trace all.
    _trace_only = set()
    _to_env = os.environ.get('BR_TRACE_ONLY', '').strip()
    if _to_env:
        for _tok in _to_env.replace(',', ' ').split():
            try:
                _trace_only.add(int(_tok, 16))
            except ValueError:
                pass
    # BR_TRACE_EXCEPT=<comma VAs>: trace everything EXCEPT these (a denylist,
    # the complement of BR_TRACE_ONLY).  Use it to drop the per-texel/per-vertex
    # leaf spam (BrTex3dTexel 0x100271f0, BrMtxXfmDir3 0x10034a70) while keeping
    # the full breadcrumb -- so a hang/crash still self-localizes at tolerable
    # speed.  BR_TRACE_ONLY, if set, takes precedence.
    _trace_except = set()
    _te_env = os.environ.get('BR_TRACE_EXCEPT', '').strip()
    if _te_env:
        for _tok in _te_env.replace(',', ' ').split():
            try:
                _trace_except.add(int(_tok, 16))
            except ValueError:
                pass
    # BR_GOLDEN=1: build the WORKING game with the same logging -- every traced
    # function uses the ORIGINAL bytes (relocated into the annex) instead of our
    # transcription, so its trace is the ground-truth reference to diff against.
    _golden = os.environ.get('BR_GOLDEN') == '1'
    if _golden:
        from capstone import Cs as _Cs, CS_ARCH_X86 as _A, CS_MODE_32 as _M, \
            x86 as _x86
        _gmd = _Cs(_A, _M); _gmd.detail = True
        _god = open(ib.ORIG_DLL, 'rb').read()
        _gpe = _st.unpack_from('<I', _god, 0x3c)[0]
        _gopt = _st.unpack_from('<H', _god, _gpe + 20)[0]; _gb = _gpe + 24 + _gopt
        for _gi in range(_st.unpack_from('<H', _god, _gpe + 6)[0]):
            _o = _gb + _gi * 40
            if _god[_o:_o + 8].rstrip(b'\0') == b'.text':
                _gtv = _st.unpack_from('<I', _god, _o + 12)[0]
                _gtro = _st.unpack_from('<I', _god, _o + 20)[0]
        _gvas = sorted(int(r['va'], 16) for _rp in
                       ('build/match/report.csv', 'build/match/report_cpp.csv')
                       if os.path.exists(os.path.join(ROOT, _rp))
                       for r in csv.DictReader(open(os.path.join(ROOT, _rp)))
                       if r.get('va'))

        def _orig_full(va):
            """Original bytes of the function, code + trailing jump tables,
            bounded by the next function symbol."""
            j = _gvas.index(va) if va in _gvas else None
            nxt = _gvas[j + 1] if (j is not None and j + 1 < len(_gvas)) \
                else va + 0x4000
            off = _gtro + (va - 0x10000000 - _gtv)
            return _god[off:off + (nxt - va)]

        def _relocate_golden(va, a_va, body):
            b = bytearray(body); delta = a_va - va; end = va + len(body)
            for ins in _gmd.disasm(bytes(body), va):
                off = ins.address - va
                if ins.operands and ins.operands[0].type == _x86.X86_OP_IMM \
                        and (ins.mnemonic in ('call', 'jmp') or
                             ins.mnemonic[0] == 'j') and ins.size in (5, 6):
                    tgt = ins.operands[0].imm & 0xFFFFFFFF
                    if not (va <= tgt < end):
                        nr = (tgt - (a_va + off + ins.size)) & 0xFFFFFFFF
                        _st.pack_into('<I', b, off + ins.size - 4, nr)
                for op in ins.operands:
                    if op.type == _x86.X86_OP_MEM and op.mem.disp and \
                            va <= (op.mem.disp & 0xFFFFFFFF) < end and \
                            ins.mnemonic in ('jmp', 'call'):
                        disp = op.mem.disp & 0xFFFFFFFF; toff = disp - va
                        while toff + 4 <= len(body):
                            e = _st.unpack_from('<I', body, toff)[0]
                            if va <= e < end:
                                _st.pack_into('<I', b, toff,
                                              (e + delta) & 0xFFFFFFFF)
                                toff += 4
                            else:
                                break
                        if ins.disp_offset:
                            _st.pack_into('<I', b, off + ins.disp_offset,
                                          (disp + delta) & 0xFFFFFFFF)
            return bytes(b)
    _sink_va = [None]
    _texsink_va = [None]     # value-logging sink for br_tex3d_append
    if _trace_on:
        from t3b_env import image as _timage
        from reloc_pair import _our_sites as _t_sites
        from relocmap import REL_DIR32 as _T_DIR32
        _im = _timage().imports
        _t_imports = _im() if callable(_im) else _im
        _tobj, _terr, _ = ib._compile_dll_obj('src/core/diag/br_trace.c',
                                              'O2', True, ())
        if _tobj is None:
            print('  BR_TRACE: sink compile failed (%s) -- tracing OFF' % _terr)
            _trace_on = False
        else:
            _td, _ts, _tsy, _tr = _rfp(_tobj)

            # intra-object symbol -> annex VA (populated as sinks are placed);
            # lets a sink reference a sibling annexed function (the crash
            # filter is referenced by DIR32 &BrGlCrashFilter, and, if the
            # install helper is not inlined, by a REL32 call).
            _known_syms = {}

            def _annex_sink(symdec):
                _tf = next((s for s in _tsy if _fsm(s['name'], symdec)
                            and _ts.get(s['sec'], {}).get('name', '')
                                    .startswith('.text')), None)
                if _tf is None:
                    return None, 'symbol not found'
                _tsec = _ts[_tf['sec']]
                _tnx = [s['val'] for s in _tsy if s['sec'] == _tf['sec']
                        and s['val'] > _tf['val'] and '$' not in s['name']]
                _tend = min(_tnx) if _tnx else _tsec['size']
                _tbody = bytearray(_td[_tsec['praw'] + _tf['val']:
                                       _tsec['praw'] + _tend])
                _sva = (annex_cursor[0] + 15) & ~15
                _tsl, _ = _t_sites(_tobj, symdec, _tend - _tf['val'])
                for _o, _sy, _rt, _ad, _k in (_tsl or []):
                    # a sibling annexed function wins over the import table;
                    # then the IAT (import names carry no @N, so also try the
                    # @-stripped form).
                    _sl = (_known_syms.get(_sy)
                           or _known_syms.get(_sy.split('@', 1)[0])
                           or _t_imports.get(_sy)
                           or _t_imports.get(_sy.split('@', 1)[0]))
                    if _sl is None:
                        return None, 'unresolved %s' % _sy
                    _val = (_sl + _ad) & 0xFFFFFFFF if _rt == _T_DIR32 else \
                           (_sl + _ad - (_sva + _o + 4)) & 0xFFFFFFFF
                    _st.pack_into('<I', _tbody, _o, _val)
                annex.append((_sva, 0, symdec.strip('_@').split('@')[0],
                              bytes(_tbody)))
                annex_cursor[0] = _sva + len(_tbody)
                _known_syms[symdec] = _sva
                _known_syms[symdec.split('@', 1)[0]] = _sva
                return _sva, None

            # The crash filter must be annexed FIRST so its VA is known when
            # the sinks (which reference it) are placed.  It only calls
            # imports, so it never depends on a sibling.
            _cfva, _cfwhy = _annex_sink('_BrGlCrashFilter@4')
            if _cfva is None:
                print('  BR_TRACE: crash filter failed (%s) -- tracing OFF'
                      % _cfwhy)
                _trace_on = False
            else:
                _sva, _why = _annex_sink('_BrDiagTrace@8')
            if _trace_on and _sva is None:
                print('  BR_TRACE: sink failed (%s) -- tracing OFF' % _why)
                _trace_on = False
            elif _trace_on:
                _sink_va[0] = _sva
                _tvsva, _tvwhy = _annex_sink('_BrDiagTexState@8')
                if _tvsva is not None:
                    _texsink_va[0] = _tvsva
                print('  BR_TRACE ON: sink 0x%08X, tex-state sink %s, crash '
                      'filter 0x%08X (all transcribed fns traced; slow)'
                      % (_sva, ('0x%08X' % _tvsva) if _tvsva else 'OFF',
                         _cfva))

    _amaps = {}

    def _annex_fill(obj, symname, disp_name, va, plen, sites, anchors,
                    trust_name_addr=False):
        """Build the full body for the annex; returns (annex_va, bytes) or
        (None, why).  Values: identity slots recomputed at the annex base;
        phase-B values rebased by relocation type (a REL32 displacement
        shifts by the placement delta, an absolute DIR32 does not); anything
        still unnamed resolves through the same trusted/confirmed channels,
        else the function blocks -- honestly, never truncated."""
        try:
            d2, secs2, syms2, rel2 = rf_parse(obj)
        except Exception as e:
            return None, 'parse: %s' % e
        fs = next((s2 for s2 in syms2
                   if func_symbol_matches(s2['name'], symname)
                   and secs2.get(s2['sec'], {}).get('name', '')
                           .startswith('.text')), None)
        if fs is None:
            return None, 'symbol not in obj'
        sec2 = secs2[fs['sec']]
        nx = [s2['val'] for s2 in syms2
              if s2['sec'] == fs['sec'] and s2['val'] > fs['val']
              and '$' not in s2['name']]
        end2 = min(nx) if nx else sec2['size']
        full = end2 - fs['val']
        body = bytearray(d2[sec2['praw'] + fs['val']:sec2['praw'] + end2])
        a_va = (annex_cursor[0] + 15) & ~15
        ident = dict(const_slot_values(obj, symname, va, full))
        jt = jump_table_slots(obj, symname, a_va, full, plen=0)
        jt_off = {off2 for (_v2, off2) in jt}
        jt_val = {off2: v2 for (_v2, off2), v2 in jt.items()}
        st2, _b2 = _our_sites(obj, symname, full)
        blocked2 = []
        for off, sym, rt, addend, _k2 in (st2 or []):
            val = None
            if off in jt_off:
                val = jt_val[off]
            elif (va, off) in ident:
                # a $T constant located by content in the reference image:
                # absolute, position-independent
                val = ident[(va, off)]
            elif '$' in sym and rt == REL_DIR32:
                # a $-label DIR32 the jump-table reader did NOT claim: a
                # cross-section label (an EH handler/funclet).  Its phase-B
                # value is an ABSOLUTE address into the image -- the same
                # dword the in-slot placement shipped -- and rebasing does
                # not apply to an absolute reference, so carry it.  Only a
                # slot with no phase-B value at all blocks.
                if (va, off) in sites:
                    val = sites[(va, off)]
                else:
                    blocked2.append((off, sym))
                    continue
            elif (va, off) in sites:
                v2 = sites[(va, off)]
                if rt == _REL32:
                    val = (v2 + (va + plen) - a_va) & 0xFFFFFFFF
                else:
                    val = v2
            else:
                a2 = _trusted(sym, anchors)
                if a2 is None and trust_name_addr:
                    # cpp-lane convention names carry the GLIDE address
                    # (the transcriptions live in src/core/cpp/<VA>.cpp),
                    # same evidence rule as the in-slot cpp placement
                    a2 = address_in_name(sym)
                if a2 is None:
                    ck2 = (obj, sym) if '$' in sym else sym
                    a2 = g_corr.get(ck2)
                    if a2 is None and _hearsay(sym) is not None \
                            and g_conf.get(ck2) == _hearsay(sym):
                        a2 = _hearsay(sym)
                if a2 is None and _a5_proven(va):
                    # the certification's OWN resolution: augment_maps reads
                    # the transcription's declared `/* 0x... */` VAs and
                    # address-bearing names -- the addresses the A5 proof
                    # ran under.  Consulted only for A5-proven certs; a
                    # byte-shape-only cert still blocks here, and the
                    # placed-image sweep remains the final arbiter.
                    if _amaps.get('key') != (obj, symname):
                        try:
                            af2, ag2 = augment_maps(obj, symname, full)
                        except Exception:
                            af2, ag2 = {}, {}
                        _amaps.update(key=(obj, symname), fn=af2, gl=ag2)
                    a2 = rf_resolve(sym, _amaps['fn'], _amaps['gl'])
                    if a2 is None:
                        a2 = address_in_name(sym)
                if a2 is not None:
                    if rt == _REL32:
                        val = (a2 + addend - (a_va + off + 4)) & 0xFFFFFFFF
                    else:
                        val = (a2 + addend) & 0xFFFFFFFF
            if val is None:
                blocked2.append((off, sym))
                continue
            _st.pack_into('<I', body, off, val & 0xFFFFFFFF)
        if blocked2:
            return None, 'annex slots unresolved: %s' % ', '.join(
                '%#x %s' % (o2, s2) for o2, s2 in blocked2[:3])
        annex_cursor[0] = a_va + len(body)
        annex.append((a_va, va, disp_name, bytes(body)))
        return a_va, None

    def _thunk_span(va, a_va, size):
        """The slot content at the original VA: `jmp annex` then the
        ORIGINAL's remaining bytes (unreached filler, keeps neighbours)."""
        ob2 = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        orig2 = open(ob2, 'rb').read()
        rel = (a_va - (va + 5)) & 0xFFFFFFFF
        return b'\xe9' + _st.pack('<I', rel) + orig2[5:size]

    def _trace_thunk_span(va, a_va, size):
        """24-byte trace stub at the original VA:
            pushad; pushfd; lea eax,[esp+4]; push eax; push VA;
            call sink; popfd; popad; jmp body
        then the original's remaining filler.  pushad/pushfd make the stub
        fully register/flag-transparent (a bare call would corrupt a
        thiscall/fastcall body's ECX/EDX arg -- BrGlNavPoll faulted that way).
        `lea eax,[esp+4]` (after pushad+pushfd, [esp]=eflags) points EAX at the
        pushad frame; it is pushed as the sink's 2nd arg so BrDiagTrace(va,
        frame) can read the caller's ecx/edx and stack args.  eax is restored
        by popad.  The __stdcall sink takes 2 args (ret 8).  Needs size >= 24."""
        ob2 = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        orig2 = open(ob2, 'rb').read()
        # br_tex3d_append gets the value-logging sink (dumps the texture-table
        # count/cap/ptr); everything else the plain arg-logging sink.
        sink = _sink_va[0]
        if va == 0x10027A10 and _texsink_va[0] is not None:
            sink = _texsink_va[0]
        b = b'\x60\x9c'                                              # pushad;pushfd (@0)
        b += b'\x8d\x44\x24\x04'                                     # lea eax,[esp+4] (@2)
        b += b'\x50'                                                 # push eax (@6)
        b += b'\x68' + _st.pack('<I', va & 0xFFFFFFFF)              # push VA (@7)
        b += b'\xe8' + _st.pack('<I', (sink - (va + 17)) & 0xFFFFFFFF)  # call (@12)
        b += b'\x9d\x61'                                             # popfd;popad (@17)
        b += b'\xe9' + _st.pack('<I', (a_va - (va + 24)) & 0xFFFFFFFF)  # jmp (@19)
        return b + orig2[24:size]

    def _annex_golden(va):
        """Golden mode: relocate the ORIGINAL function bytes (code + trailing
        jump tables) into the annex and record them.  Returns (a_va, None)."""
        full = _orig_full(va)
        a_va = (annex_cursor[0] + 15) & ~15
        body = _relocate_golden(va, a_va, full)
        annex_cursor[0] = a_va + len(body)
        annex.append((a_va, va, 'golden_%08x' % va, body))
        return a_va, None

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
            jt_keys = jump_table_slots(obj, name, va, size, plen=plen)
            reloc_offs = {o for o, _s, _r, _a, _k in (st or [])}
            for (ova, ooff), oval in _load_overrides().items():
                if ova != va:
                    continue
                k = (va, ooff)
                if ooff not in reloc_offs:
                    # STALE ROW: no relocation at that offset in THIS object
                    # (the source moved since the row was cut).  Applying it
                    # writes four bytes into the middle of an instruction --
                    # BrRaceStep's tail carried 90 such rows, 2 bytes off,
                    # after a one-statement fix.  Never applied; fails the
                    # gate until the rows are regenerated.
                    STALE_ROWS.append((va, name, ooff, oval))
                    continue
                if k in jt_keys:
                    # A jump-table dispatch displacement is EXACT from the
                    # object symbol table: its value is `va + label_offset`
                    # in OUR body, so a lockstep row's copied ORIGINAL
                    # operand points at the original table and dispatches a
                    # byte-different T3 body mid-instruction (BrRaceStep hit
                    # three of its four switches this way and faulted
                    # in-game).  The CSV outranks register-blind pairing --
                    # never the exact same-section `$L` table.  (Const slots
                    # are NOT guarded: const_slot_values is content-matched
                    # and can mislocate on a shared leading dword -- e.g.
                    # BrFadeTick's float pool $T1315 matched a zero dword in
                    # .text -- so the hand row is the sanctioned correction
                    # there and must still win.)
                    if oval != jt_keys[k]:
                        print('  NOTE %s off %#x: CSV row %#x IGNORED -- '
                              'jump-table slot is exact at %#x'
                              % (name, ooff, oval, jt_keys[k]))
                    continue
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
            if _trace_on and _sink_va[0] is not None and size >= 24 \
                    and (not _trace_only or va in _trace_only) \
                    and va not in _trace_except:
                # BR_TRACE: annex the whole body and leave a trace stub at the
                # VA, whether or not it would have fit in-slot.  Annex failure
                # or a too-small slot falls through to normal placement.
                if _golden:
                    a_va, _tw = _annex_golden(va)
                else:
                    a_va, _tw = _annex_fill(obj, name, name, va, len(pre),
                                            sites, anchors)
                if a_va is not None:
                    byname.pop(name, None)   # keep compiled_functions off it
                    best[va] = (name, _trace_thunk_span(va, a_va, size),
                                0, 0, 'T3')
                    got.add(va)
                    continue
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
            # NO truncation admission.  The old config/t3_slot_ok.csv escape
            # ("placed-image A5 EQUIVALENT") shipped BrGlNavPoll with its
            # final `add esp,0x10; ret 4` cut mid-instruction: the seeds
            # never reached that arm, and on hardware it corrupted esp and
            # FELL THROUGH into 0x100597C0 (the 2026-09-21 credits-screen
            # page fault).  An over-slot body is annexed or it blocks.
            byname.pop(name, None)       # never accept a truncated placement
            a_va, why2 = _annex_fill(obj, name, name, va, len(pre),
                                     sites, anchors)
            if a_va is None:
                print('  TRUNCATED %s: %dB over the slot and the annex '
                      'could not resolve it -- BLOCKED (%s)'
                      % (name, len(over), why2))
                continue
            best[va] = (name, _thunk_span(va, a_va, size), 0, 0, 'T3')
            got.add(va)
            print('  ANNEXED %s: %dB body at 0x%08X, thunk at 0x%08X'
                  % (name, code_len, a_va, va))
        if os.environ.get('BR_DUMP_SITES'):
            with open(os.environ['BR_DUMP_SITES'], 'a') as f:
                for (sva, soff), sval in sorted(sites.items()):
                    f.write('0x%08X,0x%X,0x%08X\n' % (sva, soff, sval))
        # Resolve the placement with the certification's OWN address data --
        # the same augment_maps the annex path (over-slot bodies) already
        # trusts, plus the address-in-name reader compiled_functions documents
        # this lane as passing.  A certified T3 body's residue is a reschedule,
        # so its relocation slots sit at offsets the image-pairing (`sites`)
        # cannot recover; augment_maps supplies the real target from the src
        # `/* 0x<VA> */` declaration, a self-encoding DAT_/Br..._<hex> name, or
        # the CRT import slot -- exactly the addresses the A5 oracle certified
        # each body EQUIVALENT under.  ref_fill stays False: a slot with no
        # known address still blocks the function, never a copied reference
        # dword (the 0x1006E360 page-fault class).
        _taf, _tag = augment_maps(obj, wanted[0][1],
                                  int(rows[wanted[0][0]]['orig_size']))
        for va, name, code, unres, fromref in ib.compiled_functions(
                [obj], _taf, _tag, pad_short=True, ref_fill=False,
                extra_resolve=address_in_name, extra_sites=sites):
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
        jt_keys = jump_table_slots(obj, raw, va, size, plen=len(pre))
        reloc_offs = {o for o, _s, _r, _a, _k in (st or [])}
        for (ova, ooff), oval in _load_overrides().items():
            if ova != va:
                continue
            k = (va, ooff)
            if ooff not in reloc_offs:
                STALE_ROWS.append((va, r['name'], ooff, oval))   # see C lane
                continue
            if k in jt_keys:
                # Jump-table dispatch: exact from the object symbol table.
                # A lockstep row's copied original operand points at the
                # ORIGINAL table and would dispatch a byte-different T3 body
                # mid-instruction (BrRaceStep's in-game switch crashes).
                # Const slots are NOT guarded here: const_slot_values is
                # content-matched and can mislocate, so the hand row is the
                # sanctioned correction and must still win.
                if oval != jt_keys[k]:
                    print('  NOTE %s off %#x: CSV row %#x IGNORED (cpp) -- '
                          'jump-table slot is exact at %#x'
                          % (r['name'], ooff, oval, jt_keys[k]))
                continue
            sites[k] = oval
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
            if _trace_on and _sink_va[0] is not None and size >= 24 \
                    and (not _trace_only or va in _trace_only) \
                    and va not in _trace_except:
                if _golden:
                    a_va2, _tw2 = _annex_golden(va)
                else:
                    a_va2, _tw2 = _annex_fill(obj, raw, r['name'], va, len(pre),
                                              sites, anchors, trust_name_addr=True)
                if a_va2 is not None:
                    best[va] = (r['name'], _trace_thunk_span(va, a_va2, size),
                                0, 0, 'T3')
                    got_it = True
                    continue
            if clen > slot2:
                over2 = bod2[slot2:clen]
                keep2 = bytes(img.byte(va + len(pre) + slot2 + i) or 0
                              for i in range(len(over2)))
                if over2 != keep2:
                    a_va2, why3 = _annex_fill(obj, raw, r['name'], va,
                                              len(pre), sites, anchors,
                                              trust_name_addr=True)
                    if a_va2 is None:
                        print('  TRUNCATED %s (cpp): %dB over the slot and '
                              'the annex could not resolve it -- BLOCKED '
                              '(%s)' % (r['name'], len(over2), why3))
                        unplaced.append((va, r['name'], 'cpp overhang'))
                    else:
                        best[va] = (r['name'], _thunk_span(va, a_va2, size),
                                    0, 0, 'T3')
                        print('  ANNEXED %s (cpp): body at 0x%08X, thunk '
                              'at 0x%08X' % (r['name'], a_va2, va))
                    continue
        got_it = False
        # Same resolution the under-slot C lane gained in f21ec9f5: the
        # augmented maps (address-bearing names, `/* 0x<VA> */` declarations,
        # imports) plus address_in_name.  ref_fill stays False.
        _taf2, _tag2 = augment_maps(obj, raw, size)
        for va2, name2, code, unres2, fromref2 in ib.compiled_functions(
                [obj], _taf2, _tag2, only={raw: va}, pad_short=True,
                ref_fill=False, extra_resolve=address_in_name,
                extra_sites=sites):
            if va2 == va:
                best[va] = (r['name'], code, unres2, fromref2, 'T3')
                got_it = True
        if not got_it and va not in best:
            unplaced.append((va, r['name'], 'cpp symbol not placed'))
    return best, unplaced, unbuildable, annex


def assemble_contract(orig_path, t4_best, t3_best, names_at, unplaced,
                      unbuildable, annex=()):
    """Lay the T4 backbone and the T3 additions into the original image; report;
    return (image_bytes, verdict).

    The one difference from image_build.assemble is the diff classification: a
    byte differing inside a T3 span is the certified residue (reported), a byte
    differing anywhere else is a regression of a byte-exact claim (fatal).
    """
    # A force-annexed MATCHED function ships a `jmp annex` thunk at its VA,
    # which differs from the original bytes on purpose -- count it as expected
    # residue (like a T3 body), not a byte-exact regression.
    t3_vas = set(t3_best) | {v for _av, v, _n, _b in annex}
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
    for va, (n, _c, _u, _f, _l) in sorted(best.items()):
        if _u:
            print(f"    {va:#010x} {n}: {_u} unresolved slot(s)")
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

    # ---- annex: whole over-slot bodies in an appended executable section
    annex_bytes = 0
    if annex:
        import struct as _s
        pe = _s.unpack_from('<I', img, 0x3c)[0]
        nsec = _s.unpack_from('<H', img, pe + 6)[0]
        optsz = _s.unpack_from('<H', img, pe + 20)[0]
        opt = pe + 24
        salign = _s.unpack_from('<I', img, opt + 32)[0]
        falign = _s.unpack_from('<I', img, opt + 36)[0]
        hdrs = _s.unpack_from('<I', img, opt + 60)[0]
        shdr = opt + optsz + nsec * 40
        arva = _annex_rva(orig_path)
        if shdr + 40 > hdrs or any(img[shdr:shdr + 40]):
            print('\n  ANNEX FAILED: no room for a section header')
            unplaced = list(unplaced) + [(v, n, 'annex: no header room')
                                         for _a, v, n, _b in annex]
        else:
            total = max(av + len(b) for av, _v, _n, b in annex) \
                - (base + arva)
            raw = (len(img) + falign - 1) // falign * falign
            rawsz = (total + falign - 1) // falign * falign
            img += b'\0' * (raw + rawsz - len(img))
            for av, _v, _n, b in annex:
                o = raw + (av - base - arva)
                img[o:o + len(b)] = b
                annex_bytes += len(b)
            _s.pack_into('<8sIIIIIIHHI', img, shdr, b'.t3x', total, arva,
                         rawsz, raw, 0, 0, 0, 0, 0x60000020)
            _s.pack_into('<H', img, pe + 6, nsec + 1)
            newsz = (arva + total + salign - 1) // salign * salign
            _s.pack_into('<I', img, opt + 56, newsz)
            os.makedirs(os.path.dirname(ANNEX_MANIFEST), exist_ok=True)
            with open(ANNEX_MANIFEST, 'w') as f:
                f.write('va,annex_va,length,name\n')
                for av, v, n, b in sorted(annex, key=lambda e: e[1]):
                    f.write('0x%08X,0x%08X,%d,%s\n' % (v, av, len(b), n))
            print(f"\nannex (.t3x, over-slot bodies)   : {len(annex)} "
                  f"functions, {annex_bytes:,} bytes at RVA {arva:#x}")
            for av, v, n, b in sorted(annex, key=lambda e: e[1]):
                print(f"    {v:#x} {n}: {len(b)}B body at {av:#x}")
    elif os.path.exists(ANNEX_MANIFEST):
        os.remove(ANNEX_MANIFEST)

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

    # ---- ABI screens over the PLACED bytes (tools/t3abi.py) -------------
    # A convention error at a T3 call site survives every byte metric and
    # every per-function oracle run whose seeds miss the arm (0x100597C0 and
    # BrCarStep->0x1002F640, 2026-09-21): screen the final image against the
    # reference's own behaviour -- argument-register liveness, double pops,
    # and the body's own ret-K -- and fail the gate on any flag.
    from t3abi import abi_screen
    refb = open(orig_path, 'rb').read()
    ref_text = refb[traw:traw + tvsize]
    img_text = bytes(img[traw:traw + tvsize])
    t3_spans = [(va, len(usable[va][1]), usable[va][0])
                for va in sorted(usable) if va in t3_vas]
    annex_by_va = {v: (av, bytes(b)) for av, v, _n, b in annex} if annex else {}
    abi_flags = abi_screen(ref_text, base + trva, img_text, base + trva,
                           t3_spans, annex_by_va)
    print(f"\nABI screens on placed T3 bodies      : {len(abi_flags)} flag(s)"
          f"  (register-arg liveness / double pop / callee ret-K)")
    for fl in abi_flags:
        print(f"    {fl}")

    # A wrong claim is a decomp defect; a tree that will not compile says nothing
    # about the claims.  Both exit non-zero, but they call for opposite actions,
    # so name which.  T3 residue is in NEITHER bucket.
    print(f"\nStale reloc_overrides rows (no relocation there): {len(STALE_ROWS)}")
    for sva, sname, soff, sval in STALE_ROWS[:20]:
        print(f"    {sva:#010x} {sname} off {soff:#x} -> {sval:#010x}")
    if STALE_ROWS:
        print("    regenerate: .venv/bin/python tools/lockstep_rows.py <VA> --write "
              "(after removing that function's lockstep rows)")
    claims_bad = bool(regressions or overlaps or conflicting or outside
                      or unplaced or blocked or abi_flags or STALE_ROWS)
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


def _force_annex(annex):
    """Spill the byte-locked functions named in config/force_annex.csv into the
    annex with their (grown) transcribed body, and leave a `jmp annex` thunk at
    the original VA.  This is how a diagnostic edit to a MATCHED (T4) function --
    which the in-slot lane would revert for overflowing its slot -- still ships:
    the fatal-exit logger (BrLogFatalPrintf) lives here.  Returns (thunk
    overrides for the T4 map, extended annex list)."""
    import csv as _csv
    import struct as _st
    from t3b_env import image as _image
    from reloc_fill import load_maps, parse as rf_parse, func_symbol_matches
    from reloc_pair import _our_sites
    from relocmap import REL_DIR32, REL_REL32

    p = os.path.join(ROOT, 'config', 'force_annex.csv')
    if not os.path.exists(p):
        return {}, annex
    rows = [r for r in _csv.DictReader(open(p)) if r.get('va')]
    if not rows:
        return {}, annex

    fnmap, _gl = load_maps()
    _im = _image().imports
    imports = _im() if callable(_im) else _im
    annex = list(annex)
    cursor = max([av + len(b) for av, _v, _n, b in annex], default=None)
    if cursor is None:
        cursor = 0x10000000 + _annex_rva(ib.ORIG_DLL)

    # orig_size per VA from report.csv (thunk filler needs the slot length)
    sizes = {}
    for r in _csv.DictReader(open(REPORT)):
        if r.get('va') and r.get('orig_size'):
            try:
                sizes[r['va'].lower()] = int(r['orig_size'])
            except ValueError:
                pass

    overrides = {}
    for r in rows:
        va = int(r['va'], 16)
        obj, err, _how = ib._compile_dll_obj(r['file'], r['opt'], True, ())
        if obj is None:
            print('  FORCE-ANNEX %s: compile failed (%s)' % (r['name'], err))
            continue
        d, secs, syms, relocs = rf_parse(obj)
        fn = next((s for s in syms
                   if func_symbol_matches(s['name'], r['symbol'])
                   and secs.get(s['sec'], {}).get('name', '').startswith('.text')),
                  None)
        if fn is None:
            print('  FORCE-ANNEX %s: symbol not in obj' % r['name'])
            continue
        sec = secs[fn['sec']]
        nx = [s['val'] for s in syms if s['sec'] == fn['sec']
              and s['val'] > fn['val'] and '$' not in s['name']]
        end = min(nx) if nx else sec['size']
        full = end - fn['val']
        body = bytearray(d[sec['praw'] + fn['val']:sec['praw'] + end])
        a_va = (cursor + 15) & ~15
        st, _b = _our_sites(obj, r['symbol'], full)
        blocked = []
        for off, sym, rt, addend, _k in (st or []):
            if sym.startswith('__imp__'):
                slot = imports.get(sym) or imports.get(sym.split('@', 1)[0])
                if slot is None:
                    blocked.append(sym); continue
                val = (slot + addend) & 0xFFFFFFFF if rt == REL_DIR32 else \
                      (slot + addend - (a_va + off + 4)) & 0xFFFFFFFF
            else:
                base = sym.lstrip('_@').split('@')[0]
                # BrOperatorNew is the original's static-CRT operator new; it
                # is not a claimed glide function so the maps don't carry it.
                _known = {'BrOperatorNew': 0x1007DFE0}
                tgt = fnmap.get(base) or fnmap.get(sym) or _known.get(base)
                if tgt is None:
                    blocked.append(sym); continue
                val = (tgt + addend - (a_va + off + 4)) & 0xFFFFFFFF \
                      if rt == REL_REL32 else (tgt + addend) & 0xFFFFFFFF
            _st.pack_into('<I', body, off, val & 0xFFFFFFFF)
        if blocked:
            print('  FORCE-ANNEX %s BLOCKED: unresolved %s'
                  % (r['name'], ', '.join(blocked[:4])))
            continue
        size = sizes.get(r['va'].lower(), 5)
        ob2 = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        orig2 = open(ob2, 'rb').read()
        rel = (a_va - (va + 5)) & 0xFFFFFFFF
        thunk = b'\xe9' + _st.pack('<I', rel) + orig2[5:size]
        overrides[va] = (r['name'], thunk, 0, 0, 'T4')
        annex.append((a_va, va, r['name'], bytes(body)))
        cursor = a_va + len(body)
        print('  FORCE-ANNEXED %s: %dB body at 0x%08X, thunk at 0x%08X'
              % (r['name'], full, a_va, va))
    return overrides, annex


def main():
    argv = sys.argv[1:]

    def opt(flag, default=None):
        return argv[argv.index(flag) + 1] if flag in argv else default

    out_dir = opt('--out-dir', ib.OUT_DIR)
    no_write = '--no-write' in argv
    recompile = '--recompile' in argv
    t3_only = '--t3-only' in argv
    jobs = int(opt('--jobs')) if '--jobs' in argv else None

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
            recompile, dprog, jobs=jobs)
        sys.stderr.write('\r' + ' ' * 70 + '\r')

    def tprog(n, total, f):
        sys.stderr.write('\r  T3 functions %d/%d %-40s'
                         % (n, total, os.path.basename(f)))
        sys.stderr.flush()
    t3_best, t3_unplaced, t3_unbuildable, annex = collect_t3(
        recompile, tprog, jobs=jobs)
    sys.stderr.write('\r' + ' ' * 70 + '\r')

    for va, (name, *_rest) in t3_best.items():
        names_at.setdefault(va, set()).add(name)

    # Force-annex diagnostic/over-slot MATCHED functions (fatal-exit logger).
    fa_overrides, annex = _force_annex(annex)
    for va, entry in fa_overrides.items():
        t4_best[va] = entry
        names_at.setdefault(va, set()).add(entry[0])

    img, verdict = assemble_contract(
        ib.ORIG_DLL, t4_best, t3_best, names_at,
        t4_unplaced + t3_unplaced, t4_unbuildable + t3_unbuildable,
        annex=annex)

    dest = os.path.join(out_dir, 'BRGlide.GOLDEN.dll'
                        if os.environ.get('BR_GOLDEN') == '1'
                        else 'BRGlide.T3.dll')
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
