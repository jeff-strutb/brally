#!/usr/bin/env python3
"""Boss.ico (off the disc) -> an .iconset directory for iconutil.

The disc's icon is one 32x32 16-colour image. Scaling it with a smoothing
filter turns it to mush at 512 px, so each size is a nearest-neighbour
upscale of the original pixels, written as PNG with the ICO's AND mask as
alpha. No imaging library: the ICO's BMP payload is decoded here.

Usage: mkicns.py Boss.ico out.iconset
"""
import os
import struct
import sys
import zlib


def ico_rgba(data):
    """Largest image of an ICO -> (w, h, rows of RGBA tuples), top row first."""
    n = struct.unpack_from('<H', data, 4)[0]
    best = None
    for i in range(n):
        w, h, _, _, _, bpp, size, off = struct.unpack_from('<BBBBHHII', data, 6 + 16 * i)
        w, h = w or 256, h or 256
        if best is None or w * h > best[0] * best[1]:
            best = (w, h, off, size)
    w, h, off, size = best
    blob = data[off:off + size]
    if blob[:8] == b'\x89PNG\r\n\x1a\n':
        sys.exit('mkicns: PNG-in-ICO is not handled (not what the disc carries)')
    hsz, _, _, _, bpp = struct.unpack_from('<IiiHH', blob, 0)
    ncol = struct.unpack_from('<I', blob, 32)[0] or (1 << bpp if bpp <= 8 else 0)
    pal = [blob[hsz + 4 * i:hsz + 4 * i + 3][::-1] for i in range(ncol)]   # BGRX -> RGB
    px = hsz + 4 * ncol
    xstride = ((w * bpp + 31) // 32) * 4
    mstride = ((w + 31) // 32) * 4
    mask = px + xstride * h
    rows = []
    for y in range(h):
        src = h - 1 - y                                      # BMP rows are bottom-up
        row = []
        for x in range(w):
            if bpp <= 8:
                bit = x * bpp
                b = blob[px + src * xstride + bit // 8]
                idx = (b >> (8 - bpp - bit % 8)) & ((1 << bpp) - 1)
                r, g, bl = pal[idx]
            elif bpp == 24:
                bl, g, r = blob[px + src * xstride + 3 * x:px + src * xstride + 3 * x + 3]
            else:
                bl, g, r = blob[px + src * xstride + 4 * x:px + src * xstride + 4 * x + 3]
            transparent = blob[mask + src * mstride + x // 8] >> (7 - x % 8) & 1
            row.append((r, g, bl, 0 if transparent else 255))
        rows.append(row)
    return w, h, rows


def png(path, w, h, rows, scale):
    raw = bytearray()
    for row in rows:
        line = bytearray(b'\0')
        for p in row:
            line += bytes(p) * scale
        raw += bytes(line) * scale
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, h * scale, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    w, h, rows = ico_rgba(open(sys.argv[1], 'rb').read())
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for pt in (16, 32, 128, 256, 512):
        for mul, suffix in ((1, ''), (2, '@2x')):
            size = pt * mul
            if size % w:
                continue
            png(os.path.join(out, 'icon_%dx%d%s.png' % (pt, pt, suffix)), w, h, rows, size // w)
    if w > 16:
        # 16 px is below the source: halve by sampling every other pixel
        half = [row[::w // 16] for row in rows[::h // 16]]
        png(os.path.join(out, 'icon_16x16.png'), 16, 16, half, 1)


if __name__ == '__main__':
    main()
