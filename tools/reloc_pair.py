#!/usr/bin/env python3
"""Recover a T3 function's unresolvable relocation addresses by PAIRING its
recompiled relocation sites with the original body's own address fields.

WHY THIS EXISTS.  62 T3-certified functions reference hand-named statics and
globals (`g_brInMouse`, `g_s17$S695`, `Br72CtlNew`) that no map gives an
address for, so the contract-valid image build blocks them.  reloc_learn.py
cannot help: it inverts addresses only at a MASKED-MATCH function's slots,
where our reloc offsets are provably the original's -- exactly what a T3 body
(a certified reschedule) does not give.  But the original body still CONTAINS
every address; only the offsets moved.  This tool finds them by instruction
shape instead of by offset.

HOW.  Both bodies are disassembled.  Each side yields "sites": instructions
carrying a 4-byte address field (a DIR32 imm/disp whose value lands inside the
image, or a REL32 call).  A site's KEY is its register-blind instruction
skeleton -- mnemonic plus operand shape with registers wildcarded and the
address field masked -- because a T3 residue recolours registers and
reschedules, but does not change WHICH instructions touch an absolute address.
Sites whose symbol already resolves are consumed first against the original
field holding exactly their known value.  What remains is matched per key,
and an assignment is accepted only when it is FORCED:

  * a key group pairs only when both sides have the SAME number of sites, and
  * the group names ONE symbol and implies ONE address (value - our addend),
    or the group has exactly one site on each side.

Everything else is refused.  Refusal is safe: the function stays blocked, as
it is today.  Accepted addresses are validated against the image's sections
(REL32 targets must be .text), a symbol paired at several sites or in several
functions must imply the SAME address every time, and `--selftest` re-derives
every KNOWN slot of every placeable T3 function by the same rules and demands
100% agreement -- the lever is evidence-gated, never a guess.

Usage:
    .venv/bin/python tools/reloc_pair.py --selftest   # prove the pairing rules
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from capstone import Cs, CS_ARCH_X86, CS_MODE_32              # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM               # noqa: E402
import reloc_fill                                             # noqa: E402
from relocmap import REL_DIR32, REL_REL32                     # noqa: E402

_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = True


def _skeleton(ins, field_kind):
    """Register-blind instruction key.  `field_kind` says which operand kind
    holds the address field ('imm', 'mem', 'rel'), so a `mov [g], reg` never
    keys with a `mov reg, OFFSET g` even when mnemonics agree."""
    ops = []
    for op in ins.operands:
        if op.type == X86_OP_IMM:
            ops.append('imm')
        elif op.type == X86_OP_MEM:
            # base/index registers wildcarded by presence only: recolouring
            # changes WHICH register, not whether one is used.
            ops.append('mem%s%s' % ('b' if op.mem.base else '',
                                    'i' if op.mem.index else ''))
        else:
            ops.append('r%d' % op.size)
    # A transfer is a transfer: our compile spells `call f; ret` where the
    # original tail-calls `jmp f` -- same relocation, different mnemonic, and
    # keying them apart breaks conservation on exactly those functions.
    mnem = 'xfer' if field_kind == 'rel' else ins.mnemonic
    return (mnem, field_kind, tuple(ops))


def _address_fields(body, va, lo, hi, text_lo, text_hi):
    """[(off_of_field, key, kind, raw_value)] for every 4-byte field in `body`
    that can hold a relocated address: an imm32/disp32 whose value lands in
    [lo, hi) (DIR32), or a rel32 call/jmp displacement (REL32, target in
    .text).  Encoding offsets are taken from capstone's imm/disp layout."""
    out = []
    for ins in _md.disasm(bytes(body), va):
        ioff = ins.address - va
        # ANY control-flow immediate is a code displacement, not a relocation:
        # a long jcc's target lands in .text and would pollute the DIR32 pool
        # (measured: je/jne/jl fields broke conservation on BrRcaFixup).
        is_branch = ins.mnemonic == 'call' or ins.mnemonic.startswith('j')
        # DIR32: immediate operands and memory displacements
        for op in ins.operands:
            if op.type == X86_OP_IMM and ins.imm_size == 4 and not is_branch:
                val = op.imm & 0xFFFFFFFF
                if lo <= val < hi:
                    out.append((ioff + ins.imm_offset,
                                _skeleton(ins, 'imm'), REL_DIR32, val))
            elif op.type == X86_OP_MEM and op.mem.disp:
                val = op.mem.disp & 0xFFFFFFFF
                if lo <= val < hi and ins.disp_size == 4:
                    out.append((ioff + ins.disp_offset,
                                _skeleton(ins, 'mem'), REL_DIR32, val))
        # REL32: e8/e9; value stored is the displacement.  A jmp landing back
        # inside this body is control flow, not a tail-call relocation.
        if ins.mnemonic in ('call', 'jmp') and len(ins.operands) == 1 \
                and ins.operands[0].type == X86_OP_IMM:
            tgt = ins.operands[0].imm & 0xFFFFFFFF
            inside = va <= tgt < va + len(body)
            if text_lo <= tgt < text_hi and ins.imm_size == 4 \
                    and (ins.mnemonic == 'call' or not inside):
                out.append((ioff + ins.imm_offset,
                            _skeleton(ins, 'rel'), REL_REL32, tgt))
    return out


