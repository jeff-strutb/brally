"""Census the core by instruction-pattern, to find what can be decompiled
mechanically rather than by hand.

Hand-tracing 1,708 functions one at a time is not a viable rate. But a large
fraction of any C codebase compiled by MSVC is formulaic: field getters,
field setters, struct clears, global load/store, thin forwarders. Those have
recognisable instruction signatures and can be emitted automatically, leaving
hand work for the genuinely intricate ones (x87 math, control flow, vtables).

This tool classifies every shared function and reports how much of the core
each pattern covers, so effort goes where it actually pays.
"""
import sys, os, csv, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_MEM, CS_OP_REG, CS_OP_IMM

DLL = 'orig/BRD3D.dll'


def classify(ins):
    """Return (pattern_name, detail) for a decoded function body."""
    if not ins:
        return ('empty', '')
    mn = [i.mnemonic for i in ins]
    ops = [i.op_str for i in ins]
    n = len(ins)

    # strip a trailing ret for shape matching
    if mn and mn[-1] == 'ret':
        core_mn, core_ops = mn[:-1], ops[:-1]
    else:
        return ('no-ret', '')          # tail-call, jump table, etc.

    body = list(zip(core_mn, core_ops))

    if not body:
        return ('stub-ret', '')

    # thiscall field getter:  mov eax, [ecx+N] ; ret
    if len(body) == 1 and body[0][0] == 'mov' and \
       body[0][1].startswith('eax, dword ptr [ecx'):
        return ('this-getter', body[0][1])

    # cdecl field getter: mov eax,[esp+4] ; mov eax,[eax+N] ; ret
    if len(body) == 2 and all(m == 'mov' for m, _ in body) and \
       'esp' in body[0][1] and 'eax' in body[1][1]:
        return ('arg-getter', body[1][1])

    # global load: mov eax, [imm32] ; ret
    if len(body) == 1 and body[0][0] == 'mov' and \
       body[0][1].startswith('eax, dword ptr [0x'):
        return ('global-get', body[0][1])

    # global store: mov eax,[esp+4] ; mov [imm32], eax ; ret
    if len(body) == 2 and body[0][0] == 'mov' and body[1][0] == 'mov' and \
       'esp' in body[0][1] and '[0x' in body[1][1]:
        return ('global-set', body[1][1])

    # all-mov field writes (clears / small initialisers)
    if all(m == 'mov' for m, _ in body) and len(body) <= 24:
        if any('[e' in o and o.startswith('dword ptr') is False for _, o in body):
            return ('field-writes', '%d stores' % len(body))

    # pure x87: needs hand work
    if any(m.startswith('f') for m in core_mn):
        return ('x87-math', '%d insns' % n)

    # has branches: control flow, hand work
    if any(m.startswith('j') for m in core_mn):
        return ('branching', '%d insns' % n)

    # has calls
    if 'call' in core_mn:
        return ('calls-out', '%d insns' % n)

    return ('other', '%d insns' % n)


def main():
    p = pelib.load(DLL)
    text, text_va = p.text()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    cls = {}
    if os.path.exists('config/shared.csv'):
        for r in csv.DictReader(open('config/shared.csv')):
            cls[int(r['va'], 16)] = r['class']

    funcs = [(int(r['va'], 16), int(r['size']))
             for r in csv.DictReader(open('config/functions.csv'))]

    counts = collections.Counter()
    bytes_ = collections.Counter()
    examples = collections.defaultdict(list)

    rows = []
    for va, size in funcs:
        if cls.get(va) != 'shared' or size < 1:
            continue
        b = text[va - text_va: va - text_va + size]
        ins = list(md.disasm(b, va))
        pat, detail = classify(ins)
        counts[pat] += 1
        bytes_[pat] += size
        if len(examples[pat]) < 3:
            examples[pat].append('%08X' % va)
        rows.append(('0x%08X' % va, size, pat, detail))

    with open('config/patterns.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'size', 'pattern', 'detail'])
        w.writerows(rows)

    total = sum(counts.values())
    print("shared functions classified: %d\n" % total)
    print("  %-14s %6s %7s %7s  %s" % ("pattern", "count", "pct", "bytes", "examples"))
    for pat, c in counts.most_common():
        print("  %-14s %6d %6.1f%% %7d  %s"
              % (pat, c, 100.0 * c / total, bytes_[pat],
                 ' '.join(examples[pat])))

    mech = sum(counts[k] for k in
               ('this-getter', 'arg-getter', 'global-get', 'global-set',
                'field-writes', 'stub-ret'))
    print("\nmechanically emittable now: %d (%.1f%%)" % (mech, 100.0 * mech / total))
    print("needs hand work:            %d" % (total - mech))


if __name__ == '__main__':
    main()
