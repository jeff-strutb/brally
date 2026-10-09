#!/usr/bin/env python3
"""shot2png.py RAW... -- the frames the game dumped (shotNNNNN_WxH.raw: the
GPU's 15-bit pixels, rcp_ps1.c TGR_SHOT_AT) as PNGs beside them."""
import re
import struct
import sys
import zlib


def png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    px = struct.unpack('<%dH' % (len(d) // 2), d)
    rgb = bytearray()
    for c in px:
        r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
        rgb += bytes((r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2))
    w = int(re.search(r'_(\d+)x\d+\.raw$', path).group(1))
    png(path[:-4] + '.png', w, len(px) // w, bytes(rgb))
    print(path[:-4] + '.png')