def _jmp_hop(img, a):
    """The address `a`'s one-hop jmp-thunk target in the original image, or
    None.  `e9 rel32` only: the link-stage thunks that alias a function's
    entry (the original calls the thunk, the maps name the implementation)."""
    try:
        if img.byte(a) != 0xE9:
            return None
        d = 0
        for i in range(4):
            d |= img.byte(a + 1 + i) << (8 * i)
        if d >= 0x80000000:
            d -= 0x100000000
        return (a + 5 + d) & 0xFFFFFFFF
    except Exception:
        return None


def _our_sites(obj_path, fname, size):
    """[(off, sym, rt, addend, key)] for the function's relocation slots, keyed
    by the recompiled instruction that carries each slot."""
    d, secs, syms, relocs = reloc_fill.parse(obj_path)
    fn = next((s for s in syms
               if reloc_fill.func_symbol_matches(s['name'], fname)
               and secs.get(s['sec'], {}).get('name', '').startswith('.text')),
              None)
    if fn is None:
        return None, None
    sec = secs[fn['sec']]
    body = d[sec['praw'] + fn['val']:sec['praw'] + fn['val'] + size]
    byidx = {s['idx']: s for s in syms}
    # index instructions by covered byte range
    spans = [(ins.address, ins.address + ins.size, ins)
             for ins in _md.disasm(bytes(body), 0)]

    def covering(off):
        for a, b, ins in spans:
            if a <= off < b:
                return ins
        return None

    sites = []
    for rva, si, rt in relocs.get(fn['sec'], []):
        off = rva - fn['val']
        if not (0 <= off <= size - 4):
            continue
        t = byidx.get(si)
        if t is None:
            continue
        addend = struct.unpack_from('<i', body, off)[0]
        ins = covering(off)
        if ins is None:
            sites.append((off, t['name'], rt, addend, None))
            continue
        if rt == REL_REL32:
            kind = 'rel'
        else:
            kind = 'imm'
            for op in ins.operands:
                if op.type == X86_OP_MEM and ins.disp_size == 4 \
                        and ins.address + ins.disp_offset == off:
                    kind = 'mem'
        sites.append((off, t['name'], rt, addend, _skeleton(ins, kind)))
    return sites, body


