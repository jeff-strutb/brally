"""Minimal PE/COFF reader for the Boss Rally decomp.

Only what the project needs: sections, imports, exports, base relocations.
No external dependencies.
"""
import struct
from dataclasses import dataclass, field


@dataclass
class Section:
    name: str
    vaddr: int          # RVA
    vsize: int
    raw_ptr: int
    raw_size: int

    def contains_rva(self, rva: int) -> bool:
        return self.vaddr <= rva < self.vaddr + max(self.vsize, self.raw_size)


@dataclass
class PE:
    data: bytes
    image_base: int
    entry_rva: int
    sections: list = field(default_factory=list)
    imports: dict = field(default_factory=dict)     # va -> "DLL!Name"
    exports: dict = field(default_factory=dict)     # va -> name
    relocs: set = field(default_factory=set)        # RVAs of 32-bit fixups

    # --- address helpers -------------------------------------------------
    def sect_for_rva(self, rva):
        for s in self.sections:
            if s.contains_rva(rva):
                return s
        return None

    def rva_to_off(self, rva):
        s = self.sect_for_rva(rva)
        if s is None or rva - s.vaddr >= s.raw_size:
            return None
        return s.raw_ptr + (rva - s.vaddr)

    def va_to_off(self, va):
        return self.rva_to_off(va - self.image_base)

    def read(self, va, n):
        o = self.va_to_off(va)
        if o is None:
            return None
        return self.data[o:o + n]

    def u32(self, va):
        b = self.read(va, 4)
        return None if b is None or len(b) < 4 else struct.unpack('<I', b)[0]

    def section_by_name(self, name):
        for s in self.sections:
            if s.name == name:
                return s
        return None

    def text(self):
        s = self.section_by_name('.text')
        return self.data[s.raw_ptr:s.raw_ptr + s.raw_size], self.image_base + s.vaddr


def load(path) -> PE:
    d = open(path, 'rb').read()
    pe_off = struct.unpack('<I', d[0x3c:0x40])[0]
    assert d[pe_off:pe_off + 4] == b'PE\0\0', "not a PE"
    nsec = struct.unpack('<H', d[pe_off + 6:pe_off + 8])[0]
    optsz = struct.unpack('<H', d[pe_off + 20:pe_off + 22])[0]
    opt = pe_off + 24
    magic = struct.unpack('<H', d[opt:opt + 2])[0]
    assert magic == 0x10b, "PE32+ not supported"
    entry_rva = struct.unpack('<I', d[opt + 16:opt + 20])[0]
    image_base = struct.unpack('<I', d[opt + 28:opt + 32])[0]
    ddbase = opt + 96
    dd = [struct.unpack('<II', d[ddbase + i * 8:ddbase + i * 8 + 8]) for i in range(16)]

    pe = PE(data=d, image_base=image_base, entry_rva=entry_rva)
    so = pe_off + 24 + optsz
    for i in range(nsec):
        s = d[so + i * 40: so + (i + 1) * 40]
        vs, va, rs, ra = struct.unpack('<IIII', s[8:24])
        pe.sections.append(Section(s[:8].rstrip(b'\0').decode('latin1'), va, vs, ra, rs))

    def cstr(rva):
        o = pe.rva_to_off(rva)
        if o is None:
            return None
        e = d.index(b'\0', o)
        return d[o:e].decode('latin1')

    # ---- imports: record the IAT slot VA for each imported symbol -------
    irva, _ = dd[1]
    if irva:
        o = pe.rva_to_off(irva)
        while True:
            ent = d[o:o + 20]
            if len(ent) < 20 or ent == b'\0' * 20:
                break
            oft, _, _, name_rva, first_thunk = struct.unpack('<IIIII', ent)
            dll = cstr(name_rva) or "?"
            lookup = oft or first_thunk
            lo = pe.rva_to_off(lookup)
            k = 0
            while lo is not None:
                t = struct.unpack('<I', d[lo + k * 4: lo + k * 4 + 4])[0]
                if t == 0:
                    break
                if t & 0x80000000:
                    nm = "#%d" % (t & 0xFFFF)
                else:
                    nm = cstr(t + 2) or "?"
                slot_va = image_base + first_thunk + k * 4
                pe.imports[slot_va] = "%s!%s" % (dll, nm)
                k += 1
            o += 20

    # ---- exports --------------------------------------------------------
    erva, _ = dd[0]
    if erva:
        o = pe.rva_to_off(erva)
        nnames = struct.unpack('<I', d[o + 24:o + 28])[0]
        addr_rva = struct.unpack('<I', d[o + 28:o + 32])[0]
        name_rva = struct.unpack('<I', d[o + 32:o + 36])[0]
        ord_rva = struct.unpack('<I', d[o + 36:o + 40])[0]
        po, oo, ao = pe.rva_to_off(name_rva), pe.rva_to_off(ord_rva), pe.rva_to_off(addr_rva)
        for i in range(nnames):
            nr = struct.unpack('<I', d[po + i * 4:po + i * 4 + 4])[0]
            ordi = struct.unpack('<H', d[oo + i * 2:oo + i * 2 + 2])[0]
            fn = struct.unpack('<I', d[ao + ordi * 4:ao + ordi * 4 + 4])[0]
            pe.exports[image_base + fn] = cstr(nr)

    # ---- base relocations (type 3 = HIGHLOW, a 32-bit address fixup) ----
    rrva, rsz = dd[5]
    if rrva:
        o = pe.rva_to_off(rrva)
        end = o + rsz
        while o < end:
            page, blk = struct.unpack('<II', d[o:o + 8])
            if blk < 8:
                break
            for k in range((blk - 8) // 2):
                e = struct.unpack('<H', d[o + 8 + k * 2:o + 10 + k * 2])[0]
                if (e >> 12) == 3:
                    pe.relocs.add(page + (e & 0xFFF))
            o += blk
    return pe


if __name__ == '__main__':
    import sys
    p = load(sys.argv[1])
    print("image base %08X  entry %08X" % (p.image_base, p.image_base + p.entry_rva))
    for s in p.sections:
        print("  %-8s rva %08X vsize %8d raw %8d" % (s.name, s.vaddr, s.vsize, s.raw_size))
    print("imports %d  exports %s  relocs %d" % (len(p.imports), p.exports, len(p.relocs)))
