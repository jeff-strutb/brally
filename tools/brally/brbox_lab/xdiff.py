"""shared scorer: normalised instruction diff that ignores relocated absolute operands."""
import re, difflib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
MD = Cs(CS_ARCH_X86, CS_MODE_32)
def norm(ins):
    s = ins.op_str
    s = re.sub(r'\[(0x[0-9a-f]+|\d+)\]', '[A]', s)          # absolute memory operand
    s = re.sub(r'^(0x[0-9a-f]{5,}|0)$', 'I', s) if ins.mnemonic == 'push' else s
    if ins.mnemonic == 'call': s = 'T'
    s = re.sub(r'0x[0-9a-f]{5,}', 'A', s)
    return '%s %s' % (ins.mnemonic, s)
def seq(code):
    return [norm(i) for i in MD.disasm(code, 0)]
def score(orig, code):
    a, b = seq(orig), seq(code)
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    return sum(max(i2 - i1, j2 - j1) for op, i1, i2, j1, j2 in sm.get_opcodes() if op != 'equal') + abs(len(orig) - len(code))


def split_score(orig, code, cut=0x150):
    """(light, loop) diff counts, cut at an ORIGINAL offset."""
    a = [(i.address, norm(i)) for i in MD.disasm(orig, 0)]
    b = [(i.address, norm(i)) for i in MD.disasm(code, 0)]
    sm = difflib.SequenceMatcher(None, [x[1] for x in a], [x[1] for x in b], autojunk=False)
    lo = hi = 0
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == 'equal':
            continue
        at = a[i1][0] if i1 < len(a) else a[-1][0]
        n = max(i2 - i1, j2 - j1)
        if at < cut: lo += n
        else: hi += n
    return lo, hi