def _solve_group(group, cands):
    """{sym: addr} when EXACTLY ONE assignment of bases to the group's symbols
    reproduces the candidate fields, else None.

    A key group may hold several symbols (two statics accessed with the same
    instruction shape).  Their member-offset addends are a fingerprint: symbol
    S with addends {0,4,8} and symbol T with {0} match fields {X,X+4,X+8,Y}
    only as S=X, T=Y.  Every symbol's base candidates are enumerated from the
    fields through its own addends; a combination is a solution when the
    multiset of implied field values equals the multiset of candidate values.
    Zero solutions or two+ distinct solutions -> None (refused).  REL32 sites
    ignore addends (the field already carries the absolute target)."""
    import itertools
    syms = sorted({s[1] for s in group})
    fvals = sorted(f[3] for f in cands)
    per_sym = []
    for sym in syms:
        ssites = [s for s in group if s[1] == sym]
        rt = ssites[0][2]
        bases = set()
        for f in cands:
            for s in ssites:
                b = f[3] if rt == REL_REL32 else (f[3] - s[3]) & 0xFFFFFFFF
                bases.add(b)
        per_sym.append((sym, rt, ssites, sorted(bases)))
    total = 1
    for _sym, _rt, _ss, bases in per_sym:
        total *= len(bases)
        if total > 20000:
            return None
    solutions = set()
    for combo in itertools.product(*(b for _s, _r, _ss, b in per_sym)):
        implied = []
        for (sym, rt, ssites, _b), base in zip(per_sym, combo):
            for s in ssites:
                implied.append(base if rt == REL_REL32
                               else (base + s[3]) & 0xFFFFFFFF)
        if sorted(implied) == fvals:
            solutions.add(combo)
            if len(solutions) > 1:
                return None
    if len(solutions) != 1:
        return None
    combo = next(iter(solutions))
    return {p[0]: b for p, b in zip(per_sym, combo)}


def pair_function(obj_path, fname, va, size, orig_body, img, resolve_fn,
                  plen=0):
    """({(va, off): value}, refused: [(sym, why)], assigned: {sym: addr})
    -- forced pairings only.

    `resolve_fn(sym)` returning an address marks a site KNOWN; known sites are
    consumed against the original field holding exactly known+addend (same
    key), which both validates the key discipline and removes those fields
    from the candidate pool.  The returned values are the full slot dwords
    (address + addend for DIR32; the site-relative displacement for REL32,
    computed against the body's post-preamble image address `va + plen`).
    """
    sites, _body = _our_sites(obj_path, fname, size)
    if sites is None:
        return {}, [('*', 'function symbol not in object')], {}
    lo = img.base
    hi = max(s[2] for s in img.sections)
    fields = _address_fields(orig_body, va + plen, lo, hi,
                             img.text_lo, img.text_hi)
    consumed = set()
    unknown = []
    known_addrs = set()
    for off, sym, rt, addend, key in sites:
        a = resolve_fn(sym)
        if a is None:
            unknown.append((off, sym, rt, addend, key))
            continue
        known_addrs.add(a & 0xFFFFFFFF)
        want = (a + addend) & 0xFFFFFFFF if rt == REL_DIR32 else a
        for i, (foff, fkey, frt, fval) in enumerate(fields):
            if i in consumed or frt != rt:
                continue
            # A REL32 field may call a jmp-thunk ALIAS of the known target
            # (the original prefers the thunk, the maps the implementation);
            # unconsumed alias fields broke conservation on BrRcaFixup.
            hit = (fval == want or
                   (rt == REL_REL32 and _jmp_hop(img, fval) == want))
            if hit and (key is None or fkey == key):
                consumed.add(i)
                break
    # group the unknowns and the leftover fields by key
    out, refused = {}, []
    bykey = {}
    for s in unknown:
        bykey.setdefault(s[4], []).append(s)
    leftover = {}
    for i, f in enumerate(fields):
        if i not in consumed:
            leftover.setdefault(f[1], []).append(f)
    # CONSERVATION, per key: every bucket that holds an unknown site must
    # hold EXACTLY as many candidate fields.  A surplus in such a bucket
    # means a drifted or masquerading field could be the forced "candidate"
    # -- measured on the self-test, that is exactly where 1:1 pairs go wrong
    # -- so the bucket is refused.  A stray field in a bucket with NO unknown
    # sites is a KNOWN anchor whose original instruction changed shape; it
    # cannot reach any unknown's pool, so it blocks nothing.
    keys_ok = all(len(leftover.get(k, [])) == len(g)
                  for k, g in bykey.items())
    if not keys_ok:
        return {}, [(s[1], 'conservation failed (key %s: %d sites vs %d '
                     'fields)' % (s[4], len(bykey[s[4]]),
                                  len(leftover.get(s[4], []))))
                    for s in unknown], {}
    assigned = {}                       # sym -> address, cross-site agreement
    for key, group in bykey.items():
        cands = leftover.get(key, [])
        if key is None or len(cands) != len(group):
            for s in group:
                refused.append((s[1], 'no forced candidate (key group %d vs %d)'
                                % (len(group), len(cands))))
            continue
        by_sym = _solve_group(group, cands)
        if by_sym is None:
            for s in group:
                refused.append((s[1], 'ambiguous within key group'))
            continue
        ok = True
        for sym, a in by_sym.items():
            rt = next(s[2] for s in group if s[1] == sym)
            if rt == REL_REL32 and not (img.text_lo <= a < img.text_hi):
                ok = False
            if rt == REL_DIR32 and not (img.mapped(a) or img.is_bss(a)):
                ok = False
            if sym in assigned and assigned[sym] != a:
                ok = False
            # DECOY GUARD.  Every self-test miss paired an unknown onto an
            # address a KNOWN symbol of this function already owns -- a
            # same-key field of the known symbol left unconsumed by a shape
            # drift.  An unknown symbol recovering to a known symbol's own
            # address is that decoy, not a discovery.
            if a in known_addrs:
                ok = False
        if not ok:
            for s in group:
                refused.append((s[1], 'validation failed'))
            continue
        for s in group:
            off, sym, rt, addend, _k = s
            a = by_sym[sym]
            assigned[sym] = a
            if rt == REL_DIR32:
                out[(va, off)] = (a + addend) & 0xFFFFFFFF
            else:
                # linker semantics, inverted from reloc_learn's equation:
                # dword = target + addend - (site_va + 4)
                out[(va, off)] = (a + addend
                                  - (va + plen + off + 4)) & 0xFFFFFFFF
    return out, refused, assigned


