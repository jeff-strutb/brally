#!/usr/bin/env python3
"""Screen every tagged-diff function for a FRAME-SIZE mismatch.

    .venv/bin/python3 tools/framescreen.py [--all]

Why this is worth a whole tool: on 0x1000A110 a 4-byte frame gap
(`sub esp,0x48` against the original's `0x4c`) was carried for eight sessions
and was written off as "allocation, not code".  It was neither -- it was a
STORAGE CLASS.  VC5 never enregisters an array and never packs one into a
reused argument slot, so an array always spends a slot in the locals area;
two `uint8_t` scalars spend nothing, because they land in the dead argument
slots.  Declaring the pair as `uint8_t pack[2]` closed the gap and made the
prologue byte-exact.

So a frame that is a few bytes SMALLER than the original's, in a function
whose instruction count is otherwise close, is a specific and cheap-to-fix
defect, not a wall:

    recomp frame < orig frame   ->  a scalar (or scalars) that the source
                                    should declare as an ARRAY, or a local
                                    the original keeps live and we do not
    recomp frame > orig frame   ->  a local the original does not have, or
                                    one we force to memory that it keeps in
                                    a register

Nothing downstream of the frame can line up until it matches -- every stack
displacement in the function moves with it -- so these rows are worth taking
before any region grinding.

Reads the frame immediate straight out of the bytes: MSVC5 opens with either
`sub esp, imm` or `push ebp; mov ebp,esp; and esp,-8; sub esp, imm`, so the
first `sub esp` in the first few instructions is the frame.  Functions with
no such prologue (leaf functions with no locals) are skipped silently.

Default output is only the MISMATCHED rows, worst gap first.  `--all` also
lists the rows that agree.
"""
import sys, os, csv
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True


def frame_of(code):
    """The `sub esp, imm` immediate in the first few instructions, or None."""
    for n, i in enumerate(md.disasm(code, 0)):
        if n > 6:
            break
        if i.mnemonic == 'sub' and i.op_str.startswith('esp, 0x'):
            return int(i.op_str.split(', ')[1], 16)
        if i.mnemonic == 'sub' and i.op_str.startswith('esp, '):
            try:
                return int(i.op_str.split(', ')[1])
            except ValueError:
                return None
    return None


def main():
    show_all = '--all' in sys.argv
    rows = []
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        for r in csv.DictReader(f):
            if r['status'] != 'diff':
                continue
            ob = os.path.join(ROOT, 'build', 'match', 'orig', r['va'] + '.bin')
            if not os.path.exists(ob):
                continue
            of = frame_of(open(ob, 'rb').read())
            if of is None:
                continue
            # ‼ Use the row's OWN flag variant.  fn.py's obj_fnbase and the
            # sweep's obj_O2 are /O2 only; scoring an /Od or /O2 /Oy- row
            # against them compares two different compiles and invents a
            # frame gap.  (Caught on 0x1002ECEB, an O2y row, which read as a
            # 76-byte gap against the wrong object.)
            rf = None
            OPTDIR = {'O2': 'O2', 'Od': 'Od', 'O2y': 'O2y', 'O2p': 'O2p'}
            tag = OPTDIR.get(r.get('opt', 'O2'), 'O2')
            for tag in (['fnbase', tag] if tag == 'O2' else [tag]):
                p = os.path.join(ROOT, 'build', 'match', 'obj_' + tag,
                                 os.path.splitext(os.path.basename(r['file']))[0] + '.obj')
                if not os.path.exists(p):
                    continue
                try:
                    t = parse_coff_obj(p)
                except Exception:
                    continue
                if r['name'] in t:
                    rf = frame_of(t[r['name']][0])
                    break
            if rf is None:
                continue
            rows.append((of - rf, of, rf, r['va'], r['name'], r['file']))

    bad = [x for x in rows if x[0] != 0]
    bad.sort(key=lambda x: (-abs(x[0]), x[3]))
    print('%-12s %-34s %6s %6s %6s' % ('va', 'name', 'orig', 'ours', 'gap'))
    for gap, of, rf, va, name, fn in bad:
        note = 'ours SMALLER -- look for a scalar that should be an ARRAY' \
               if gap > 0 else 'ours LARGER -- a local the original does not spend'
        print('%-12s %-34s %6s %6s %+6d   %s' % (va, name[:34], hex(of), hex(rf), -gap, note))
    if show_all:
        for gap, of, rf, va, name, fn in rows:
            if gap == 0:
                print('%-12s %-34s %6s %6s %6s' % (va, name[:34], hex(of), hex(rf), 'ok'))
    print('\n%d of %d diff rows with a readable frame MISMATCH' % (len(bad), len(rows)))


if __name__ == '__main__':
    sys.exit(main() or 0)
