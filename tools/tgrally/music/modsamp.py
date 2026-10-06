"""Extract samples from MOD / XM / S3M / IT as numpy float arrays + metadata."""
import struct, numpy as np

def _s(b): return b.split(b'\0')[0].decode('latin-1').rstrip()

def load(data):
    if data[:17] == b'Extended Module: ': return xm(data)
    if data[44:48] == b'SCRM': return s3m(data)
    if data[:4] == b'IMPM': return it(data)
    return mod(data)

def mk(name, pcm, bits, c5=None, fmt='', loop=None, extra=''):
    a = np.asarray(pcm, dtype=np.float64) / (128.0 if bits == 8 else 32768.0)
    return dict(name=name, pcm=a, bits=bits, c5=c5, fmt=fmt, loop=loop, len=len(a), raw=pcm, extra=extra)

def mod(d):
    tag = d[1080:1084]
    nch = {b'M.K.':4, b'M!K!':4, b'FLT4':4, b'4CHN':4, b'6CHN':6, b'8CHN':8, b'FLT8':8}.get(tag)
    if nch is None and tag[2:] == b'CH': nch = int(tag[:2])
    if nch is None and tag[1:] == b'CHN': nch = int(tag[:1])
    nsmp = 31 if nch else 15
    hdr = []
    p = 20
    for i in range(nsmp):
        name = _s(d[p:p+22]); ln = struct.unpack('>H', d[p+22:p+24])[0]*2
        ft = d[p+24] & 15; vol = d[p+25]
        ls = struct.unpack('>H', d[p+26:p+28])[0]*2; ll = struct.unpack('>H', d[p+28:p+30])[0]*2
        hdr.append((name, ln, ft, ls, ll)); p += 30
    norders = d[p]; orders = d[p+2:p+130]; p += 130 + (4 if nsmp == 31 else 0)
    npat = max(orders[:128]) + 1
    p += npat * 64 * 4 * (nch or 4)
    out = []
    for name, ln, ft, ls, ll in hdr:
        raw = np.frombuffer(d[p:p+ln], dtype=np.int8).astype(np.int16); p += ln
        out.append(mk(name, raw, 8, c5=8363, fmt='MOD', loop=(ls, ll) if ll > 2 else None, extra='ft%d' % ft))
    return out

