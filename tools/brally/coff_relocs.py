"""Extract per-reloc (offset_in_func, symbol_name, type) for one .text symbol
in a COFF .obj. Complements match_diff.parse_coff_obj (which drops names)."""
import struct

def func_relocs(obj_path, want):
    data = open(obj_path,'rb').read()
    nsec = struct.unpack_from('<H', data, 2)[0]
    symptr = struct.unpack_from('<I', data, 8)[0]
    nsym = struct.unpack_from('<I', data, 12)[0]
    strtab = symptr + nsym*18
    def symname(i):
        off = symptr + i*18
        raw = data[off:off+8]
        if raw[:4]==b'\x00\x00\x00\x00':
            so = struct.unpack_from('<I', raw, 4)[0]
            end = data.index(b'\x00', strtab+so)
            return data[strtab+so:end].decode('latin1')
        return raw.split(b'\x00')[0].decode('latin1')
    # section headers
    secs=[]
    for s in range(nsec):
        off = 20 + s*40
        name = data[off:off+8].split(b'\x00')[0].decode('latin1')
        raw = struct.unpack_from('<I', data, off+20)[0]
        roff = struct.unpack_from('<I', data, off+24)[0]
        nrel = struct.unpack_from('<H', data, off+32)[0]
        secs.append((name, raw, roff, nrel))
    # find the symbol's section + value
    target=None
    for i in range(nsym):
        off = symptr + i*18
        nm = symname(i)
        secnum = struct.unpack_from('<h', data, off+12)[0]
        val = struct.unpack_from('<I', data, off+8)[0]
        cls = data[off+16]
        aux = data[off+17]
        clean = nm.lstrip('_@').split('@')[0]
        if clean==want and secnum>0:
            target=(secnum-1, val); break
        i2=i  # skip aux handled by range (we ignore aux rows loosely)
    if target is None: raise KeyError(want)
    seci, fval = target
    name, raw, roff, nrel = secs[seci]
    out=[]
    for r in range(nrel):
        o = roff + r*10
        rva = struct.unpack_from('<I', data, o)[0]
        sidx = struct.unpack_from('<I', data, o+4)[0]
        typ = struct.unpack_from('<H', data, o+8)[0]
        # only relocs within this function
        foff = rva - fval
        if foff < 0: continue
        out.append((foff, symname(sidx), typ))  # typ 6=DIR32, 0x14=REL32
    out.sort()
    return out

if __name__=='__main__':
    import sys
    for t in func_relocs(sys.argv[1], sys.argv[2]):
        print("%04x  %-8s %s"%(t[0], hex(t[2]), t[1]))
