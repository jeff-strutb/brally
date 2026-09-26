"""Place compiled Top Gear Rally functions into the ROM image.

A tiny linker for the N64 lane.  It takes a function out of an IDO object and
produces its bytes at a chosen address, with every relocation resolved:

  named symbols      through n64/config/symbols_tgr.csv (or func_/D_ names)
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


def link_function(obj, name, place_va, data_va, fnvas, syms, self_va=None):
    """-> (code bytes, [(va, bytes)] data blocks).

    place_va  where the code goes
    data_va   where this object's .rodata/.data go (placed whole)
    fnvas     name -> VA for the object's other functions
    self_va   where calls to the function itself should go (default place_va)
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
    for sname in ('.rodata', '.data', '.sdata', '.bss'):
        si, blob = obj.sec(sname)
        if si is None or not blob:
            continue
        cur = (cur + 15) & ~15
        secbase[sname] = cur
        blocks.append([cur, bytearray(blob), si])
        cur += len(blob)

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
    for o, typ, si in rels:
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
            if typ == 5:
                lo = next((o2 for (o2, t2, s2) in rels if o2 > o and t2 == 6 and s2 == si), None)
                if lo is None:
                    raise LinkError('HI16 without LO16')
                addend = ((w & 0xffff) << 16) + sext16(B.words(text[lo:lo + 4])[0] & 0xffff)
            else:
                hi = max((o2 for (o2, t2, s2) in rels if o2 < o and t2 == 5 and s2 == si), default=None)
                hw = B.words(text[hi:hi + 4])[0] if hi is not None else 0
                addend = ((hw & 0xffff) << 16) + sext16(w & 0xffff)
            if kind == 'sec':
                if val == '.text':
                    tgt = text_va(addend)
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
                elif val in secbase:
                    v = secbase[val] + add
                else:
                    raise LinkError('data pointer into %s' % val)
            else:
                v = val + add
            struct.pack_into('>I', blob, o, v & 0xffffffff)

    code = b''.join(struct.pack('>I', x) for x in out)
    return code, [(b, bytes(blob)) for b, blob, _ in blocks]
