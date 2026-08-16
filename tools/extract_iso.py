"""Extract a file from the retail CD image (MODE1/2352 raw sectors).

WHY RAW SECTORS MATTER: a .BIN ripped from a CD stores 2352 bytes per sector,
of which only 2048 are user data -- 16 bytes of sync/header come first and 288
bytes of EDC/ECC follow. So a byte offset inside the ISO filesystem is NOT a
byte offset inside the .BIN, and any tool that assumes it is will read
plausible-looking garbage that drifts further out of alignment the deeper it
reads. The mapping is applied explicitly in iso_read().

The image is verified to be 2352-byte sectors before anything is read: the
first sector must begin with the CD sync pattern, and the file length must be
an exact multiple of 2352.

Usage:  extract_iso.py <image.bin> <NAME.EXT> <outfile>
"""
import sys, struct

RAW, USER, HDR = 2352, 2048, 16
SYNC = b'\x00' + b'\xff' * 10 + b'\x00'


def open_image(path):
    fh = open(path, 'rb')
    if fh.read(12) != SYNC:
        raise SystemExit("%s: not a MODE1/2352 image (no CD sync at sector 0)" % path)
    fh.seek(0, 2)
    n = fh.tell()
    if n % RAW:
        raise SystemExit("%s: length %d is not a multiple of %d" % (path, n, RAW))
    return fh, n // RAW


def iso_read(fh, off, count):
    """Read `count` bytes at ISO logical offset `off`, skipping sector headers."""
    out = bytearray()
    while count > 0:
        lba, within = divmod(off, USER)
        take = min(USER - within, count)
        fh.seek(lba * RAW + HDR + within)
        out += fh.read(take)
        off += take
        count -= take
    return bytes(out)


def find_record(fh, nsectors, want):
    """Scan directory extents for `want`, returning (lba, size).

    Deliberately a scan rather than a directory-tree walk: the tree needs the
    PVD, the root record and recursion, and all this needs is one file whose
    name is known. A scan cannot get lost.
    """
    want = want.upper().encode()
    for lba in range(16, min(nsectors, 4096)):
        fh.seek(lba * RAW + HDR)
        sec = fh.read(USER)
        i = 0
        while i < USER - 33:
            length = sec[i]
            if length < 34:
                i += 1
                continue
            nlen = sec[i + 32]
            name = sec[i + 33:i + 33 + nlen]
            if name.split(b';')[0] == want:
                ext = struct.unpack_from('<I', sec, i + 2)[0]
                size = struct.unpack_from('<I', sec, i + 10)[0]
                return ext, size
            i += length
    return None, None


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    image, want, out = sys.argv[1:]
    fh, nsectors = open_image(image)
    lba, size = find_record(fh, nsectors, want)
    if lba is None:
        raise SystemExit("%s: not found in %s" % (want, image))
    data = iso_read(fh, lba * USER, size)
    if len(data) != size:
        raise SystemExit("short read: wanted %d, got %d" % (size, len(data)))
    open(out, 'wb').write(data)
    print("%s: lba %d, %d bytes -> %s" % (want, lba, size, out))
    if data[:2] == b'MZ':
        print("  (MZ header present -- looks like a PE image)")


if __name__ == '__main__':
    main()
