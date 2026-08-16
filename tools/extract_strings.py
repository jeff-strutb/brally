"""Recover the localised UI strings from BRString.dll.

BRString.dll is a RESOURCE-ONLY DLL: it has a .rsrc and a .reloc and no code
at all. The strings are Win32 RT_STRING (type 6) resources, stored as UTF-16LE,
which is why a plain `strings` pass over the file finds nothing but the DOS
stub and the section names.

RT_STRING packs strings in BLOCKS OF SIXTEEN. A resource whose name-id is N
holds ids (N-1)*16 .. (N-1)*16+15, and inside the block each entry is a u16
character count followed by that many UTF-16 code units -- with EMPTY entries
written as a bare zero length, not omitted. So the id of any given string
depends on counting the empty slots correctly; skipping them silently shifts
every following id, which would put the wrong caption on every menu control.

Usage:  extract_strings.py <BRString.dll> <out.txt>
"""
import sys, struct

RT_STRING = 6


def rva_to_off(sections, rva):
    for name, vaddr, vsize, praw, rawsz in sections:
        if vaddr <= rva < vaddr + max(vsize, rawsz):
            return praw + (rva - vaddr)
    return None


def load(path):
    d = open(path, 'rb').read()
    e_lfanew = struct.unpack_from('<I', d, 0x3C)[0]
    nsec = struct.unpack_from('<H', d, e_lfanew + 6)[0]
    optsz = struct.unpack_from('<H', d, e_lfanew + 20)[0]
    sect0 = e_lfanew + 24 + optsz
    secs = []
    for i in range(nsec):
        o = sect0 + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        vsize, vaddr, rawsz, praw = struct.unpack_from('<IIII', d, o + 8)
        secs.append((name, vaddr, vsize, praw, rawsz))
    rsrc = next((s for s in secs if s[0] == '.rsrc'), None)
    if not rsrc:
        raise SystemExit("no .rsrc section")
    return d, secs, rsrc[1], rsrc[3]


def entries(d, base, off):
    """Yield (id_or_name, is_dir, child_offset) for one resource directory."""
    nnamed, nid = struct.unpack_from('<HH', d, off + 12)
    p = off + 16
    for _ in range(nnamed + nid):
        nameid, sub = struct.unpack_from('<II', d, p)
        yield nameid & 0x7FFFFFFF, bool(sub & 0x80000000), (sub & 0x7FFFFFFF)
        p += 8


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    d, secs, rsrc_rva, rsrc_off = load(src)

    out = {}
    for typ, is_dir, toff in entries(d, rsrc_off, rsrc_off):
        if typ != RT_STRING or not is_dir:
            continue
        for nameid, is_dir2, noff in entries(d, rsrc_off, rsrc_off + toff):
            if not is_dir2:
                continue
            for _lang, _isd, loff in entries(d, rsrc_off, rsrc_off + noff):
                data_rva, size = struct.unpack_from('<II', d, rsrc_off + loff)
                blk = rva_to_off(secs, data_rva)
                base_id = (nameid - 1) * 16
                p, i = blk, 0
                while i < 16 and p < blk + size:
                    (n,) = struct.unpack_from('<H', d, p)
                    p += 2
                    if n:
                        s = d[p:p + n * 2].decode('utf-16-le', 'replace')
                        out[base_id + i] = s
                        p += n * 2
                    i += 1

    with open(dst, 'w', encoding='utf-8') as fh:
        fh.write("# Recovered from BRString.dll (RT_STRING, UTF-16LE).\n")
        fh.write("# id<TAB>text. Empty slots are omitted here but were COUNTED\n")
        fh.write("# when assigning ids -- see the note in extract_strings.py.\n")
        for k in sorted(out):
            fh.write("%d\t%s\n" % (k, out[k].replace('\n', '\\n')))
    print("recovered %d strings -> %s" % (len(out), dst))
    for k in sorted(out)[:12]:
        print("  %4d  %r" % (k, out[k]))


if __name__ == '__main__':
    main()
