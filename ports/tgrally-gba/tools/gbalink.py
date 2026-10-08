#!/usr/bin/env python3
"""gbalink.py OUT.gba OBJ... -- link ARM (not Thumb) ELF objects into a GBA
cartridge image, so the proof of concept needs nothing but the host's clang.

  ROM   0x08000000  .crt0 first, then code and read-only data, then the load
                    images of the IWRAM and EWRAM data
  IWRAM 0x03000000  .iwram* (copied by crt0), then the overlays, then .iwram_bss* (zeroed)
        overlays    .ovl_NAME*: code that takes turns in one region after .iwram, each
                    linked there with its image in ROM (__ovl_NAME_lma, __ovl_NAME_words;
                    __ovl_base): the program copies in the one it is about to run
  EWRAM 0x02000000  .data* (copied), then .bss*, .ewram_bss* and COMMON (zeroed)

Relocations: R_ARM_ABS32, R_ARM_REL32, R_ARM_PC24/CALL/JUMP24, R_ARM_V4BX.  A branch reaches
only 32 MB: one between the cartridge and IWRAM goes through a stub the link adds beside the
caller (at the end of the ROM's code, or of the resident IWRAM's), which loads the target and
BXes to it (so it may be Thumb).
GBALINK_PLACE=FILE: lines "FUNCTION REGION" put a function's own section (-ffunction-sections:
.text.FUNCTION) into IWRAM ("iwram") or an overlay (its NAME).
Writes OUT.gba and OUT.map (symbol addresses)."""
import os
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
    if name.startswith('.ovl_'):
        return 'ovl' + name[4:].split('.')[0]
    if typ == SHT_NOBITS or name.startswith(('.bss', '.sbss', '.ewram_bss')):
        return 'bss'
    if name.startswith('.data'):
        return 'data'
    return 'rom'


def align(x, a):
    return (x + a - 1) // a * a if a > 1 else x


