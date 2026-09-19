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
    for r in csv.DictReader(open(os.path.join(ROOT, 'build/match/report.csv'))):
        if r.get('va') and r['va'].lower() == '0x%08x' % va:
            row = r
    if row is None:
        return None, 'no report row'
    size = int(row['orig_size'])
    pre = ib.PREAMBLES.get('0x%08x' % va, b'')
    orig = open(os.path.join(ROOT, 'build/match/orig/0x%08X.bin' % va),
                'rb').read()[len(pre):]
    obj, err, _ = ib._compile_dll_obj(row['file'], row['opt'], False, {})
    if obj is None:
        return None, 'compile: %s' % err
    sites, body = rp._our_sites(obj, row['name'], size)
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

    afn, agl = augment_maps(obj, row['name'], size)

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
        for op in ins.operands:
            if op.type == rp.X86_OP_IMM and ins.imm_size == 4:
                vals.append((ins.imm_offset, op.imm & 0xFFFFFFFF))
            elif op.type == rp.X86_OP_MEM and ins.disp_size == 4:
                vals.append((ins.disp_offset, op.mem.disp & 0xFFFFFFFF))
        # match by the slot's position INSIDE the instruction, so an insn
        # with both a disp32 and an imm32 pairs each slot to its own field
        for foff, fval in vals:
            if foff == off_in_ins:
                return fval
        return vals[0][1] if len(vals) == 1 else None

    rows_out, disagreements = [], []
    skipped = 0
    for off, sym, rt, addend, _key in sites:
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
            return None, 'slot %#x: no counterpart operand (%s)' % (off, sym)
        if rt == REL_REL32:
            slot = (val - (va + len(pre) + off + 4)) & 0xFFFFFFFF
            implied = val
        else:
            slot = val & 0xFFFFFFFF
            implied = (val - addend) & 0xFFFFFFFF
        k = known(sym)
        if k is not None:
            want = (k + addend) & 0xFFFFFFFF if rt == REL_DIR32 else k
            agree = (want == (val & 0xFFFFFFFF) or
                     (rt == REL_REL32 and rp._jmp_hop(img, val) == k))
            if not agree:
                disagreements.append((off, sym, k, implied))
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
