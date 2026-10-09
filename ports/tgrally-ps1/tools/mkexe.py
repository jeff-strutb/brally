#!/usr/bin/env python3
"""mkexe.py ELF EXE -- a PS-EXE from a linked ELF: the 2 KB header the BIOS
reads (entry, gp, load address and size, the region's licence line) and the
image from the first loaded byte to the end of .data, padded to 2 KB."""
import struct
import sys

PT_LOAD = 1
REGION = b'Sony Computer Entertainment Inc. for North America area'


def main():
    src, dst = sys.argv[1], sys.argv[2]
    elf = open(src, 'rb').read()
    if elf[:4] != b'\x7fELF' or elf[4] != 1 or elf[5] != 1:
        sys.exit('mkexe: %s is not a 32-bit little-endian ELF' % src)
    entry, phoff = struct.unpack_from('<II', elf, 0x18)[0], struct.unpack_from('<I', elf, 0x1C)[0]
    phentsize, phnum = struct.unpack_from('<HH', elf, 0x2A)
    segs = []
    for i in range(phnum):
        p_type, p_off, p_va, _, p_filesz, p_memsz, _, _ = struct.unpack_from('<8I', elf, phoff + i * phentsize)
        if p_type == PT_LOAD and p_filesz:
            segs.append((p_va, elf[p_off:p_off + p_filesz]))
    lo = min(va for va, _ in segs)
    hi = max(va + len(b) for va, b in segs)
    img = bytearray(hi - lo)
    for va, b in segs:
        img[va - lo:va - lo + len(b)] = b
    img += bytes(-len(img) % 2048)
    hdr = bytearray(2048)
    hdr[0:8] = b'PS-X EXE'
    struct.pack_into('<IIII', hdr, 0x10, entry, 0, lo, len(img))
    struct.pack_into('<II', hdr, 0x30, 0, 0)            # crt0 sets its own stack
    hdr[0x4C:0x4C + len(REGION)] = REGION
    open(dst, 'wb').write(bytes(hdr) + bytes(img))
    print('%s: %d bytes loaded at %08X, entry %08X' % (dst, len(img), lo, entry))


if __name__ == '__main__':
    main()
