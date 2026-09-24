#!/usr/bin/env python3
"""Emit reloc_overrides.csv rows for a blocked T3 function whose recompiled
instruction stream aligns with the original's in FULL LOCKSTEP.

The bucket pairing in reloc_pair.py forces assignments locally and refuses
same-shape ambiguity.  But when BOTH streams have the same instruction count
and every position pairs with the same mnemonic and register-blind operand
shape, the whole-stream alignment forces every site at once: site i's address
IS the original instruction i's operand.  That is a stronger condition than
any bucket -- one mismatched position and this tool refuses the function
entirely (fall back to the hand lane).

Verification is included in the condition itself, and every KNOWN slot doubles
as a check: a known site whose original operand disagrees with its resolution
is reported (stale row exposed) and the function still emits, because the
original is the authority.

Usage:
    .venv/bin/python tools/lockstep_rows.py <VA> [--write]

Prints the rows (and appends them to config/reloc_overrides.csv with --write).
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import image_build as ib                                  # noqa: E402
import reloc_pair as rp                                   # noqa: E402
from reloc_fill import resolve                            # noqa: E402
from relocmap import REL_DIR32, REL_REL32                 # noqa: E402
from t3b_env import image, augment_maps, address_in_name  # noqa: E402


def lockstep_rows(va):
    """(rows, disagreements) or (None, why)."""
    img = image()
    row = None
    name = None
    obj = None
    for r in csv.DictReader(open(os.path.join(ROOT, 'build/match/report.csv'))):
        if r.get('va') and r['va'].lower() == '0x%08x' % va:
            row = r
    if row is not None:
        # The same twin rule image_build_t3 places by: a report.csv row from
        # a DIFFERENT file than the certified transcription is a twin graded
        # at this VA; the certified body (the C++ lane) is what gets placed,
        # so rows must be derived from ITS object (BrExt_10054B50: rows cut
        # from the C twin's offsets landed a data address in a call).
        from t3 import certified
        cfile = certified().get('0x%08x' % va, {}).get('file')
        if cfile and cfile != row['file']:
            row = None
    if row is not None:
        name = row['name']
        opt = row['opt']
        vp = os.path.join(ROOT, 'config', 't3_variant.csv')
        if os.path.exists(vp):
            for vr in csv.DictReader(open(vp)):
                if vr.get('va') and int(vr['va'], 16) == va:
                    opt = vr['opt']       # rows must match the PLACED variant
        obj, err, _ = ib._compile_dll_obj(row['file'], opt, False, {})
        if obj is None:
            return None, 'compile: %s' % err
    else:
        # C++ lane: the row lives in report_cpp.csv, the object is the cpp
        # sweep's cached build, and the symbol is the raw mangled name.
        import cpp_score
        tags = [ib._opt_tag(o) for o in cpp_score.DEFAULT_OPTS]
        for r in csv.DictReader(open(os.path.join(
                ROOT, 'build/match/report_cpp.csv'))):
            if r.get('va') and r['va'].lower() == '0x%08x' % va:
                row = r
        if row is None:
            return None, 'no report row (either lane)'
        base = os.path.splitext(os.path.basename(row['file']))[0]
        try:
            ti = tags.index(row['opt'])
        except ValueError:
            return None, 'cpp opt %r unknown' % row.get('opt')
        obj = os.path.join(cpp_score.OBJ_DIR,
                           '%s_sweep_%08X_%d.obj' % (base, va, ti))
        if not os.path.exists(obj):
            return None, 'cpp sweep obj missing'
        _impl, symtag, kind = cpp_score.parse_implements_name(
            os.path.join(ROOT, row['file']), va)
        name = ib._raw_symbol(obj, symtag, kind)
        if name is None:
            return None, 'cpp raw symbol not found'
    size = int(row['orig_size'])
    pre = ib.PREAMBLES.get('0x%08x' % va, b'')
    orig = open(os.path.join(ROOT, 'build/match/orig/0x%08X.bin' % va),
                'rb').read()[len(pre):]
    sites, body = rp._our_sites(obj, name, size)
    if sites is None:
        return None, 'symbol not in object'

    ours = list(rp._md.disasm(bytes(body), 0))
    theirs = list(rp._md.disasm(bytes(orig), va + len(pre)))

    def shape(ins):
        return (ins.mnemonic, rp._skeleton(ins, 'x')[2])

    pairs = {}
    if len(ours) == len(theirs) and \
            all(shape(a) == shape(b) for a, b in zip(ours, theirs)):
        for a, b in zip(ours, theirs):
            pairs[a.address] = b           # full lockstep
    else:
        # BLOCK-WISE lockstep: diff the two shape sequences; inside an exact
        # matching block the positional argument holds block-locally, so a
        # site there is forced exactly as in the full-stream case.  A site
        # in a non-matching region is NOT emitted -- the function stays
        # blocked unless something else proves it.  Blocks shorter than 4
        # instructions are ignored: a tiny coincidental run is not an
        # anchor.
        from difflib import SequenceMatcher
        sa = [shape(i) for i in ours]
        sb = [shape(i) for i in theirs]
        sm = SequenceMatcher(None, sa, sb, autojunk=False)
        blocks = [b for b in sm.get_matching_blocks() if b.size >= 4]
        for bi, (a0, b0, n) in enumerate(blocks):
            # BOUNDARY GUARD.  A homogeneous run of same-shape instructions
            # can absorb a shifted alignment at a block edge: with two
            # `mov cl, [abs]` where the original has one, the matcher pairs
            # our FIRST with the original's and the values land one slot
            # early (measured on BrCarDrawVehicle's byte-scramble region).
            # An edge position is only trusted when its shape does NOT
            # appear in the adjacent unmatched gap on either stream.
            prev_a = blocks[bi - 1].a + blocks[bi - 1].size if bi else 0
            prev_b = blocks[bi - 1].b + blocks[bi - 1].size if bi else 0
            next_a = blocks[bi + 1].a if bi + 1 < len(blocks) else len(sa)
            next_b = blocks[bi + 1].b if bi + 1 < len(blocks) else len(sb)
            gap_lo = set(sa[prev_a:a0]) | set(sb[prev_b:b0])
            gap_hi = set(sa[a0 + n:next_a]) | set(sb[b0 + n:next_b])
            lo_skip = 0
            while lo_skip < n and sa[a0 + lo_skip] in gap_lo:
                lo_skip += 1
            hi_skip = 0
            while hi_skip < n - lo_skip and sa[a0 + n - 1 - hi_skip] in gap_hi:
                hi_skip += 1
            for j in range(lo_skip, n - hi_skip):
                pairs[ours[a0 + j].address] = theirs[b0 + j]

    afn, agl = augment_maps(obj, name, size)

    def known(sym):
        v = resolve(sym, afn, agl)
        return v if v is not None else address_in_name(sym)

    def field_value(ins, rt, our_ins, off_in_ins):
        """The original instruction's counterpart operand value."""
        vals = []
        if rt == REL_REL32:
            if ins.mnemonic in ('call', 'jmp') and ins.operands and \
                    ins.operands[0].type == rp.X86_OP_IMM:
                return ins.operands[0].imm & 0xFFFFFFFF
            return None
        # an absolute operand of ours pairs only with an ABSOLUTE operand of
        # the original: `mov [DAT_117aa0e0], 0` against `mov [ebx+0x558], 0`
        # has no address to copy -- 0x558 is an offset from ebx
        ours_abs = any(op.type == rp.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0
                       for op in our_ins.operands)
        for op in ins.operands:
            if op.type == rp.X86_OP_IMM and ins.imm_size == 4:
                vals.append(('imm', ins.imm_offset, op.imm & 0xFFFFFFFF))
            elif op.type == rp.X86_OP_MEM and ins.disp_size == 4:
                if ours_abs and (op.mem.base != 0 or op.mem.index != 0) and \
                        our_ins.disp_offset == off_in_ins:
                    continue
                vals.append(('disp', ins.disp_offset, op.mem.disp & 0xFFFFFFFF))
        # match by the slot's position INSIDE the instruction, so an insn
        # with both a disp32 and an imm32 pairs each slot to its own field
        for _k, foff, fval in vals:
            if foff == off_in_ins:
                return fval
        # otherwise only a field of the SAME kind: our `mov [DAT_x], 1`
        # against the original's `mov [ebp+2c], 1` has no address to copy --
        # the original's imm32 is the stored value, not DAT_x (FUN_1002f790
        # was placed writing to [reg+1] off exactly that pairing)
        kind = None
        if our_ins.disp_size == 4 and our_ins.disp_offset == off_in_ins:
            kind = 'disp'
        elif our_ins.imm_size == 4 and our_ins.imm_offset == off_in_ins:
            kind = 'imm'
        same = [fval for k, _foff, fval in vals if k == kind]
        if len(same) == 1:
            return same[0]
        # a different-kind field may still carry the same address (`mov eax,
        # offset X` vs `lea eax, [X]`), but only if it IS an address
        if len(vals) == 1:
            v = vals[0][2]
            if img.mapped(v) or img.is_bss(v):
                return v
        return None

    # Jump-table dispatch displacements and const slots that name this
    # function's own `$` labels are EXACT from the object symbol table
    # (image_build resolves them via jump_table_slots).  Their ORIGINAL
    # operand points at the ORIGINAL table, so a lockstep row here would
    # dispatch a byte-different T3 body mid-instruction (BrRaceStep's
    # in-game switches).  Never emit a row for them.
    jt_offsets = {o for (_v, o) in
                  rp.jump_table_slots(obj, name, va, size, plen=len(pre))}

    # WIDTH GUARD.  A memory operand pinned to the original's cell must read
    # as many bytes as the original reads there.  BrCrImpulseSolve's `< 0.0`
    # is a qword literal where the original compares a float: pinned to the
    # original's 0.0f at 0x10077A78, the qword took the next float (1.0f) as
    # its high half -- 0.0078125 -- and the contact test fired on resting
    # cars.  Different widths: no row; the machine resolves the literal by
    # content.  (The cell itself is the original's call: a transcription's
    # initializer that disagrees with the original's cell is a guess.)
    def mem_width(ins):
        for op in ins.operands:
            if op.type == rp.X86_OP_MEM:
                return op.size
        return None

    rows_out, disagreements = [], []
    skipped = 0
    if os.environ.get('LOCKSTEP_SITES'):
        for off, sym, rt, addend, _key in sites:
            print('# SITE %#x %s' % (off, sym))
    for off, sym, rt, addend, _key in sites:
        if off in jt_offsets:
            skipped += 1
            print('# JUMP-TABLE slot %#x %s (exact from symbol table -- '
                  'resolved by jump_table_slots, no row)' % (off, sym))
            continue
        # A self-encoding cpp name (?g_<HEX>, sub_<HEX>) carries its own
        # address, so image_build resolves the slot EXACTLY as
        # address_in_name(sym) + OUR object's addend.  A lockstep row copies
        # the ORIGINAL instruction's operand instead, which already folded in
        # the ORIGINAL's addend -- wrong whenever the two compilers picked a
        # different base offset (BrRaceStep's tyre loop: our object built
        # `mov eax, &g_AF2094 + 8` and read the pointer at [eax-8], but the
        # original used base+0, so the copied operand dropped the +8 and the
        # placed image dereferenced null in Quick Race).  Never emit a row
        # for an address-bearing symbol; the machine resolution is addend-
        # aware and correct.
        named = address_in_name(sym)
        our_ins = None
        for a in ours:
            if a.address <= off < a.address + a.size:
                our_ins = a
                break
        if our_ins is None:
            return None, 'slot %#x outside instructions' % off
        their = pairs.get(our_ins.address)
        if their is None:
            skipped += 1
            print('# UNANCHORED slot %#x %s (outside matching blocks)'
                  % (off, sym))
            continue
        val = field_value(their, rt, our_ins, off - our_ins.address)
        if val is None:
            skipped += 1
            print('# NO-COUNTERPART slot %#x %s (aligned insn carries no '
                  'matching operand)' % (off, sym))
            continue
        if rt == REL_REL32:
            slot = (val - (va + len(pre) + off + 4)) & 0xFFFFFFFF
            implied = val
        else:
            slot = val & 0xFFFFFFFF
            implied = (val - addend) & 0xFFFFFFFF
            # a DIR32 relocation always resolves to an address in the image;
            # a counterpart that is not one (0x558, an offset from the
            # original's base register) cannot be this slot's value
            if not any(img.mapped(x) or img.is_bss(x) for x in (val, implied)):
                skipped += 1
                print('# NOT-AN-ADDRESS slot %#x %s (original field %#x)' % (off, sym, val))
                continue
        if rt == REL_DIR32 and our_ins.disp_size == 4 and \
                our_ins.disp_offset == off - our_ins.address:
            wo, wt = mem_width(our_ins), mem_width(their)
            if wo and wt and wo != wt:
                skipped += 1
                print('# WIDTH-MISMATCH slot %#x %s: we read %d bytes, the original '
                      'reads %d at %#x' % (off, sym, wo, wt, implied))
                continue
        if named is not None:
            # An address-bearing name coined from THIS binary (?g_<HEX>,
            # sub_<HEX>) is exact, and image_build resolves it addend-aware
            # -- a copied operand would lose our own addend (BrRaceStep's
            # tyre loop).  But names carried over from the D3D build
            # (BrX10069530, FUN_100378c0) embed the OTHER binary's address:
            # there the original's operand is the only truth.  Tell them
            # apart by whether the name lands where the original points.
            near = (named == implied) if rt == REL_REL32 else \
                abs(((val - named) + 0x80000000) % (1 << 32) - 0x80000000) < 0x1000
            if near or (rt == REL_REL32 and rp._jmp_hop(img, val) == named):
                # pin it from the NAME (exact, with our own addend) rather
                # than leaving it to the image builder's map precedence
                if rt == REL_REL32:
                    val = named
                    slot = (named - (va + len(pre) + off + 4)) & 0xFFFFFFFF
                else:
                    val = (named + addend) & 0xFFFFFFFF
                    slot = val
                rows_out.append((off, slot, sym, val, addend))
                continue
        k = known(sym)
        if k is not None:
            want = (k + addend) & 0xFFFFFFFF if rt == REL_DIR32 else k
            agree = (want == (val & 0xFFFFFFFF) or
                     (rt == REL_REL32 and rp._jmp_hop(img, val) == k))
            if not agree:
                disagreements.append((off, sym, k, implied))
                if rt == REL_REL32 and img.text_lo <= k < img.text_hi and named is None:
                    # A call/jump whose callee resolves to a real function
                    # the source NAMES: the source calls that function (a
                    # wrapper where the original inlined its body, e.g.
                    # BrFtolTrunc 0x10018990 `fld [esp+4]; jmp _ftol` vs the
                    # original's bare `call _ftol`).  Retargeting it to the
                    # original's callee would skip the wrapper's argument
                    # handling -- BrExt_10052030 crashed under the live
                    # oracle on exactly such a row.  The name wins.
                    skipped += 1
                    print('# CALLEE KEPT slot %#x %s -> %#x (original calls %#x)'
                          % (off, sym, k, implied))
                    continue
            # A hearsay resolution that AGREES still gets a row: the
            # agreement is the evidence, and the placement gate does not
            # trust an unconfirmed name row on its own.
        rows_out.append((off, slot, sym, val, addend))
    return (rows_out, disagreements), None