def xm(d):
    hs = struct.unpack_from('<I', d, 60)[0]
    npat, nins = struct.unpack_from('<HH', d, 70)
    q = 60 + hs
    for _ in range(npat):
        h, = struct.unpack_from('<I', d, q); ps, = struct.unpack_from('<H', d, q+7); q += h + ps
    out = []
    for i in range(nins):
        isz, = struct.unpack_from('<I', d, q); iname = _s(d[q+4:q+26]); n, = struct.unpack_from('<H', d, q+27)
        if not n: q += isz; continue
        shs, = struct.unpack_from('<I', d, q+29); q += isz
        hdrs = []
        for j in range(n):
            ln, ls, ll = struct.unpack_from('<III', d, q); ft = struct.unpack_from('<b', d, q+13)[0]
            typ = d[q+14]; rel = struct.unpack_from('<b', d, q+16)[0]; sname = _s(d[q+18:q+40])
            hdrs.append((sname, ln, ls, ll, ft, typ, rel)); q += shs
        for sname, ln, ls, ll, ft, typ, rel in hdrs:
            raw = d[q:q+ln]; q += ln
            if typ & 16:
                a = np.frombuffer(raw[:ln//2*2], dtype='<i2').astype(np.int64); a = np.cumsum(a).astype(np.int16).astype(np.int32); bits = 16
                ls//=2; ll//=2
            else:
                a = np.frombuffer(raw, dtype=np.int8).astype(np.int64); a = np.cumsum(a).astype(np.int8).astype(np.int16); bits = 8
            c5 = 8363 * 2 ** ((rel + ft/128.0) / 12.0)
            out.append(mk('%s / %s' % (iname, sname), a, bits, c5=c5, fmt='XM', loop=(ls, ll) if typ & 3 else None, extra='rel%d ft%d' % (rel, ft)))
    return out

def s3m(d):
    ordn, insn = struct.unpack_from('<HH', d, 32)
    ffi = struct.unpack_from('<H', d, 42)[0]
    ptrs = struct.unpack_from('<%dH' % insn, d, 96 + ordn)
    out = []
    for pp in ptrs:
        o = pp * 16
        if d[o] != 1: continue
        memseg = (d[o+13] << 16) | struct.unpack_from('<H', d, o+14)[0]
        ln, ls, le = struct.unpack_from('<III', d, o+16); flags = d[o+31]; c2 = struct.unpack_from('<I', d, o+32)[0]
        name = _s(d[o+48:o+76]); fname = _s(d[o+1:o+13])
        off = memseg * 16
        if flags & 4:
            raw = np.frombuffer(d[off:off+ln*2], dtype='<u2' if ffi == 2 else '<i2').astype(np.int32)
            if ffi == 2: raw = raw - 32768
            bits = 16
        else:
            raw = np.frombuffer(d[off:off+ln], dtype=np.uint8 if ffi == 2 else np.int8).astype(np.int16)
            if ffi == 2: raw = raw - 128
            bits = 8
        out.append(mk('%s [%s]' % (name, fname), raw, bits, c5=c2, fmt='S3M', loop=(ls, le-ls) if flags & 1 else None))
    return out

def _it_decomp(d, off, ln, bits16, it215):
    """IT 2.14/2.15 sample decompression (OpenMPT ITDecompression semantics)."""
    out = np.zeros(ln, dtype=np.int32); pos = 0; p = off
    blk, defw, fetchA, lowB, upB, m = ((0x4000, 17, 4, -8, 7, 0xFFFF) if bits16 else (0x8000, 9, 3, -4, 3, 0xFF))
    while pos < ln:
        clen = struct.unpack_from('<H', d, p)[0]; p += 2
        buf = d[p:p+clen]; p += clen
        bits = int.from_bytes(buf, 'little'); bp = 0
        def rd(n):
            nonlocal bp
            v = (bits >> bp) & ((1 << n) - 1); bp += n; return v
        cnt = min(blk, ln - pos); width = defw; m1 = m2 = 0; i = 0
        def cw(cur, w):
            w += 1
            if w >= cur: w += 1
            return w
        while i < cnt:
            if width > defw: break
            v = rd(width); top = 1 << (width - 1)
            if width <= 6:
                if v == top: width = cw(width, rd(fetchA)); continue
            elif width < defw:
                if top + lowB <= v <= top + upB: width = cw(width, v - (top + lowB)); continue
            else:
                if v & top: width = (v & ~top) + 1; continue
                v &= ~top; top = 0
            if top and (v & top): v -= top << 1
            m1 += v; m2 += m1
            val = (m2 if it215 else m1) & m
            if val > m // 2: val -= m + 1
            out[pos + i] = val; i += 1
        pos += cnt
    return out

def it(d):
    ordn, insn, smpn, patn = struct.unpack_from('<HHHH', d, 32)
    base = 0xC0 + ordn + insn * 4
    ptrs = struct.unpack_from('<%dI' % smpn, d, base)
    out = []
    for o in ptrs:
        fname = _s(d[o+4:o+16]); flg = d[o+18]; name = _s(d[o+20:o+46]); cvt = d[o+46]
        ln, ls, le, c5 = struct.unpack_from('<IIII', d, o+48)
        sp = struct.unpack_from('<I', d, o+72)[0]
        if not (flg & 1) or ln == 0: continue
        b16 = bool(flg & 2); comp = bool(flg & 8)
        if comp:
            a = _it_decomp(d, sp, ln, b16, bool(cvt & 4))
        elif b16:
            a = np.frombuffer(d[sp:sp+ln*2], dtype='<i2').astype(np.int32)
            if not (cvt & 1): a = a - 32768
        else:
            a = np.frombuffer(d[sp:sp+ln], dtype=np.int8 if cvt & 1 else np.uint8).astype(np.int16)
            if not (cvt & 1): a = a - 128
        if flg & 4: a = a[::2]  # stereo: left only
        out.append(mk('%s [%s]' % (name, fname), a, 16 if b16 else 8, c5=c5, fmt='IT', loop=(ls, le-ls) if flg & 16 else None, extra='comp' if comp else ''))
    return out
