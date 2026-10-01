#!/usr/bin/env python3
"""Minimal PE32 reader for the original DLL: sections, base relocations,
reads by virtual address."""
import struct


class PE:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        e = struct.unpack_from('<I', self.d, 0x3C)[0]
        nsec, = struct.unpack_from('<H', self.d, e + 6)
        optsz, = struct.unpack_from('<H', self.d, e + 20)
        opt = e + 24
        self.base, = struct.unpack_from('<I', self.d, opt + 28)
        ndd, = struct.unpack_from('<I', self.d, opt + 92)
        self.dd = [struct.unpack_from('<II', self.d, opt + 96 + 8 * i) for i in range(ndd)]
        self.secs = []
        so = opt + optsz
        for i in range(nsec):
            name = self.d[so:so + 8].rstrip(b'\0').decode()
            vsz, va, rsz, rp = struct.unpack_from('<IIII', self.d, so + 8)
            self.secs.append((name, va, vsz, rp, rsz))
            so += 40

    def off(self, rva):
        for n, va, vsz, rp, rsz in self.secs:
            if va <= rva < va + max(vsz, rsz):
                return rp + rva - va if rva - va < rsz else None
        return None

    def read(self, va, n):
        o = self.off(va - self.base)
        return self.d[o:o + n] if o is not None else b'\0' * n

    def dword(self, va):
        return struct.unpack('<I', self.read(va, 4))[0]

    def relocs(self):
        """VAs of every HIGHLOW fixup (each holds an absolute address)."""
        rva, size = self.dd[5]
        o = self.off(rva)
        end = o + size
        out = []
        while o < end:
            page, bsz = struct.unpack_from('<II', self.d, o)
            if bsz == 0:
                break
            for i in range((bsz - 8) // 2):
                ent, = struct.unpack_from('<H', self.d, o + 8 + 2 * i)
                if ent >> 12 == 3:
                    out.append(self.base + page + (ent & 0xFFF))
            o += bsz
        return out
