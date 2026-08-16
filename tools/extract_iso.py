"""Read the retail CD image (MODE1/2352 raw sectors): list it, or pull a file out.

WHY RAW SECTORS MATTER: a .BIN ripped from a CD stores 2352 bytes per sector,
of which only 2048 are user data -- 16 bytes of sync/header come first and 288
bytes of EDC/ECC follow. So a byte offset inside the ISO filesystem is NOT a
byte offset inside the .BIN, and any tool that assumes it is will read
plausible-looking garbage that drifts further out of alignment the deeper it
reads. The mapping is applied explicitly in iso_read().

The image is verified to be 2352-byte sectors before anything is read: the
first sector must begin with the CD sync pattern, and the file length must be
an exact multiple of 2352.

The listing walks the real ISO 9660 directory tree from the Primary Volume
Descriptor, so it reports the actual filesystem -- names, sizes, LBAs and full
paths -- rather than whatever a heuristic scan happens to recognise. The
extract path still uses the tolerant scan, because for a single known name a
scan cannot get lost.

Usage:
    extract_iso.py <image.bin> <NAME.EXT> <outfile>     extract one file
    extract_iso.py --list <image.bin>                   list the whole tree
    extract_iso.py --extract-path <image.bin> <ISO/PATH> <outfile>
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


# ---------------------------------------------------------------- ISO 9660

# Set when the volume carries a Joliet supplementary descriptor, in which case
# directory names are UCS-2 big-endian rather than 8.3 ASCII.
JOLIET = [False]

# The three escape sequences ECMA-119 assigns to Joliet's UCS-2 levels.
_JOLIET_ESC = (b'%/@', b'%/C', b'%/E')


def read_pvd(fh):
    """Return (root_lba, root_size), preferring JOLIET over the 8.3 tree.

    WHY THIS MATTERS AND IS NOT COSMETIC: this disc stores long names in a
    Joliet supplementary descriptor and MANGLED 8.3 names in the primary one.
    Reading only the primary yields BUT-MA~1.BMP where the game asks for
    but-maind.bmp -- so five sprites the executable names could not be found,
    and it looked as though the disc simply did not ship them. It ships all of
    them; the primary tree just cannot spell them.

    The supplementary descriptor is preferred when present, and the escape
    sequence is checked rather than assumed, because a type-2 descriptor is not
    necessarily Joliet.
    """
    primary = None
    for lba in range(16, 32):
        d = iso_read(fh, lba * USER, USER)
        if d[1:6] != b'CD001':
            continue
        if d[0] == 1 and primary is None:
            root = d[156:156 + 34]
            primary = (struct.unpack_from('<I', root, 2)[0],
                       struct.unpack_from('<I', root, 10)[0])
        elif d[0] == 2:
            esc = d[88:120].rstrip(b'\x00')
            if esc in _JOLIET_ESC:
                JOLIET[0] = True
                root = d[156:156 + 34]
                return (struct.unpack_from('<I', root, 2)[0],
                        struct.unpack_from('<I', root, 10)[0])
        elif d[0] == 255:
            break
    if primary is not None:
        return primary
    raise SystemExit("no Primary Volume Descriptor found")


def read_dir(fh, lba, size):
    """Yield (name, child_lba, child_size, is_dir) for one directory extent."""
    data = iso_read(fh, lba * USER, size)
    i = 0
    while i < len(data):
        length = data[i]
        if length == 0:
            # records never straddle a 2048-byte block; skip to the next one
            i = (i // USER + 1) * USER
            if i >= len(data):
                break
            continue
        nlen = data[i + 32]
        name = bytes(data[i + 33:i + 33 + nlen])
        flags = data[i + 25]
        child_lba = struct.unpack_from('<I', data, i + 2)[0]
        child_size = struct.unpack_from('<I', data, i + 10)[0]
        if nlen == 1 and name in (b'\x00', b'\x01'):
            pass                            # '.' and '..'
        else:
            if JOLIET[0]:
                # UCS-2 BE; the ';1' version suffix is encoded too.
                text = name.decode('utf-16-be', 'replace').split(';')[0]
            else:
                text = name.split(b';')[0].decode('ascii', 'replace')
            yield (text,
                   child_lba, child_size, bool(flags & 0x02))
        i += length


def walk(fh, lba=None, size=None, prefix=''):
    """Recursively yield (path, lba, size, is_dir) for the whole tree."""
    if lba is None:
        lba, size = read_pvd(fh)
    for name, clba, csize, isdir in read_dir(fh, lba, size):
        path = prefix + '/' + name if prefix else name
        yield (path, clba, csize, isdir)
        if isdir:
            for row in walk(fh, clba, csize, path):
                yield row


def resolve(fh, isopath):
    """Look up a '/'-separated ISO path, returning (lba, size)."""
    want = isopath.upper().replace('\\', '/').strip('/')
    for path, lba, size, isdir in walk(fh):
        if not isdir and path.upper() == want:
            return lba, size
    return None, None


def cmd_list(image):
    fh, nsectors = open_image(image)
    rows = list(walk(fh))
    total = 0
    for path, lba, size, isdir in sorted(rows):
        if isdir:
            print("%-52s   <DIR>              lba %d" % (path, lba))
        else:
            print("%-52s %12d bytes   lba %d" % (path, size, lba))
            total += size
    nfile = sum(1 for r in rows if not r[3])
    ndir = len(rows) - nfile
    print("\n%d files, %d directories, %d bytes total (data track is %d sectors)"
          % (nfile, ndir, total, nsectors))


def cmd_extract(image, want, out, bypath=False):
    fh, nsectors = open_image(image)
    if bypath:
        lba, size = resolve(fh, want)
    else:
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


def main():
    a = sys.argv[1:]
    if len(a) == 2 and a[0] in ('--list', '-l'):
        cmd_list(a[1])
    elif len(a) == 4 and a[0] == '--extract-path':
        cmd_extract(a[1], a[2], a[3], bypath=True)
    elif len(a) == 3:
        cmd_extract(a[0], a[1], a[2])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
