"""Compare a compiled function against the original BRD3D.dll bytes.

The decomp verification loop:

    cl.exe /c src/pod.cpp  ->  pod.obj
    python tools/objdiff.py pod.obj ?PodFile@@... 0x10008810

Both sides are normalised the same way -- every relocated dword and every rel32
CALL/JMP displacement is zeroed -- because those hold addresses that cannot
match until link time. What is left is the instruction encoding, which is what
"matching" actually means.

With no arguments this runs a self-test of the diff engine against the original
binaries, so the harness can be trusted before a compiler exists.
"""
import sys, os, csv, struct, bisect
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32)


# ---------------------------------------------------------------- COFF .obj
class Obj:
    """Just enough COFF object reader to pull a named function's bytes."""

    def __init__(self, path):
        d = self.data = open(path, 'rb').read()
        mach, nsec, ts, symptr, nsym, optsz, chars = struct.unpack('<HHIIIHH', d[:20])
        if mach != 0x14c:
            raise SystemExit("not an i386 COFF object (machine %04x)" % mach)
        self.sections = []
        for i in range(nsec):
            s = d[20 + i * 40: 60 + i * 40]
            name = s[:8].rstrip(b'\0').decode('latin1')
            vsize, vaddr, rawsz, rawptr, relptr = struct.unpack('<IIIII', s[8:28])
            nrel = struct.unpack('<H', s[32:34])[0]
            self.sections.append(dict(name=name, rawptr=rawptr, rawsz=rawsz,
                                      relptr=relptr, nrel=nrel, index=i + 1))
        # symbol table
        strtab_off = symptr + nsym * 18
        self.symbols = {}
        i = 0
        while i < nsym:
            e = d[symptr + i * 18: symptr + (i + 1) * 18]
            if e[:4] == b'\0\0\0\0':
                off = struct.unpack('<I', e[4:8])[0]
                end = d.index(b'\0', strtab_off + off)
                name = d[strtab_off + off:end].decode('latin1')
            else:
                name = e[:8].rstrip(b'\0').decode('latin1')
            value, secnum, _typ, sclass, naux = struct.unpack('<IhHBB', e[8:18])
            self.symbols.setdefault(name, (secnum, value))
            i += 1 + naux

    def func_bytes(self, name, size=None):
        if name not in self.symbols:
            near = [k for k in self.symbols if name in k]
            raise SystemExit("symbol %r not found. candidates: %s" % (name, near[:8]))
        secnum, value = self.symbols[name]
        sec = next(s for s in self.sections if s['index'] == secnum)
        blob = bytearray(self.data[sec['rawptr']: sec['rawptr'] + sec['rawsz']])
        # zero this section's relocation targets
        for k in range(sec['nrel']):
            r = self.data[sec['relptr'] + k * 10: sec['relptr'] + (k + 1) * 10]
            vaddr = struct.unpack('<I', r[:4])[0]
            if 0 <= vaddr <= len(blob) - 4:
                blob[vaddr:vaddr + 4] = b'\0\0\0\0'
        end = len(blob) if size is None else value + size
        return bytes(blob[value:end])


# ---------------------------------------------------------------- original
def orig_bytes(va, dll='orig/BRD3D.dll', fcsv='config/functions.csv'):
    p = pelib.load(dll)
    text, text_va = p.text()
    rows = [(int(r['va'], 16), int(r['size'])) for r in csv.DictReader(open(fcsv))]
    size = dict(rows).get(va)
    if size is None:
        raise SystemExit("no function at %08X in %s" % (va, fcsv))
    b = bytearray(text[va - text_va: va - text_va + size])
    relocs = {p.image_base + r for r in p.relocs}
    for k in range(len(b) - 3):
        if (va + k) in relocs:
            b[k:k + 4] = b'\0\0\0\0'
    return bytes(b)


def zero_rel32(b):
    b = bytearray(b)
    i = 0
    while i < len(b) - 4:
        if b[i] in (0xE8, 0xE9):
            b[i + 1:i + 5] = b'\0\0\0\0'
            i += 5
        else:
            i += 1
    return bytes(b)


def insns(b, base=0):
    return [(i.address - base, i.mnemonic, i.op_str, i.bytes.hex())
            for i in md.disasm(b, base)]


def report(a, b, label_a="original", label_b="compiled"):
    a, b = zero_rel32(a), zero_rel32(b)
    if a == b:
        print("MATCH  (%d bytes)" % len(a))
        return True
    ia, ib = insns(a), insns(b)
    print("MISMATCH  %s=%d bytes  %s=%d bytes" % (label_a, len(a), label_b, len(b)))
    print("%-42s | %s" % (label_a, label_b))
    print("-" * 90)
    n = max(len(ia), len(ib))
    shown = 0
    for k in range(n):
        ra = "%-6s %-28s" % (ia[k][1], ia[k][2]) if k < len(ia) else ""
        rb = "%-6s %-28s" % (ib[k][1], ib[k][2]) if k < len(ib) else ""
        same = (k < len(ia) and k < len(ib) and ia[k][3] == ib[k][3])
        if same and shown > 40:
            continue
        print("%s%-42s | %s" % (" " if same else ">", ra, rb))
        shown += 1
    return False


def self_test():
    """Validate the diff engine using only the original binaries."""
    print("=== objdiff self-test (no compiler required) ===\n")
    print("1. a function against itself -- must MATCH")
    a = orig_bytes(0x10008780)
    ok1 = report(a, a)

    print("\n2. two different functions -- must MISMATCH")
    b = orig_bytes(0x100087B0)
    ok2 = not report(a, b, "GetPodLength", "ReadPod")

    print("\n3. a cross-build pair (BRD3D vs BRGlide, same source) -- should MATCH")
    import collections, hashlib
    rows = list(csv.DictReader(open('config/shared.csv'))) if os.path.exists('config/shared.csv') else []
    pair = next((r for r in rows if r['class'] == 'shared' and int(r['size']) > 40), None)
    ok3 = None
    if pair:
        x = orig_bytes(int(pair['d3d_va'], 16))
        y = orig_bytes(int(pair['glide_va'], 16), 'orig/BRGlide.dll', 'config/functions_glide.csv')
        print("   %s vs %s (%s bytes)" % (pair['d3d_va'], pair['glide_va'], pair['size']))
        ok3 = report(x, y, "BRD3D", "BRGlide")
    print("\nengine works: %s" % ("yes" if (ok1 and ok2) else "NO"))


def main():
    if len(sys.argv) == 1:
        self_test()
        return
    obj, sym, va = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    a = orig_bytes(va)
    b = Obj(obj).func_bytes(sym, len(a))
    report(a, b)


if __name__ == '__main__':
    main()
