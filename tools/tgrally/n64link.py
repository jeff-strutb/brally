"""Place compiled Top Gear Rally functions into the ROM image.

A tiny linker for the N64 lane.  It takes a function out of an IDO object and
produces its bytes at a chosen address, with every relocation resolved:

  named symbols      through config/tgrally/symbols_tgr.csv (or func_/D_ names)
  the object's own   functions tagged @implements resolve to their ROM
  functions          address (or to where they are being placed)
  .rodata / .data    the object's literal sections are placed at a data
                     address the caller supplies (they are the body's own
                     strings, float constants and jump tables)

Used by n64image.py (the M2 and M1 image builds) and by the live oracle, which
runs a candidate body in place of the original.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64build as B  # noqa: E402


class LinkError(Exception):
    pass


def sext16(x):
    return x - 0x10000 if x & 0x8000 else x


def link_function(obj, name, place_va, data_va, fnvas, syms, self_va=None, static_va=None):
    """-> (code bytes, [(va, bytes)] data blocks).

    place_va  where the code goes
    data_va   where this object's .rodata/.data go (placed whole)
    fnvas     name -> VA for the object's other functions
    self_va   where calls to the function itself should go (default place_va)
    static_va section -> ROM VA for sections that hold the file's statics
              (n64build.static_bases): references go to the statics' ROM
              homes, and those sections are not placed at data_va; a
              (section, addend) key gives one static its own home
    """
    ti, text = obj.sec('.text')
    pieces = {n: (s, e) for n, s, e in B.carve(obj) if n}
    if name not in pieces:
        raise LinkError('%s is not defined in the object' % name)
    start, end = pieces[name]
    ws = B.words(text[start:end])
    while ws and ws[-1] == 0:
        ws.pop()
    self_va = place_va if self_va is None else self_va

    # data sections, laid out one after another from data_va (8-aligned)
    secbase, blocks, cur = {}, [], data_va
    static_va = static_va or {}
    for sname in ('.rodata', '.data', '.sdata', '.bss'):
        si, blob = obj.sec(sname)
        if sname in static_va:
            secbase[sname] = static_va[sname]
            continue
        if si is None or not blob:
            continue
        cur = (cur + 15) & ~15
        secbase[sname] = cur
        blocks.append([cur, bytearray(blob), si])
        cur += len(blob)

    # The object's literals that already sit in the ROM: when a prefix of its
    # .rodata (its strings, in order) is byte-for-byte the ROM's .rodata at
    # one place, references into that prefix go to the ROM's copy, so a
    # pointer to a literal that the body stores or passes on is the
    # original's pointer.  The ROM's .rodata is never written, so the content
    # at either address is the same.
    romlit = rom_literals(obj)

    def rodata_va(addend):
        if romlit and addend < romlit[1]:
            return romlit[0] + addend
        return secbase['.rodata'] + addend

    def text_va(off):
        for n, (s, e) in pieces.items():
            if s <= off < e:
                if n == name:
                    return self_va + (off - s) if off != s else self_va
                if n in fnvas:
                    return fnvas[n] + (off - s)
        raise LinkError('reference into .text+0x%X, which no tagged function owns' % off)

    def target(si):
        s = obj.syms[si]
        if s['type'] == 3:
            return ('sec', obj.secs[s['shndx']]['name'])
        if s['shndx'] == ti:
            # IDO relocates against the symbol itself (the field holds only
            # the addend), so the target is the symbol's own address
            if s['name'] == name:
                return ('abs', self_va)
            v = fnvas.get(s['name'], B.resolve(s['name'], syms))
            if v is None:
                raise LinkError('unresolved %s' % s['name'])
            return ('abs', v)
        v = B.resolve(s['name'], syms)
        if v is None:
            raise LinkError('unresolved %s' % s['name'])
        return ('abs', v)

    rels = obj.rels.get(ti, [])
    out = list(ws)
    for k, (o, typ, si) in enumerate(rels):
        if not (start <= o < end):
            continue
        i = (o - start) // 4
        if i >= len(out):
            continue
        w = ws[i]
        kind, val = target(si)
        if typ == 4:
            if kind == 'sec':
                if val != '.text':
                    raise LinkError('jal into %s' % val)
                tgt = text_va((w & 0x03FFFFFF) << 2)
            else:
                tgt = val + ((w & 0x03FFFFFF) << 2)
            out[i] = (w & 0xFC000000) | ((tgt >> 2) & 0x03FFFFFF)
        elif typ in (5, 6):
            # pairs go by relocation-table order, not by offset: a HI16 takes
            # the next LO16 entry, and a LO16 the last HI16 entry before it
            # (IDO can schedule another literal's addiu between a lui and its
            # own addiu, and every .rodata literal shares one section symbol)
            if typ == 5:
                lo = next((o2 for (o2, t2, s2) in rels[k + 1:] if t2 == 6 and s2 == si), None)
                if lo is None:
                    raise LinkError('HI16 without LO16')
                addend = ((w & 0xffff) << 16) + sext16(B.words(text[lo:lo + 4])[0] & 0xffff)
            else:
                hi = next((o2 for (o2, t2, s2) in reversed(rels[:k]) if t2 == 5 and s2 == si), None)
                hw = B.words(text[hi:hi + 4])[0] if hi is not None else 0
                addend = ((hw & 0xffff) << 16) + sext16(w & 0xffff)
            if kind == 'sec':
                if val == '.text':
                    tgt = text_va(addend)
                elif val == '.rodata' and val in secbase:
                    tgt = rodata_va(addend)
                elif (val, addend) in static_va:
                    tgt = static_va[(val, addend)]
                elif val in secbase:
                    tgt = secbase[val] + addend
                else:
                    raise LinkError('reference to %s' % val)
            else:
                tgt = val + addend
            tgt &= 0xffffffff
            if typ == 5:
                out[i] = (w & 0xFFFF0000) | (((tgt + 0x8000) >> 16) & 0xffff)
            else:
                out[i] = (w & 0xFFFF0000) | (tgt & 0xffff)
        else:
            raise LinkError('relocation type %d in .text' % typ)

    # R_MIPS_32 inside the data sections (jump tables, pointer tables)
    for base, blob, si in blocks:
        for o, typ, sj in obj.rels.get(si, []):
            if typ != 2:
                raise LinkError('relocation type %d in data' % typ)
            add = struct.unpack_from('>I', blob, o)[0]
            kind, val = target(sj)
            if kind == 'sec':
                if val == '.text':
                    v = text_va(add)
                elif val == '.rodata' and val in secbase:
                    v = rodata_va(add)
                elif (val, add) in static_va:
                    v = static_va[(val, add)]
                elif val in secbase:
                    v = secbase[val] + add
                else:
                    raise LinkError('data pointer into %s' % val)
            else:
                v = val + add
            struct.pack_into('>I', blob, o, v & 0xffffffff)

    code = b''.join(struct.pack('>I', x) for x in out)
    return code, [(b, bytes(blob)) for b, blob, _ in blocks]


ROM_RODATA = (0x8026FAB0, 0x802AC400)      # the ROM's .data/.rodata, vram
MIN_LITERALS = 32                          # bytes; shorter prefixes are not trusted
_rom = None


def rom_literals(obj):
    """-> (ROM address, length) of the longest prefix of the object's .rodata
    found byte-exact at exactly one place in the ROM's .rodata, stopping at
    the first word a relocation patches; or None."""
    global _rom
    si, blob = obj.sec('.rodata')
    if si is None or len(blob) < MIN_LITERALS:
        return None
    relocs = [o for o, t, s in obj.rels.get(si, [])]
    limit = min(relocs + [len(blob)])
    if limit < MIN_LITERALS:
        return None
    if _rom is None:
        d = B.Rom().d
        off = ROM_RODATA[0] - B.BASE + B.ROMOFF
        _rom = d[off:off + ROM_RODATA[1] - ROM_RODATA[0]]
    head = bytes(blob[:MIN_LITERALS])
    at = _rom.find(head)
    if at < 0 or _rom.find(head, at + 1) >= 0:
        return None
    n = MIN_LITERALS
    while n < limit and _rom[at + n:at + n + 1] == blob[n:n + 1]:
        n += 1
    return ROM_RODATA[0] + at, n
