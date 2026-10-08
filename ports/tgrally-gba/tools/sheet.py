"""sheet.py OUT.png GBA.png:N64.png ... -- a comparison sheet: each row the
GBA shot (shrunk 2x from gbarun's 3x) beside the original's frame."""
import struct
import sys
import zlib

import png


def read(path):
    d = open(path, 'rb').read()
    i, idat, w = 8, b'', 0
    while i < len(d):
        n, t = struct.unpack_from('>I4s', d, i)
        c = d[i + 8:i + 8 + n]
        if t == b'IHDR':
            w, h, bd, ct = struct.unpack_from('>IIBB', c)
        elif t == b'IDAT':
            idat += c
        i += 12 + n
    bpp = {2: 3, 6: 4}[ct]
    raw, rows, prev = zlib.decompress(idat), [], bytearray(w * bpp)
    stride = w * bpp
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * bpp:x * bpp + 3]) for x in range(w)])
        prev = line
    return w, h, rows


def main():
    out, pairs = sys.argv[1], [p.split(':') for p in sys.argv[2:]]
    gap, W = 8, 360 + 8 + 320
    H = len(pairs) * (240 + gap) - gap
    img = [(32, 32, 32)] * (W * H)
    for r, (g, n) in enumerate(pairs):
        y0 = r * (240 + gap)
        gw, gh, grows = read(g)
        for y in range(240):
            for x in range(360):
                img[(y0 + y) * W + x] = grows[y * 2][x * 2]
        nw, nh, nrows = read(n)
        for y in range(min(240, nh)):
            for x in range(min(320, nw)):
                img[(y0 + y) * W + 368 + x] = nrows[y][x]
    png.write(out, W, H, img)
    print(out)


if __name__ == '__main__':
    main()