def stub_key(o, sec, off, name, value, sinfo, shndx):
    """a far branch's target: the symbol, and the offset from it the instruction's addend makes"""
    w, = struct.unpack_from('<I', o.d, sec[4] + off)
    eff = value + (((w & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4 + 8
    return (name, eff) if shndx == 0 or sinfo >> 4 else (id(o), shndx, eff)


def mem(key):
    return 'rom' if key in ('crt0', 'rom') else 'iw' if key == 'iwram' or key.startswith('ovl_') else 'ram'


def main():
    out, objs = sys.argv[1], [Obj(p) for p in sys.argv[2:]]
    place_of = {}
    if os.environ.get('GBALINK_PLACE'):
        for line in open(os.environ['GBALINK_PLACE']):
            f = line.split('#')[0].split()
            if len(f) == 2:
                place_of['.text.' + f[0]] = 'iwram' if f[1] == 'iwram' else 'ovl_' + f[1]
    secs = {k: [] for k in ('crt0', 'rom', 'iwram', 'iwbss', 'data', 'bss')}
    key_of = {}
    for o in objs:
        for i, s in enumerate(o.sh):
            name = o.names[i]
            if not (s[2] & SHF_ALLOC) or name.startswith('.ARM.exidx') or s[5] == 0:
                continue
            k = place_of.get(name) or region(name, s[1])
            secs.setdefault(k, []).append((o, i))
            key_of[(o, i)] = k
    ovls = sorted(k for k in secs if k.startswith('ovl_'))
    defs = {}                                        # each global's section
    for o in objs:
        for name, value, size, info, shndx in o.syms:
            if info >> 4 and shndx and shndx < 0xFF00 and (o, shndx) in key_of:
                defs[name] = (o, shndx)
    far = {'rom': [], 'iw': []}                      # the stubs: (target symbol key) by the caller's memory
    for o in objs:
        for s in o.sh:
            if s[1] != SHT_REL or (o, s[7]) not in key_of:
                continue
            src = mem(key_of[(o, s[7])])
            for k in range(s[5] // 8):
                off, info = struct.unpack_from('<II', o.d, s[4] + k * 8)
                if info & 0xFF not in (1, 28, 29):
                    continue
                name, value, size, sinfo, shndx = o.syms[info >> 8]
                tgt = defs.get(name) if shndx == 0 else (o, shndx) if shndx < 0xFF00 else None
                if tgt is None or tgt not in key_of:
                    continue
                if mem(key_of[tgt]) != src:
                    t = stub_key(o, o.sh[s[7]], off, name, value, sinfo, shndx)
                    if t not in far[src]:
                        far[src].append(t)
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
    stub_rom = align(end_rom, 4)
    end_rom = stub_rom + 12 * len(far['rom'])
    iw_end = place('iwram', IWRAM)
    stub_iw = align(iw_end, 4)
    iw_end = stub_iw + 12 * len(far['iw'])
    ovl_base = align(iw_end, 4)
    ovl_end = {k: align(place(k, ovl_base), 4) for k in ovls}
    ovl_top = max(list(ovl_end.values()) + [ovl_base])
    iwb_start = align(ovl_top, 4)
    iwb_end = place('iwbss', iwb_start)
    d_end = place('data', EWRAM)
    b_start = align(d_end, 4)
    b_end = place('bss', b_start)
    iw_lma = align(end_rom, 4)
    ovl_lma, a = {}, align(iw_lma + (iw_end - IWRAM), 4)
    for k in ovls:
        ovl_lma[k] = a
        a = align(a + ovl_end[k] - ovl_base, 4)
    d_lma = a
    rom_size = align(d_lma + (d_end - EWRAM), 4) - ROM
    common_at = b_end
    glob = {'__iwram_lma': iw_lma, '__iwram_start': IWRAM, '__iwram_end': align(iw_end, 4),
            '__data_lma': d_lma, '__data_start': EWRAM, '__data_end': align(d_end, 4),
            '__iwram_bss_start': iwb_start, '__iwram_bss_end': align(iwb_end, 4),
            '__bss_start': b_start, '__ovl_base': ovl_base}
    for k in ovls:
        glob['__%s_lma' % k] = ovl_lma[k]
        glob['__%s_words' % k] = (ovl_end[k] - ovl_base) // 4
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
    ovl_img = {k: bytearray(ovl_end[k] - ovl_base) for k in ovls}
    ovl_of = {sec: k for k in ovls for sec in secs[k]}

    def image(o, i):
        a, s = addr[(o, i)], o.sh[i]
        if s[1] == SHT_NOBITS:
            return None, 0
        if (o, i) in ovl_of:
            return ovl_img[ovl_of[(o, i)]], ovl_base
        for buf, base in ((rom, ROM), (iw_img, IWRAM), (d_img, EWRAM)):
            if base <= a < base + len(buf) + 1 and (base != ROM or a < ROM + rom_size):
                if a + s[5] <= base + len(buf):
                    return buf, base
        return None, 0

    for key in ['crt0', 'rom', 'iwram', 'data'] + ovls:
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
                    src = mem(key_of[(o, tgt)])
                    t = stub_key(o, o.sh[tgt], off, name, value, sinfo, shndx)
                    if t in far[src]:                 # through its stub
                        S = (stub_rom if src == 'rom' else stub_iw) + 12 * far[src].index(t) - (A << 2) - 8
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
    objs_by_id = {id(o): o for o in objs}
    for src, base, buf, bb in (('rom', stub_rom, rom, ROM), ('iw', stub_iw, iw_img, IWRAM)):
        for n, t in enumerate(far[src]):         # ldr ip, [pc]; bx ip; .word target
            if len(t) == 2:
                if t[0] not in glob:
                    sys.exit('gbalink: stub to undefined %s' % t[0])
                S = glob[t[0]] + t[1]
            else:
                S = addr[(objs_by_id[t[0]], t[1])] + t[2]
            struct.pack_into('<III', buf, base + 12 * n - bb, 0xE59FC000, 0xE12FFF1C, S & 0xFFFFFFFF)
    rom[iw_lma - ROM:iw_lma - ROM + len(iw_img)] = iw_img
    rom[d_lma - ROM:d_lma - ROM + len(d_img)] = d_img
    for k in ovls:
        rom[ovl_lma[k] - ROM:ovl_lma[k] - ROM + len(ovl_img[k])] = ovl_img[k]
    rom[0xA0:0xAC] = b'TGRALLY POC '
    rom[0xAC:0xB0] = b'CTGE'
    rom[0xB0:0xB2] = b'00'
    rom[0xB2] = 0x96
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    open(out, 'wb').write(rom)
    local = {}                                       # static functions too, for the profiles
    for o in objs:
        for name, value, size, info, shndx in o.syms:
            if info >> 4 == 0 and info & 0xF == 2 and name and (o, shndx) in addr and name not in glob:
                local[name + '.'] = addr[(o, shndx)] + value
    with open(out.rsplit('.', 1)[0] + '.map', 'w') as f:
        for name, a in sorted(list(glob.items()) + list(local.items()), key=lambda x: x[1]):
            f.write('%08X %s\n' % (a, name))
    print('%s: ROM %d KB, IWRAM %d B code + %d B bss, overlays %s, EWRAM %d B data + %d B bss' % (
        out, rom_size // 1024, iw_end - IWRAM, iwb_end - iwb_start,
        ' '.join('%s %d B' % (k[4:], ovl_end[k] - ovl_base) for k in ovls) or 'none', d_end - EWRAM,
        glob['__bss_end'] - b_start))


if __name__ == '__main__':
    main()
