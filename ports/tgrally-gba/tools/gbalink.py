#!/usr/bin/env python3
"""gbalink.py OUT.gba OBJ... -- link ARM (not Thumb) ELF objects into a GBA
cartridge image, so the proof of concept needs nothing but the host's clang.

  ROM   0x08000000  .crt0 first, then code and read-only data, then the load
                    images of the IWRAM and EWRAM data
  IWRAM 0x03000000  .iwram* (copied by crt0), then .iwram_bss* (zeroed)
  EWRAM 0x02000000  .data* (copied), then .bss*, .ewram_bss* and COMMON (zeroed)

Relocations: R_ARM_ABS32, R_ARM_REL32, R_ARM_PC24/CALL/JUMP24, R_ARM_V4BX.
Calls from ROM to IWRAM need -mlong-calls (a branch reaches only 32 MB).
Writes OUT.gba and OUT.map (symbol addresses)."""
import struct
import sys

ROM, IWRAM, EWRAM = 0x08000000, 0x03000000, 0x02000000
SHT_NOBITS, SHT_SYMTAB, SHT_REL, SHF_ALLOC = 8, 2, 9, 2


class Obj:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        self.path = path
        shoff, = struct.unpack_from('<I', d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
        self.sh = [struct.unpack_from('<10I', d, shoff + i * shentsize) for i in range(shnum)]
        strtab = self.sh[shstrndx]
        self.names = [self.cstr(strtab[4] + s[0]) for s in self.sh]
        self.syms = []
        for i, s in enumerate(self.sh):
            if s[1] == SHT_SYMTAB:
                st = self.sh[s[6]]
                for j in range(s[5] // 16):
                    name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', d, s[4] + j * 16)
                    self.syms.append((self.cstr(st[4] + name), value, size, info, shndx))

    def cstr(self, off):
        return self.d[off:self.d.index(b'\0', off)].decode()


def region(name, typ):
    if name == '.crt0':
        return 'crt0'
    if name.startswith('.iwram_bss'):
        return 'iwbss'
    if name.startswith('.iwram'):
        return 'iwram'
    if typ == SHT_NOBITS or name.startswith(('.bss', '.sbss', '.ewram_bss')):
        return 'bss'
    if name.startswith('.data'):
        return 'data'
    return 'rom'


def align(x, a):
    return (x + a - 1) // a * a if a > 1 else x


def main():
    out, objs = sys.argv[1], [Obj(p) for p in sys.argv[2:]]
    secs = {k: [] for k in ('crt0', 'rom', 'iwram', 'iwbss', 'data', 'bss')}
    for o in objs:
        for i, s in enumerate(o.sh):
            name = o.names[i]
            if not (s[2] & SHF_ALLOC) or name.startswith('.ARM.exidx') or s[5] == 0:
                continue
            secs[region(name, s[1])].append((o, i))
    addr = {}

    def place(key, base):
        a = base
        for o, i in secs[key]:
            s = o.sh[i]
            a = align(a, max(s[8], 1))
            addr[(o, i)] = a
            a += s[5]
        return a

    end_rom = place('rom', place('crt0', ROM))
    iw_end = place('iwram', IWRAM)
    iwb_start = align(iw_end, 4)
    iwb_end = place('iwbss', iwb_start)
    d_end = place('data', EWRAM)
    b_start = align(d_end, 4)
    b_end = place('bss', b_start)
    iw_lma = align(end_rom, 4)
    d_lma = align(iw_lma + (iw_end - IWRAM), 4)
    rom_size = align(d_lma + (d_end - EWRAM), 4) - ROM
    common_at = b_end
    glob = {'__iwram_lma': iw_lma, '__iwram_start': IWRAM, '__iwram_end': align(iw_end, 4),
            '__data_lma': d_lma, '__data_start': EWRAM, '__data_end': align(d_end, 4),
            '__iwram_bss_start': iwb_start, '__iwram_bss_end': align(iwb_end, 4),
            '__bss_start': b_start}
    for o in objs:                                   # globals, and COMMON at the end of BSS
        for name, value, size, info, shndx in o.syms:
            bind = info >> 4
            if bind == 0 or not name:
                continue
            if shndx == 0xFFF2:
                common_at = align(common_at, max(value, 4))
                glob.setdefault(name, common_at)
                common_at += size
            elif shndx and shndx < 0xFF00 and (o, shndx) in addr:
                glob[name] = addr[(o, shndx)] + value
    glob['__bss_end'] = align(common_at, 4)
    if glob['__bss_end'] > 0x02040000 or glob['__iwram_bss_end'] > 0x03007400:
        sys.exit('gbalink: EWRAM ends %X, IWRAM %X: too big' % (glob['__bss_end'], glob['__iwram_bss_end']))
    rom = bytearray(rom_size)
    iw_img = bytearray(iw_end - IWRAM)
    d_img = bytearray(d_end - EWRAM)

    def image(o, i):
        a, s = addr[(o, i)], o.sh[i]
        if s[1] == SHT_NOBITS:
            return None, 0
        for buf, base in ((rom, ROM), (iw_img, IWRAM), (d_img, EWRAM)):
            if base <= a < base + len(buf) + 1 and (base != ROM or a < ROM + rom_size):
                if a + s[5] <= base + len(buf):
                    return buf, base
        return None, 0

    for key in ('crt0', 'rom', 'iwram', 'data'):
        for o, i in secs[key]:
            buf, base = image(o, i)
            s = o.sh[i]
            buf[addr[(o, i)] - base:addr[(o, i)] - base + s[5]] = o.d[s[4]:s[4] + s[5]]
    undefined = set()
    for o in objs:
        for ri, s in enumerate(o.sh):
            if s[1] != SHT_REL or (o, s[7]) not in addr:
                continue
            tgt = s[7]
            buf, base = image(o, tgt)
            if buf is None:
                continue
            for k in range(s[5] // 8):
                off, info = struct.unpack_from('<II', o.d, s[4] + k * 8)
                typ, si = info & 0xFF, info >> 8
                name, value, size, sinfo, shndx = o.syms[si]
                if shndx == 0:
                    if name not in glob:
                        undefined.add(name)
                        continue
                    S = glob[name]
                elif shndx == 0xFFF2:
                    S = glob[name]
                elif shndx >= 0xFF00:
                    S = value
                else:
                    S = addr.get((o, shndx), 0) + value
                P = addr[(o, tgt)] + off
                at = P - base
                w, = struct.unpack_from('<I', buf, at)
                if typ == 2:                          # ABS32
                    struct.pack_into('<I', buf, at, (S + w) & 0xFFFFFFFF)
                elif typ == 3:                        # REL32
                    struct.pack_into('<I', buf, at, (S + w - P) & 0xFFFFFFFF)
                elif typ in (1, 28, 29):              # PC24, CALL, JUMP24
                    A = ((w & 0xFFFFFF) ^ 0x800000) - 0x800000
                    v = S + (A << 2) - P
                    if S & 1 or not -(1 << 25) <= v < (1 << 25):
                        sys.exit('gbalink: branch to %s out of reach from %X (use -marm -mlong-calls)' % (name, P))
                    struct.pack_into('<I', buf, at, (w & 0xFF000000) | ((v >> 2) & 0xFFFFFF))
                elif typ == 40:                       # V4BX
                    pass
                else:
                    sys.exit('gbalink: relocation type %d (%s) in %s' % (typ, name, o.path))
    if undefined:
        sys.exit('gbalink: undefined: ' + ' '.join(sorted(undefined)))
    rom[iw_lma - ROM:iw_lma - ROM + len(iw_img)] = iw_img
    rom[d_lma - ROM:d_lma - ROM + len(d_img)] = d_img
    rom[0xA0:0xAC] = b'TGRALLY POC '
    rom[0xAC:0xB0] = b'CTGE'
    rom[0xB0:0xB2] = b'00'
    rom[0xB2] = 0x96
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    open(out, 'wb').write(rom)
    with open(out.rsplit('.', 1)[0] + '.map', 'w') as f:
        for name, a in sorted(glob.items(), key=lambda x: x[1]):
            f.write('%08X %s\n' % (a, name))
    print('%s: ROM %d KB, IWRAM %d B code + %d B bss, EWRAM %d B data + %d B bss' % (
        out, rom_size // 1024, iw_end - IWRAM, iwb_end - iwb_start, d_end - EWRAM, glob['__bss_end'] - b_start))


if __name__ == '__main__':
    main()