def audit_function(obj_path, fname, va, size, orig_body, img, resolve_fn,
                   plen=0):
    """({(va, off): corrected_value}, [(sym, paired_addr, map_addr)]).

    Hold out each RESOLVABLE symbol in turn and re-derive it by pairing
    alone.  Where the pairing forces a value that DISAGREES with the map's,
    the original image's own dword wins: hand-coined names (BrSubXXXXXXXX
    suffixes, g_<HEX> rows) have proven to carry stale D3D-space addresses,
    and a placed function must reference what the original references, not
    what a name remembers.  Every override is returned for reporting; a
    symbol the pairing cannot force is left on its map value unchanged.
    """
    sites, _b = _our_sites(obj_path, fname, size)
    if sites is None:
        return {}, []
    corrections, reports = {}, []
    resolvable = sorted({s[1] for s in sites
                         if resolve_fn(s[1]) is not None})
    for hold in resolvable:
        out, _r, asg = pair_function(
            obj_path, fname, va, size, orig_body, img,
            lambda s, hold=hold: None if s == hold else resolve_fn(s),
            plen=plen)
        t = resolve_fn(hold)
        for off, sym, rt, addend, _k in sites:
            if sym != hold or (va, off) not in out:
                continue
            if rt == REL_DIR32:
                want = (t + addend) & 0xFFFFFFFF
            else:
                want = (t + addend - (va + plen + off + 4)) & 0xFFFFFFFF
            if out[(va, off)] != want:
                corrections[(va, off)] = out[(va, off)]
                reports.append((sym, asg.get(sym), t))
    return corrections, sorted(set(reports))


