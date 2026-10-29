#!/usr/bin/env python3
"""regionextras.py -- what one Top Gear Rally library in Remastered takes from
the other cartridges, appended to its own cartridge data at ROM 0x800000
(ports/tgrally/tools/assets.py, RM_EXTRAS).

    regionextras.py usa|jpn OUT

Remastered builds Top Gear Rally twice, from the USA cartridge and from
Japan's (its PAL/Japan menus), and each needs a little of the other:

  0x800000  the other front-end tune: the USA library gets Japan's (the same
            as Europe's), the Japan library the USA's.  A packed record as
            the cartridge has it (its header, then its compressed bytes).
  0x840000  the other cartridge's three cars, which Remastered offers beside
            this one's (Japan replaced Type-RS with Type-PJ, Type-M3 with
            Type-TR, the Helmet with Type-MN): a count, then for each its
            kind, its 0x60-byte car record as the other cartridge has it
            (big-endian, its model's ROM offset moved to its copy here, its
            name's address 0) and its name (16 bytes); then the three model
            files, packed records.

Needs the USA and Japan ROMs (reference/tgrally, as tools/tgrally/region.py
names them).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))

BASE = 0x800000
CARS = 0x40000                      # the cars' block, from BASE
TWINS = (3, 6, 9)                   # the kinds Japan changed: Type-RS/PJ, M3/TR, Helmet/MN


def rom(region):
    import region as R
    p = R.rom_path(region)
    R.check_rom(p, region)
    return open(p, 'rb').read()


def at(region, usa_va):
    """a USA address at its home in region's cartridge"""
    import csv
    if region == 'usa':
        return usa_va
    for r in csv.DictReader(open(os.path.join(ROOT, 'config/tgrally', region, 'addrmap.csv'))):
        if int(r['usa'], 16) == usa_va and r['va']:
            return int(r['va'], 16)
    raise SystemExit('regionextras: %08X has no home in %s' % (usa_va, region))


def record(data, off):
    """a packed record: its header (compressed length, unpacked size), then
    that many bytes"""
    total, = struct.unpack('>I', data[off:off + 4])
    return data[off:off + 8 + total]


def va_rom(region, va):
    import region as R
    return va - R.BASE + R.ROMOFF


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ('usa', 'jpn'):
        sys.exit(__doc__)
    me, out = sys.argv[1], sys.argv[2]
    other = 'jpn' if me == 'usa' else 'usa'
    blob = bytearray()
    tune = record(rom(other), at(other, 0x000EBC00))
    if len(tune) > CARS:
        sys.exit('regionextras: the tune is %d bytes, more than its slot' % len(tune))
    blob += tune
    blob += b'\0' * (CARS - len(blob))
    jp = rom(other)
    table = va_rom(other, at(other, 0x8028AE0C))
    head = struct.pack('>I', len(TWINS))
    files = bytearray()
    first = CARS + 4 + len(TWINS) * (4 + 0x60 + 16)
    for kind in TWINS:
        rec = bytearray(jp[table + kind * 0x60:table + kind * 0x60 + 0x60])
        name_va, = struct.unpack('>I', rec[0:4])
        name = jp[va_rom(other, name_va):va_rom(other, name_va) + 16].split(b'\0')[0][:15]
        model, = struct.unpack('>I', rec[0x14:0x18])
        f = record(jp, model)
        rec[0:4] = b'\0\0\0\0'
        rec[0x14:0x18] = struct.pack('>I', BASE + first + len(files))
        head += struct.pack('>I', kind) + bytes(rec) + name.ljust(16, b'\0')
        files += f
        files += b'\0' * (-len(files) & 15)
    blob += head + files
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, 'wb').write(bytes(blob))
    print('regionextras: %s library: the %s tune (%d bytes), its cars (%d bytes)'
          % (me, other, len(tune), len(head) + len(files)))


if __name__ == '__main__':
    main()
