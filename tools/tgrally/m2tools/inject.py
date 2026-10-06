"""inject.py VA draft.c [PROC:EMIT:REG ...] : compile the draft body with the hooked
ugen (build/tgrally/ext/instr2), which moves REG to the end of ugen's temp free list at
the first temp allocation at or after emit index EMIT of procedure PROC, and
report the positional diff count and first differing word.  Tests a temp-ring
hypothesis before hunting the source construct that causes it."""
import os, sys
os.environ['TGR_TRACE_CC'] = os.path.abspath('build/tgrally/ext/instr2/out/cc')
sys.path.insert(0, 'tools/tgrally')
import n64t3 as T, n64alloc as A, n64build as B  # noqa: E402
va = int(sys.argv[1], 16)
path, name, _ = T.source_of(va)
tree = open(path).read()
body = T.function_text(tree, name)
full = tree.replace(body, open(sys.argv[2]).read().strip('\n'))
tmp = os.path.join('build/tgrally/n64/m2tools/tw', 'inject_' + os.path.basename(path))
os.makedirs(os.path.dirname(tmp), exist_ok=True)
open(tmp, 'w').write(full)
rom, fmap, syms = B.Rom(), B.function_map(), B.load_symbols()
fnvas = {n: v for v, n, _ in B.tags_in(full)}
for spec in sys.argv[3:] or ['']:
    env = {'DKWB_INJECT': spec} if spec else {}
    obj, log = A.compile_traced(tmp, env)
    have = {p[0]: p for p in B.carve(obj)}
    _, s, e = have[name]
    st, nd, notes, ours, theirs = B.grade(obj, rom, name, s, e, va, fmap[va], syms, fnvas)
    skip = int(os.environ.get('SKIP', '0'))
    first = next((k for k, (a, b) in enumerate(zip(ours, theirs)) if a != b and k >= skip), None)
    inj = [l for l in log.splitlines() if 'DKWB-INJECT' in l]
    print('%-14s %s ndiff=%s first=%s len %d/%d %s' % (spec or 'none', st, nd, first, len(ours), len(theirs), inj))
    if os.environ.get('SHOW') and first is not None:
        from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_BIG_ENDIAN
        import struct
        md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
        def dis(w, k):
            for i in md.disasm(struct.pack('>I', w), va + 4 * k):
                return '%s %s' % (i.mnemonic, i.op_str)
            return '.word %08x' % w
        a, b = (int(x) for x in os.environ['SHOW'].split(':'))
        for k in range(max(first - a, 0), min(first + b, len(ours))):
            print('%08X %s %-34s | %s' % (va + 4 * k, '!!' if ours[k] != theirs[k] else '  ', dis(ours[k], k), dis(theirs[k], k)))