def main():
    va = int(sys.argv[1], 16)
    res, why = lockstep_rows(va)
    if res is None:
        print('REFUSED: %s' % why)
        return 1
    rows_out, disagreements = res
    for off, sym, k, implied in disagreements:
        print('# STALE ROW EXPOSED off %#x %s: map %#x, original implies %#x'
              % (off, sym, k, implied))
    lines = []
    for off, slot, sym, val, addend in rows_out:
        lines.append('0x%08X,0x%X,0x%08X,lockstep %s (orig operand %#x%s)'
                     % (va, off, slot, sym.lstrip('_'), val,
                        ' addend %#x' % addend if addend else ''))
    print('\n'.join(lines) if lines else '# nothing to emit')
    if '--write' in sys.argv and lines:
        # never overwrite an existing row: a hand-derived row for the same
        # slot is stronger evidence than positional lockstep (BrFadeTick's
        # role-swapped slots), and _load_overrides is last-wins per key.
        csvp = os.path.join(ROOT, 'config', 'reloc_overrides.csv')
        have = set()
        for l in open(csvp):
            p = l.split(',')
            if len(p) > 2 and p[0].startswith('0x'):
                try:
                    have.add((int(p[0], 16), int(p[1], 0)))
                except ValueError:
                    pass
        fresh = [l for l in lines
                 if (int(l.split(',')[0], 16),
                     int(l.split(',')[1], 0)) not in have]
        skipped = len(lines) - len(fresh)
        with open(csvp, 'a') as f:
            if fresh:
                f.write('\n'.join(fresh) + '\n')
        print('# wrote %d rows%s' % (len(fresh),
              ', kept %d existing' % skipped if skipped else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
