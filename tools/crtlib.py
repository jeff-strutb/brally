#!/usr/bin/env python3
"""Read a COFF import/archive library (.LIB) without lib.exe.

The shipped CRT libraries (tools/msvc5/lib/LIBC.LIB and friends) are plain
`!<arch>` archives whose members are ordinary COFF .obj files -- the
compiler's own output for the CRT source we vendored at tools/msvc5/crt/src.
lib.exe under wine can extract them too, but it fights back (case mangling,
one file per invocation, member paths with backslashes); sixty lines of
parser is cheaper than scripting around it.

    from crtlib import members
    for name, data in members('tools/msvc5/lib/LIBC.LIB'):
        ...  # name like 'strtol.obj', data is the raw COFF obj

Members named '/' (linker directories) and '//' (the long-name string
table) are consumed internally and never yielded.  Import-library members
(IMAGE_FILE_MACHINE 0 stubs for MSVCRT.LIB) are yielded as-is; the caller's
COFF parser decides what it can use.
"""
import os
import struct


def members(path):
    """Yield (member_name, member_bytes) for every real member of `path`."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'!<arch>\n':
        raise ValueError('%s is not an ar archive' % path)
    longnames = b''
    off = 8
    while off + 60 <= len(data):
        hdr = data[off:off + 60]
        if hdr[58:60] != b'`\n':
            raise ValueError('%s: bad member header at 0x%x' % (path, off))
        rawname = hdr[0:16].rstrip()
        size = int(hdr[48:58])
        body = data[off + 60:off + 60 + size]
        off += 60 + size
        if off & 1:                      # members are 2-byte aligned
            off += 1
        if rawname == b'/':              # linker member (symbol directory)
            continue
        if rawname == b'//':             # long-name table
            longnames = body
            continue
        if rawname.startswith(b'/'):     # '/123' -> offset into longnames
            k = int(rawname[1:])
            end = longnames.index(b'\x00', k)
            name = longnames[k:end].decode('ascii', 'replace')
        else:
            name = rawname.decode('ascii', 'replace')
        name = name.rstrip('/')          # short names end in '/'
        # lib.exe stores build paths ('build\intel\st_obj\strtol.obj');
        # only the basename identifies the source file.
        name = name.replace('\\', '/').split('/')[-1].lower()
        yield name, body


if __name__ == '__main__':
    import sys
    n = t = 0
    for name, body in members(sys.argv[1]):
        n += 1
        t += len(body)
        if '-v' in sys.argv:
            print('%-20s %d' % (name, len(body)))
    print('%d members, %d bytes' % (n, t))
