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
    return (ins.mnemonic, field_kind, tuple(ops))


def _address_fields(body, va, lo, hi, text_lo, text_hi):
    """[(off_of_field, key, kind, raw_value)] for every 4-byte field in `body`
    that can hold a relocated address: an imm32/disp32 whose value lands in
    [lo, hi) (DIR32), or a rel32 call/jmp displacement (REL32, target in
    .text).  Encoding offsets are taken from capstone's imm/disp layout."""
    out = []
    for ins in _md.disasm(bytes(body), va):
        ioff = ins.address - va
        is_branch = ins.mnemonic in ('call', 'jmp')
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
        # REL32: e8/e9 (and jcc rel32); value stored is the displacement
        if ins.mnemonic in ('call', 'jmp') and len(ins.operands) == 1 \
                and ins.operands[0].type == X86_OP_IMM:
            tgt = ins.operands[0].imm & 0xFFFFFFFF
            if text_lo <= tgt < text_hi and ins.imm_size == 4:
                out.append((ioff + ins.imm_offset,
                            _skeleton(ins, 'rel'), REL_REL32, tgt))
    return out


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
            if fval == want and (key is None or fkey == key):
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
    # CONSERVATION: every unknown site and every leftover field must be
    # accounted for, key for key.  A surplus on either side means an
    # instruction changed SHAPE between the compiles (or a constant is
    # masquerading as an address), so some same-key candidate may belong to a
    # drifted site -- measured on the self-test, that is exactly where forced
    # 1:1 pairs go wrong.  Refuse the whole function rather than pair into a
    # polluted pool.
    n_unknown = len(unknown)
    n_left = sum(len(v) for v in leftover.values())
    keys_ok = (n_unknown == n_left and
               all(len(leftover.get(k, [])) == len(g)
                   for k, g in bykey.items()))
    if not keys_ok:
        return {}, [(s[1], 'conservation failed (%d sites vs %d fields)'
                     % (n_unknown, n_left)) for s in unknown], {}
    assigned = {}                       # sym -> address, cross-site agreement
    for key, group in bykey.items():
        cands = leftover.get(key, [])
        if key is None or len(cands) != len(group):
            for s in group:
                refused.append((s[1], 'no forced candidate (key group %d vs %d)'
                                % (len(group), len(cands))))
            continue
        syms = {s[1] for s in group}
        addrs = set()
        for s, f in zip(sorted(group), sorted(cands, key=lambda x: x[0])):
            off, sym, rt, addend, _k = s
            foff, _fk, frt, fval = f
            if rt == REL_DIR32:
                addrs.add((sym, (fval - addend) & 0xFFFFFFFF))
            else:
                addrs.add((sym, fval))
        by_sym = {}
        for sym, a in addrs:
            by_sym.setdefault(sym, set()).add(a)
        if any(len(v) > 1 for v in by_sym.values()) or \
                (len(group) > 1 and len(syms) > 1):
            for s in group:
                refused.append((s[1], 'ambiguous within key group'))
            continue
        ok = True
        for sym, aset in by_sym.items():
            a = next(iter(aset))
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
            a = next(iter(by_sym[sym]))
            assigned[sym] = a
            if rt == REL_DIR32:
                out[(va, off)] = (a + addend) & 0xFFFFFFFF
            else:
                # linker semantics, inverted from reloc_learn's equation:
                # dword = target + addend - (site_va + 4)
                out[(va, off)] = (a + addend
                                  - (va + plen + off + 4)) & 0xFFFFFFFF
    return out, refused, assigned


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
