#!/usr/bin/env python3
"""Spanning-CAB extractor (MSZIP), enough for the VS97 SP3 set.

The SP3 distribution is one self-extracting stub plus eight .cab volumes
forming a single spanning cabinet set; the stub's own EXTRACT and Wine's
expand both fail to walk the chain, so this does it directly.

Format facts used (MS cabinet spec):
- A folder's MSZIP blocks share one deflate history: each block is 'CK' +
  raw deflate, decompressed with the previous blocks' output as dictionary.
- A folder may CONTINUE across volumes: the last folder of volume i and the
  first folder of volume i+1 are one logical folder when file records carry
  the CONTD markers (iFolder 0xFFFD from-prev / 0xFFFE to-next / 0xFFFF both).
- A file record lives in the volume where the file STARTS; continuation
  volumes re-list it with iFolder=0xFFFD (skip those). uoff indexes into the
  LOGICAL folder's uncompressed stream.

    python3 tools/cabx.py <first-volume(.exe|.cab)> <outdir> [glob]
"""
import fnmatch, os, struct, sys, zlib

CONTD_FROM_PREV = 0xFFFD
CONTD_TO_NEXT   = 0xFFFE
CONTD_BOTH      = 0xFFFF


def cab_bytes(path):
    b = open(path, 'rb').read()
    i = -1
    while True:
        i = b.find(b'MSCF', i + 1)
        if i < 0:
            raise SystemExit('no valid MSCF in ' + path)
        try:
            (_s, _r1, cb, _r2, coff, _r3, vmin, vmaj, nfold, nfile,
             _fl, _sid, _ic) = struct.unpack_from('<4sIIIIIBBHHHHH', b, i)
        except struct.error:
            continue
        if vmaj == 1 and vmin == 3 and 0 < coff < cb and nfold < 1000 \
                and nfile < 20000 and cb <= len(b) - i + 16:
            return b[i:]


class Vol(object):
    def __init__(self, path):
        d = cab_bytes(path)
        (_s, _r1, self.cb, _r2, coffFiles, _r3, _vmin, _vmaj, cFolders,
         cFiles, flags, _sid, _ic) = struct.unpack_from('<4sIIIIIBBHHHHH', d, 0)
        off = 36
        self.res_data = 0
        res_folder = 0
        if flags & 4:
            cbH, res_folder, self.res_data = struct.unpack_from('<HBB', d, off)
            off += 4 + cbH
        def zstr():
            nonlocal off
            j = d.index(b'\0', off); s = d[off:j]; off = j + 1
            return s.decode('latin-1')
        self.prev = zstr() if flags & 1 else None
        if flags & 1: zstr()
        self.next = zstr() if flags & 2 else None
        if flags & 2: zstr()
        self.folders = []
        for _ in range(cFolders):
            coffData, cData, typ = struct.unpack_from('<IHH', d, off)
            off += 8 + res_folder
            self.folders.append((coffData, cData, typ & 0xF))
        self.files = []
        off = coffFiles
        for _ in range(cFiles):
            cb2, uoff, iFolder, _dt, _tm, _at = struct.unpack_from('<IIHHHH', d, off)
            off += 16
            j = d.index(b'\0', off); nm = d[off:j].decode('latin-1'); off = j + 1
            self.files.append((nm, cb2, uoff, iFolder))
        self.data = d

    def blocks(self, fi):
        coffData, cData, typ = self.folders[fi]
        off = coffData
        for _ in range(cData):
            _csum, cbData, cbUn = struct.unpack_from('<IHH', self.data, off)
            off += 8 + self.res_data
            yield typ, self.data[off:off + cbData], cbUn
            off += cbData


def main():
    first, outdir = sys.argv[1], sys.argv[2]
    pat = (sys.argv[3] if len(sys.argv) > 3 else '*').lower()
    vol_dir = os.path.dirname(os.path.abspath(first)) or '.'
    vols, path, seen = [], first, set()
    while path and path not in seen:
        seen.add(path)
        vols.append(Vol(path))
        nxt = vols[-1].next
        path = None
        if nxt:
            for f in os.listdir(vol_dir):
                if f.lower() == nxt.lower():
                    path = os.path.join(vol_dir, f); break
    print('%d volumes' % len(vols))

    # Build logical folders: list of lists of (vol_idx, folder_idx).
    logical = []
    for vi, v in enumerate(vols):
        first_is_contd = vi > 0 and any(f[3] in (CONTD_FROM_PREV, CONTD_BOTH)
                                        for f in v.files)
        for fi in range(len(v.folders)):
            if fi == 0 and first_is_contd and logical:
                logical[-1].append((vi, fi))
            else:
                logical.append([(vi, fi)])

    def logical_of(vi, fi):
        for li, parts in enumerate(logical):
            if (vi, fi) in parts:
                return li
        raise KeyError((vi, fi))

    # Decompress each logical folder once, dictionary carried across blocks
    # and volume boundaries.
    streams = []
    for parts in logical:
        out = bytearray()
        for vi, fi in parts:
            for typ, cdata, cbUn in vols[vi].blocks(fi):
                if typ == 1:
                    if cdata[:2] != b'CK':
                        raise SystemExit('bad MSZIP block sig')
                    z = zlib.decompressobj(-15, zdict=bytes(out[-32768:]))
                    u = z.decompress(cdata[2:]) + z.flush()
                elif typ == 0:
                    u = cdata
                else:
                    raise SystemExit('unsupported compression type %d' % typ)
                out += u
        streams.append(bytes(out))

    n = short = 0
    for vi, v in enumerate(vols):
        for nm, size, uoff, iFolder in v.files:
            if iFolder in (CONTD_FROM_PREV, CONTD_BOTH) and vi > 0:
                continue                      # re-listed continuation record
            fi = 0 if iFolder >= CONTD_FROM_PREV else iFolder
            if iFolder == CONTD_TO_NEXT or iFolder == CONTD_BOTH:
                fi = len(v.folders) - 1
            li = logical_of(vi, fi)
            blob = streams[li][uoff:uoff + size]
            rel = nm.replace('\\', '/')
            if not fnmatch.fnmatch(rel.lower(), pat):
                continue
            if len(blob) != size:
                short += 1
                print('SHORT %s: %d of %d' % (nm, len(blob), size))
                continue
            dst = os.path.join(outdir, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'wb').write(blob)
            n += 1
    print('extracted %d files, %d short' % (n, short))


if __name__ == '__main__':
    main()