def _selftest():
    """Leave-one-out over every certified T3 row: hold out each resolvable
    symbol, re-derive it by pairing alone, compare.  A disagreement where the
    pairing reproduces the original image's own dword and the map does not is
    reported as a STALE MAP row, not a failure (measured 2026-09-18: 329
    recoveries, 324 map-agreeing, 5 stale D3D-space rows exposed)."""
    import csv
    import image_build as ib
    from reloc_fill import resolve
    from t3 import certified
    from t3b_env import image, augment_maps, address_in_name

    img = image()
    cert = {v for v, i in certified().items()
            if not v.startswith('?') and i['ok']}
    rows = {}
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    for r in csv.DictReader(open(rep)):
        if r.get('status') == 'diff' and r.get('va') \
                and r['va'].lower() in cert:
            rows[int(r['va'], 16)] = r
    amb = ib._ambiguous_basenames(rows)
    tested = agree = 0
    stale = []
    for va, r in sorted(rows.items()):
        obj, _e, _h = ib._compile_dll_obj(r['file'], r['opt'], False, amb)
        if obj is None:
            continue
        size = int(r['orig_size'])
        ob = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        if not os.path.exists(ob):
            continue
        pre = ib.PREAMBLES.get('0x%08x' % va, b'')
        orig = open(ob, 'rb').read()[len(pre):]
        afn, agl = augment_maps(obj, r['name'], size)

        def full(sym):
            v = resolve(sym, afn, agl)
            return v if v is not None else address_in_name(sym)
        sites, _b = _our_sites(obj, r['name'], size)
        if sites is None:
            continue
        truths = {s[1]: full(s[1]) for s in sites if full(s[1]) is not None}
        for hold, t in sorted(truths.items()):
            out, _ref, _asg = pair_function(
                obj, r['name'], va, size, orig, img,
                lambda s, hold=hold: None if s == hold else full(s),
                plen=len(pre))
            for off, sym, rt, addend, _k in sites:
                if sym != hold or (va, off) not in out:
                    continue
                if rt == REL_DIR32:
                    want = (t + addend) & 0xFFFFFFFF
                else:
                    want = (t + addend - (va + len(pre) + off + 4)) & 0xFFFFFFFF
                tested += 1
                if out[(va, off)] == want:
                    agree += 1
                else:
                    stale.append((hex(va), r['name'], sym,
                                  hex(out[(va, off)]), hex(want)))
    print('leave-one-out: %d recovered, %d agree with the maps, %d differ'
          % (tested, agree, len(stale)))
    for s in stale:
        print('  MAP DIFFERS (pairing follows the original image): %s %s %s '
              'paired=%s map=%s' % s)
    return 0 if tested and agree + len(stale) == tested else 1


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        sys.exit(_selftest())
    print(__doc__)


def jump_table_slots(obj_path, fname, va, size, plen=0):
    """{(va, off): value} for DIR32 slots naming a SAME-SECTION `$L` label.

    Those are switch jump-table entries: the case target's address is this
    function's own placement address plus the label's section offset -- known
    EXACTLY from the object's symbol table, no maps and no pairing involved.
    (A `$L` in another section is the C++ EH funclet; that one is
    oracle-only and never resolved here.)
    """
    out = {}
    try:
        d, secs, syms, relocs = reloc_fill.parse(obj_path)
    except Exception:
        return out
    fn = next((s for s in syms
               if reloc_fill.func_symbol_matches(s['name'], fname)
               and secs.get(s['sec'], {}).get('name', '').startswith('.text')),
              None)
    if fn is None:
        return out
    sec = secs[fn['sec']]
    byidx = {s['idx']: s for s in syms}
    for rva, si, rt in relocs.get(fn['sec'], []):
        off = rva - fn['val']
        if rt != REL_DIR32 or not (0 <= off <= size - 4):
            continue
        t = byidx.get(si)
        if not t or not t['name'].lstrip('_').startswith('$L') \
                or t['sec'] != fn['sec']:
            continue
        addend = struct.unpack_from('<i', d, sec['praw'] + rva)[0]
        out[(va, off)] = (va + plen + (t['val'] - fn['val'])
                         + addend) & 0xFFFFFFFF
    return out
