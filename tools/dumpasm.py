"""Emit annotated per-function disassembly for the decomp to work against.

Annotations: resolved call targets (named where known), inlined string literals,
import names, and .data global references tagged with a stable `g_<rva>` symbol.

    python tools/dumpasm.py                 # dump every function to asm/
    python tools/dumpasm.py 0x10008810      # dump one function to stdout
"""
import sys, os, csv, struct, bisect
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
import names as namelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

# DEFAULT REFERENCE: the GLIDE build.
#
# BRD3D.dll is the Direct3D build; BRGlide.dll is Glide. This tool defaulted to
# BRD3D for most of the project's life, and the agent briefs compounded it by
# describing BRD3D as "the Glide build" -- a plain factual error, repeated many
# times. Glide was the mature target for this game, and is the intended
# reference.
#
# Override with BR_REF=orig/BRD3D.dll when you specifically need the D3D build
# (e.g. to compare the two, or to read the statically linked CRT, which Glide
# imports from MSVCRT instead).
import os
DLL = os.environ.get('BR_REF', 'orig/BRGlide.dll')

# The function map must match the binary. Pairing BRGlide.dll with the D3D map
# silently disassembles the wrong bytes at the right-looking address, which is
# worse than failing.
MAP = os.environ.get('BR_MAP',
                     'config/functions_glide.csv' if 'Glide' in DLL
                     else 'config/functions.csv')


class Ctx:
    def __init__(self):
        self.p = pelib.load(DLL)
        self.text, self.text_va = self.p.text()
        self.strings = namelib.collect_strings(self.p)
        self.funcs = [(int(r['va'], 16), int(r['size']))
                      for r in csv.DictReader(open(MAP))]
        self.funcs.sort()
        self.starts = [f[0] for f in self.funcs]
        self.names = {}
        if os.path.exists('config/names.csv'):
            for r in csv.DictReader(open('config/names.csv')):
                self.names[int(r['va'], 16)] = r['name']
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True

    def fname(self, va):
        return self.names.get(va) or "sub_%08X" % va

    def size_of(self, va):
        i = bisect.bisect_left(self.starts, va)
        return self.funcs[i][1] if i < len(self.funcs) and self.funcs[i][0] == va else 0

    def annotate(self, ins):
        """Comment describing any address referenced by this instruction."""
        notes = []
        for op in ins.operands:
            vals = []
            if op.type == 2:                       # immediate
                vals.append(op.imm & 0xFFFFFFFF)
            elif op.type == 3 and op.mem.base == 0 and op.mem.index == 0:
                vals.append(op.mem.disp & 0xFFFFFFFF)
            for v in vals:
                if v in self.strings:
                    s = self.strings[v].replace('\n', '\\n')
                    notes.append('"%s"' % (s[:60] + ('...' if len(s) > 60 else '')))
                elif v in self.p.imports:
                    notes.append(self.p.imports[v])
                elif self.text_va <= v < self.text_va + len(self.text):
                    if v in self.names or self.size_of(v):
                        notes.append(self.fname(v))
                else:
                    sec = self.p.sect_for_rva(v - self.p.image_base)
                    if sec and sec.name in ('.data', '.rdata'):
                        notes.append("g_%06X" % (v - self.p.image_base))
        return "  ; " + ", ".join(dict.fromkeys(notes)) if notes else ""

    def dump(self, va, out, size=None):
        """Disassemble `size` bytes at `va`, or the map's extent if size is None.

        AN EXPLICIT SIZE OVERRIDES THE MAP ON PURPOSE. This tool used to accept a
        size argument on the command line and silently DISCARD it, always using
        config/functions.csv's extent -- and that map has documented failure
        modes: it invents entries, misses entries, truncates functions and, in at
        least one case that mattered, SPLITS one function into two at a jump
        target in the middle of it.

        The cost was not hypothetical. The Glide text emitter is one function of
        about 3050 bytes; the map calls it 1019 plus a separate 2287. Asking for
        1019 bytes and being handed exactly 1019 looked like confirmation, and a
        "the Glide emitter is a third the size of D3D's" conclusion was published
        off the back of it. Passing a size now does what it says.
        """
        mapped = self.size_of(va)
        if size is None:
            size = mapped
        elif mapped and size != mapped:
            print("; NOTE: explicit size %d overrides the map's %d for %08X"
                  % (size, mapped, va), file=out)
        if not size:
            print("no function at %08X (pass an explicit size to dump anyway)"
                  % va, file=sys.stderr)
            return
        b = self.text[va - self.text_va: va - self.text_va + size]
        print("; ---------------------------------------------------------------", file=out)
        print("; %s  @ %08X  (%d bytes)" % (self.fname(va), va, size), file=out)
        print("; ---------------------------------------------------------------", file=out)
        for ins in self.md.disasm(b, va):
            print("%08X  %-22s %-8s %-34s%s" % (
                ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str,
                self.annotate(ins)), file=out)
        print(file=out)


def main():
    ctx = Ctx()
    if len(sys.argv) > 1:
        want = int(sys.argv[2], 0) if len(sys.argv) > 2 else None
        ctx.dump(int(sys.argv[1], 16), sys.stdout, want)
        return
    os.makedirs('asm', exist_ok=True)
    n = 0
    # one file per 64KB block keeps the tree navigable
    handles = {}
    for va, size in ctx.funcs:
        if size < 1:
            continue
        blk = va & ~0xFFFF
        if blk not in handles:
            handles[blk] = open('asm/%08X.asm' % blk, 'w')
        ctx.dump(va, handles[blk])
        n += 1
    for h in handles.values():
        h.close()
    print("dumped %d functions into %d files under asm/" % (n, len(handles)))


if __name__ == '__main__':
    main()
