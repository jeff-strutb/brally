"""Region-count objective for the permuter on large EXE functions.

The raw byte-diff is useless once a size shift cascades (a 16-byte shift makes
a structurally-identical function score 600+). This counts the number of
divergent INSTRUCTION regions after aligning the two streams with relocs and
rel32 branch targets masked -- the real signal (WinMain: 15). Region 0 AND
byte-exact => true match.
"""
import re, difflib
import capstone
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_md.skipdata = True
_ABS = re.compile(r'0x4[0-9a-f]{5}\b')   # EXE globals 0x40xxxx / 0x41xxxx

def _dis(data, relset, is_recomp):
    out = []
    for i in _md.disasm(bytes(data), 0):
        t = i.mnemonic + ' ' + i.op_str
        # rel32/rel8 branch + direct call: mask target
        if i.mnemonic in ('call','jmp','je','jne','jl','jle','jg','jge','ja',
                           'jae','jb','jbe','js','jns','jo','jno','jp','jnp','jecxz','loop'):
            t = i.mnemonic + ' <T>'
        else:
            t = _ABS.sub('<G>', t)
            if is_recomp and relset is not None:
                if any((i.address+k) in relset for k in range(i.size)):
                    t = re.sub(r'\[0\]', '[<G>]', t)
                    t = re.sub(r'\[(e[a-z]{2}) \+ 0\]', r'[\1 + <G>]', t)
                    t = re.sub(r', 0$', ', <G>', t)
                    t = re.sub(r'^push 0$', 'push <G>', t)
        out.append((i.address, i.mnemonic, i.size, t))
    return out

def region_count(orig, recomp, relset):
    A = _dis(orig, None, False)
    B = _dis(recomp, relset, True)
    a = [x[3] for x in A]; b = [x[3] for x in B]
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    return sum(1 for tag,i1,i2,j1,j2 in sm.get_opcodes() if tag != 'equal')
