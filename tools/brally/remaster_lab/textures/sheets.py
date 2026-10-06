#!/usr/bin/env python3
"""Contact sheets of the dumped textures: 10 x 8 cells of 128 px, index
order written to index.csv (sheet,row,col,hash,fmt,w,h,meanrgb)."""
import os, sys, subprocess, colorsys
D, O = sys.argv[1], sys.argv[2]
CELL, COLS, ROWS, PAD = 128, 10, 8, 6
def rd(p):
    d = open(p, 'rb').read()
    parts = d.split(b'\n', 3); w, h = map(int, parts[1].split()); return w, h, parts[3]
items = []
for f in sorted(os.listdir(D)):
    if not f.endswith('.ppm'): continue
    h, fmt, wh = f[:-4].split('_'); w, hh = map(int, wh.split('x'))
    W, H, px = rd(os.path.join(D, f))
    n = W * H; r = sum(px[0::3]) / n; g = sum(px[1::3]) / n; b = sum(px[2::3]) / n
    hue, lig, sat = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
    items.append((round(sat * 4), round(hue * 12) if sat > 0.12 else -1, lig, f, fmt, W, H, (r, g, b), px))
items.sort(key=lambda t: (t[0] > 0, t[1], t[2]))
idx = open(os.path.join(O, 'index.csv'), 'w')
per = COLS * ROWS
for s in range((len(items) + per - 1) // per):
    SW, SH = COLS * (CELL + PAD), ROWS * (CELL + PAD)
    buf = bytearray([40]) * (SW * SH * 3)
    for k, it in enumerate(items[s * per:(s + 1) * per]):
        r, c = divmod(k, COLS)
        _, _, _, f, fmt, W, H, mean, px = it
        sc = CELL / max(W, H)
        tw, th = max(1, int(W * sc)), max(1, int(H * sc))
        ox, oy = c * (CELL + PAD) + (CELL - tw) // 2, r * (CELL + PAD) + (CELL - th) // 2
        for y in range(th):
            sy = min(H - 1, int(y / sc)); row = (oy + y) * SW
            for x in range(tw):
                sx = min(W - 1, int(x / sc)); si = (sy * W + sx) * 3; di = (row + ox + x) * 3
                buf[di:di + 3] = px[si:si + 3]
        idx.write('%d,%d,%d,%s,%s,%d,%d,%d %d %d\n' % (s, r, c, f.split('_')[0], fmt, W, H, *map(int, mean)))
    p = os.path.join(O, 'sheet%02d.ppm' % s)
    open(p, 'wb').write(b'P6\n%d %d\n255\n' % (SW, SH) + bytes(buf))
    subprocess.run(['sips', '-s', 'format', 'png', p, '--out', p[:-4] + '.png'], capture_output=True)
    os.remove(p)
print(len(items), 'textures,', (len(items) + per - 1) // per, 'sheets')
