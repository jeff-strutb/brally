"""Emit a function's ROM bytes as IDO-assembler source, for the hand-written
assembly the game carries (the boot entry, the audio mixer loops).

    .venv/bin/python n64/tools/n64asm.py 0x8025649C BrMixMono
    .venv/bin/python n64/tools/n64asm.py 0x802562E0 BrFloatToInt 0x802562F0 BrMixStereo

Nothing here is project code: the output is a starting point that still has
to be read, named and described before it is filed.  It
uses register numbers (IDO's as has no names), `.set noreorder` so delay slots
stay where the ROM has them, `-mips3 -32` (a cflags line; IDO's as rejects
`.set mips3`) for the 64-bit accumulators, local
labels for branch targets, and %hi/%lo for every lui pair that reaches a
symbol -- so the grader still checks each relocation against the ROM.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'n64', 'tools'))
import n64build as B  # noqa: E402
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS64, CS_MODE_BIG_ENDIAN  # noqa: E402

REGS = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5',
        't6', 't7', 's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1',
        'gp', 'sp', 'fp', 'ra']
REGNUM = {n: i for i, n in enumerate(REGS)}
REGNUM['s8'] = 30


def symname(addr, syms):
    for n, v in syms.items():
        if v == addr:
            return n
    return 'D_%08X' % addr


def emit(va, name, rom, fmap, syms, lines):
    md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS64 | CS_MODE_BIG_ENDIAN)
    size = fmap[va]
    words = [(a, rom.word(a)) for a in range(va, va + size, 4)]
    while words and words[-1][1] == 0 and len(words) > 1 and words[-2][1] != 0x03E00008:
        words.pop()                      # alignment padding is not the function's
    targets = set()
    insns = {}
    for a, w in words:
        d = list(md.disasm(w.to_bytes(4, 'big'), a))
        insns[a] = d[0] if d else None
        op = w >> 26
        if op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
            targets.add((a + 4 + (B.sext16(w & 0xffff) << 2)) & 0xffffffff)
        elif op == 2:
            targets.add(((a + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2))
    # lui pairs that address a symbol
    hi = {}
    pair = {}
    for a, w in words:
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op == 0x0F:
            hi[rt] = (a, (w & 0xffff) << 16)
            continue
        if rs in hi and op in (0x09, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x31,
                               0x35, 0x37, 0x39, 0x3D, 0x3F):
            ha, base = hi[rs]
            t = (base + B.sext16(w & 0xffff)) & 0xffffffff
            if t >= 0x80000000 and (w & 0xffff or base) and ha not in pair:
                s = symname(t, syms)
                pair[ha] = '%%hi(%s)' % s
                pair[a] = '%%lo(%s)' % s
        # a register written with the full address (or anything else) is no
        # longer a %hi half
        if op in (0x09, 0x20, 0x21, 0x23, 0x24, 0x25, 0x37) and rt in hi:
            del hi[rt]
        if op == 0 and ((w >> 11) & 31) in hi:
            del hi[(w >> 11) & 31]
    lines.append('')
    lines.append('.globl %s' % name)
    lines.append('.ent %s' % name)
    lines.append('%s:' % name)
    for a, w in words:
        if a in targets and a != va:
            lines.append('.L%08X:' % a)
        i = insns[a]
        if i is None:
            lines.append('    .word   0x%08X' % w)
            continue
        m, ops = i.mnemonic, i.op_str
        ops = re.sub(r'\$(\w+)', lambda x: '$%d' % REGNUM[x.group(1)] if x.group(1) in REGNUM
                     else x.group(0), ops)
        if a in pair:
            if m == 'lui':
                ops = re.sub(r', .*$', ', ' + pair[a], ops)
            elif m == 'addiu':
                ops = re.sub(r', [^,]*$', ', ' + pair[a], ops)
            else:
                ops = re.sub(r'(-?0x[0-9a-f]+|-?\d+)?\((\$\d+)\)$', pair[a] + r'(\2)', ops)
        op = w >> 26
        if op in (1, 4, 5, 6, 7, 20, 21, 22, 23) or op == 2:
            tgt = ((a + 4 + (B.sext16(w & 0xffff) << 2)) & 0xffffffff) if op != 2 else \
                (((a + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2))
            ops = re.sub(r'0x[0-9a-f]+$', '.L%08X' % tgt, ops)
        ops = re.sub(r'\((\$\d+)\)', r'(\1)', ops)
        ops = re.sub(r'(^|, )\((\$\d+)\)', r'\g<1>0(\2)', ops)
        lines.append('    %-7s %s' % (m, ops) if ops else '    %s' % m)
    lines.append('.end %s' % name)


def main():
    args = sys.argv[1:]
    rom, fmap, syms = B.Rom(), B.function_map(), B.load_symbols()
    lines = ['/* <file>.s -- <what the object is>\n */', '/* n64-cflags: -O2 -mips3 -32 */', '.set noreorder', '.set noat', '', '.text']
    for k in range(0, len(args), 2):
        emit(int(args[k], 16), args[k + 1], rom, fmap, syms, lines)
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
